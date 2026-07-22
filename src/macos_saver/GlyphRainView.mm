#import "GlyphRainView.h"
#import <CoreGraphics/CoreGraphics.h>
#import <os/log.h>
#include <SDL.h>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include "../../core/glyph_atlas.h"
#include "../../core/stream_field.h"
#include "../../core/config.h"

// legacyScreenSaver (the host process System Settings' screensaver picker
// launches) is *supposed* to own idle-activation/teardown-on-input at the
// process level, so this view originally never pumped its own events or
// decided when to exit. Confirmed live that this isn't reliable: an
// activation's host process can become orphaned (reparented to launchd)
// mid-animation rather than after a clean stop, if its own parent dies
// before delivering -stopAnimation -- observed running 6+ hours at 100%+
// CPU and a 673MB GPU-backed footprint, completely undetected, because
// nothing was left to ever tell it to stop. See platform/macos/
// AGENT_CONTEXT.md's "orphaned host process" section for the full incident.
//
// Why input detection inside this process can never catch that: sampling a
// live orphaned instance showed the main thread parked in
// -[NSViewServiceApplication nextEventMatchingMask:...] under
// -[NSXPCListener resume] -- legacyScreenSaver is an app extension whose
// events arrive over a ViewBridge/XPC connection *from its host*, not
// directly from the window server. Once the host dies, that bridge is dead:
// -stopAnimation never arrives, and no input event can ever reach this
// process again by any route. Both an AppKit NSResponder-override approach
// and an SDL_PollEvent-drain approach were tried and failed identically --
// an orphaned instance kept animating at 15-25% CPU straight through
// sustained real mouse/keyboard input.
//
// So the defenses below deliberately avoid the (dead) event pipeline:
//
// 1. Global HID idle time, queried out-of-band via
//    CGEventSourceSecondsSinceLastEventType against the HID system state --
//    this reflects the user's physical input activity regardless of where
//    (or whether) the window server routes the events. If fresh input shows
//    up while we're still animating, a healthy host would have dismissed us
//    within about a second; still animating kZombieConfirmSeconds later
//    means the host is gone. Suspended while the screen is locked, because
//    the system may legitimately keep a saver animating behind the unlock
//    prompt while the user types their password -- exactly the input
//    pattern this check hunts for.
// 2. A watchdog timeout, for the residual case where the user never comes
//    back to the machine at all. Fires after kWatchdogTimeoutSeconds of
//    continuous animation with no legitimate stop.
//
// Both defenses call exit() on the whole host process, not just
// self-teardown. legacyScreenSaver can host one GlyphRainView per display on
// a multi-monitor setup, all sharing one process -- but that's one
// activation, one session, created together and meant to dismiss together.
// If this view has been orphaned/stuck, every sibling view shares the exact
// same dead parent, so there's no "healthy sibling" to protect by keeping
// the process alive. Self-teardown alone could even produce a worse bug: one
// monitor's screensaver dismissing while another keeps animating -- the same
// half-dismissed failure mode core/app_loop.cpp's shared multi-display
// debounce (runMultiDisplayStreamLoop) exists to avoid on Windows/Linux.
static const NSTimeInterval kWatchdogTimeoutSeconds = 4 * 60 * 60; // 4 hours

// Ignore input from the first moments of animation: a hot-corner or
// Settings-button activation begins with the user's hand still on the
// mouse, so input shortly after start is normal and must not count as
// "input the host failed to act on".
static const NSTimeInterval kStartupGraceSeconds = 5.0;

// How long we keep animating after fresh user input before concluding the
// host is dead. A healthy dismissal lands within ~a second of input (and on
// a macOS where the host does deliver -stopAnimation, the animation timer
// stops and this countdown never even gets evaluated), so 2s is already
// double the legitimate window. Verified live: the dismissing input's host
// engine was gone well under a second after the event.
static const NSTimeInterval kZombieConfirmSeconds = 2.0;

// Heartbeat log cadence in frames (~every 10s at the 20fps tick rate).
static const uint64_t kHeartbeatFrames = 200;

