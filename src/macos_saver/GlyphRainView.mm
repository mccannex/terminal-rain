#import "GlyphRainView.h"
#include <SDL.h>
#include <cstdlib>
#include <ctime>
#include "../../core/glyph_atlas.h"
#include "../../core/stream_field.h"
#include "../../core/config.h"

// Unlike the Windows/.scr and Linux/powerdevil shells, this view never pumps
// SDL's event queue and never decides when to exit -- legacyScreenSaver
// (the host process System Settings' screensaver picker launches) owns idle
// activation and tears the whole process down on real input itself.
// animateOneFrame/stopAnimation are the entire lifecycle contract here.
@implementation GlyphRainView
{
    SDL_Window* _window;
    SDL_Renderer* _renderer;
    SDL_Texture* _atlas;
    StreamField* _field;
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
    [self setUpIfNeeded];
}

- (void)stopAnimation
{
    [super stopAnimation];
    [self teardown];
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
    if (!_window) [self setUpIfNeeded];
    if (!_field) return;

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
