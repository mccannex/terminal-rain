#import <Cocoa/Cocoa.h>
#import <ScreenSaver/ScreenSaver.h>
#include <cstdio>
#include <cstdlib>

static void require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main(int argc, char** argv)
{
    @autoreleasepool {
        require(argc == 2, "saver bundle path required");
        [NSApplication sharedApplication];
        NSBundle* bundle = [NSBundle bundleWithPath:[NSString stringWithUTF8String:argv[1]]];
        require([bundle load], "load built saver bundle");
        Class viewClass = bundle.principalClass;
        require([viewClass isSubclassOfClass:ScreenSaverView.class], "saver principal class");
        NSScreen* screen = NSScreen.mainScreen;
        require(screen != nil, "desktop display required");
        auto makeWindow = [&](BOOL preview) {
            NSWindow* window = [[NSWindow alloc] initWithContentRect:screen.frame
                styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered
                defer:NO screen:screen];
            window.releasedWhenClosed = NO;
            window.contentView = [[viewClass alloc] initWithFrame:NSMakeRect(
                0, 0, screen.frame.size.width, screen.frame.size.height) isPreview:preview];
            return window;
        };

        // ViewBridge presents Settings' embedded canvas at full display size
        // with isPreview=NO. A simultaneous fullscreen view must still render.
        NSWindow* settingsWindow = makeWindow(NO);
        NSWindow* fullscreenWindow = makeWindow(NO);
        NSWindow* previewWindow = makeWindow(YES);
        ScreenSaverView* settings = (ScreenSaverView*)settingsWindow.contentView;
        ScreenSaverView* fullscreen = (ScreenSaverView*)fullscreenWindow.contentView;
        ScreenSaverView* preview = (ScreenSaverView*)previewWindow.contentView;
        [settings startAnimation];
        [fullscreen startAnimation];
        [preview startAnimation];
        for (int frame = 0; frame < 3; ++frame) {
            [settings animateOneFrame];
            [fullscreen animateOneFrame];
            [preview animateOneFrame];
        }
        require(settings.isAnimating, "display-sized Settings canvas keeps animating");
        require(fullscreen.isAnimating, "fullscreen starts alongside display-sized Settings canvas");
        require(preview.isAnimating, "true preview coexists");
        [fullscreen stopAnimation];
        require(!fullscreen.isAnimating, "fullscreen stops");
        [settings animateOneFrame];
        require(settings.isAnimating, "stopping fullscreen preserves Settings canvas");
        [fullscreen startAnimation];
        [fullscreen animateOneFrame];
        require(fullscreen.isAnimating, "fullscreen restarts alongside Settings canvas");
        [settings stopAnimation];
        [fullscreen animateOneFrame];
        require(fullscreen.isAnimating, "stopping Settings preserves fullscreen");
        [fullscreen stopAnimation];
        [preview stopAnimation];
        [settingsWindow close];
        [fullscreenWindow close];
        [previewWindow close];
        std::puts("Full-size Settings/fullscreen coexistence, preview, and stop/restart passed.");
    }
}