static os_log_t saverLog(void)
{
    static os_log_t log;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        log = os_log_create("com.mccannex.glyph-rain", "saver");
    });
    return log;
}

// See defense 1 in the file-level comment: while the lock UI is up, input
// with the saver still animating can be legitimate, so the zombie check
// must stand down. A real zombie still dies right after unlock, confirmed
// by ordinary desktop activity instead. A NULL session dictionary reads as
// "not locked" on purpose -- an orphan with a broken session connection
// should fail toward the check running, not toward animating forever.
static BOOL screenIsLocked(void)
{
    NSDictionary* session = CFBridgingRelease(CGSessionCopyCurrentDictionary());
    return [session[@"CGSSessionScreenIsLocked"] boolValue];
}

@implementation GlyphRainView
{
    SDL_Window* _window;
    SDL_Renderer* _renderer;
    SDL_Texture* _atlas;
    StreamField* _field;
    CFAbsoluteTime _animationStartTime;
    uint64_t _frameCount;
    BOOL _permanentlyStopped;
}

- (instancetype)initWithFrame:(NSRect)frame isPreview:(BOOL)isPreview
{
    self = [super initWithFrame:frame isPreview:isPreview];
    if (self)
    {
        // Matches the SDL_Delay(50) cadence runStreamLoop/
        // runMultiDisplayStreamLoop use on the other platforms.
        self.animationTimeInterval = 1.0 / 20.0;
    }
    return self;
}

- (void)dealloc
{
    [self teardown];
}

// SDL_CreateWindowFrom (below) needs self attached to a real NSWindow to
// resolve one via [nsview window] -- true once startAnimation runs (the host
// has installed us in its window by then), not necessarily yet at
// initWithFrame:isPreview: time. Deferring setup to here, rather than init,
// avoids depending on host ordering.
- (void)startAnimation
{
    [super startAnimation];

    _animationStartTime = CFAbsoluteTimeGetCurrent();
    _frameCount = 0;
    _permanentlyStopped = NO;

    os_log(saverLog(),
           "startAnimation: view=%p pid=%d ppid=%d preview=%d frame=%.0fx%.0f",
           self, getpid(), getppid(), self.isPreview,
           self.frame.size.width, self.frame.size.height);

    [self setUpIfNeeded];
}

- (void)stopAnimation
{
    os_log(saverLog(), "stopAnimation: view=%p pid=%d frames=%llu",
           self, getpid(), _frameCount);
    [super stopAnimation];
    [self teardown];
}

// Shared by both defenses in -animateOneFrame: tears this instance down,
// then ends the whole host process -- see the file-level comment above for
// why exit() is correct here rather than an overreach. _permanentlyStopped
// guards against calling this twice (e.g. a watchdog trip racing the zombie
// check) and against animateOneFrame's own setUpIfNeeded fallback undoing
// the teardown before exit() actually takes effect.
- (void)terminateScreenSaverProcessBecause:(const char*)reason
{
    if (_permanentlyStopped) return;
    _permanentlyStopped = YES;
    os_log(saverLog(),
           "terminating host process (%{public}s): view=%p pid=%d frames=%llu",
           reason, self, getpid(), _frameCount);
    [self stopAnimation];
    exit(0);
}

- (void)setUpIfNeeded
{
    if (_window) return;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        srand(static_cast<unsigned int>(time(nullptr)));
        // SDL_Init defaults to disabling the OS's idle/screensaver detection
        // (an IOPMAssertion named "using SDL_DisableScreenSaver") -- meant
        // to stop games from being interrupted by some *other* screensaver,
        // but self-defeating here: this process IS the screensaver, so that
        // default would block macOS from ever idle-triggering it in the
        // first place. Confirmed live: a single legacyScreenSaver instance
        // (even a brief one, e.g. for a thumbnail attempt) holding this
        // assertion silently prevented "Show screensaver after 1 minute"
        // from ever firing.
        SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
        SDL_Init(SDL_INIT_VIDEO);
    });

    _window = SDL_CreateWindowFrom((__bridge void*)self);
    if (!_window) return;

    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
    if (!_renderer) _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_SOFTWARE);
    if (!_renderer)
    {
        [self teardown];
        return;
    }

    _atlas = loadGlyphAtlas(_renderer);
    if (!_atlas)
    {
        [self teardown];
        return;
    }

    int width, height;
    SDL_GetWindowSize(_window, &width, &height);

    // No bundled matrix.cfg (same call as Windows' shipped glyph_rain target
    // -- see the CMakeLists.txt comment by its copy_if_missing step): this
    // just falls through to loadConfig's compiled-in defaults.
    StreamFieldConfig config = loadConfig("matrix.cfg");
    _field = new StreamField(_renderer, _atlas, width, height, config);
}

- (void)teardown
{
    delete _field;
    _field = nullptr;
    if (_atlas) { SDL_DestroyTexture(_atlas); _atlas = nullptr; }
    if (_renderer) { SDL_DestroyRenderer(_renderer); _renderer = nullptr; }
    if (_window) { SDL_DestroyWindow(_window); _window = nullptr; }
}

- (void)animateOneFrame
{
    if (_permanentlyStopped) return;

    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    _frameCount++;

    // Watchdog: see defense 2 in the file-level comment. Checked before any
    // setup/render work so a trip takes effect immediately, not after one
    // more frame.
    if (now - _animationStartTime > kWatchdogTimeoutSeconds)
    {
        [self terminateScreenSaverProcessBecause:"watchdog timeout"];
        return;
    }

    // Zombie check: see defense 1 in the file-level comment. Deliberately
    // retroactive -- "did input happen more than kZombieConfirmSeconds ago
    // that we're somehow still animating through?" -- rather than watching
    // for input as it arrives. The session's screen-locked flag can stay
    // set for several seconds *after* a dismissal, so a single blip of
    // dismissing input (during which this check must stand down, see
    // screenIsLocked) would be missed entirely by an edge-triggered
    // version; observed live. This form catches it on the first unlocked
    // frame instead, with no further input needed. self.isPreview mirrors
    // core/app_loop.cpp's isPreview handling: a Preview thumbnail animates
    // *while* the user mouses around Settings, so input-while-animating is
    // its normal operating condition, not a stuck host.
    CFTimeInterval hidIdle = CGEventSourceSecondsSinceLastEventType(
        kCGEventSourceStateHIDSystemState, kCGAnyInputEventType);
    if (!self.isPreview && !screenIsLocked())
    {
        CFAbsoluteTime lastInputTime = now - hidIdle;
        if (lastInputTime > _animationStartTime + kStartupGraceSeconds &&
            now - lastInputTime > kZombieConfirmSeconds)
        {
            os_log(saverLog(),
                   "user input %.2fs ago never dismissed us", hidIdle);
            [self terminateScreenSaverProcessBecause:
                      "still animating well after user input; host is gone"];
            return;
        }
    }

    if (_frameCount % kHeartbeatFrames == 1)
    {
        os_log(saverLog(),
               "heartbeat: view=%p frames=%llu hidIdle=%.2f locked=%d "
               "ppid=%d preview=%d",
               self, _frameCount, hidIdle,
               screenIsLocked(), getppid(), self.isPreview);
    }

    if (!_window) [self setUpIfNeeded];
    if (!_field) return;

    // Drain SDL's queue so it can't accumulate. No input decisions are made
    // here: as the file-level comment explains, events from a real
    // activation demonstrably never reach this process's SDL queue (they
    // die with the host's ViewBridge), so anything found here is local
    // churn, and presence detection belongs to the HID check above.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}

    _field->tick();
    SDL_SetRenderTarget(_renderer, nullptr);
    SDL_RenderCopy(_renderer, _field->targetTexture(), nullptr, nullptr);
    SDL_RenderPresent(_renderer);
}

// No settings are configurable yet, mirroring src/win32/main.cpp's
// runConfigure stub -- once that changes, build the sheet's UI in code
// rather than a XIB, since Interface Builder's compiler (ibtool) isn't
// available outside full Xcode. See platform/macos/AGENT_CONTEXT.md.
- (BOOL)hasConfigureSheet
{
    return NO;
}

- (NSWindow*)configureSheet
{
    return nil;
}

@end
