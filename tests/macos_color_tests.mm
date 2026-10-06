#define SDL_MAIN_HANDLED
#include "platform/macos/metal_color_space.h"
#include <cstdio>
#include <cstdlib>

static void require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s (%s)\n", message, SDL_GetError()); std::exit(1); }
}

int main()
{
    @autoreleasepool {
        SDL_SetMainReady();
        SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
        require(SDL_Init(SDL_INIT_VIDEO) == 0, "video initialization");
        SDL_Window* window = SDL_CreateWindow("sRGB check", 0, 0, 32, 32, SDL_WINDOW_HIDDEN);
        require(window != nullptr, "hidden window");
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        require(renderer != nullptr, "Metal renderer");
        CAMetalLayer* layer = (__bridge CAMetalLayer*)SDL_RenderGetMetalLayer(renderer);
        require(layer != nil, "Metal presentation layer");
        const MTLPixelFormat format = layer.pixelFormat;
        const BOOL extendedRange = layer.wantsExtendedDynamicRangeContent;
        setMetalSRGBColorSpace(renderer);
        CGColorSpaceRef expected = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        require(layer.colorspace && CFEqual(layer.colorspace, expected), "explicit sRGB presentation");
        CGColorSpaceRelease(expected);
        require(layer.pixelFormat == format, "pixel format unchanged");
        require(layer.wantsExtendedDynamicRangeContent == extendedRange, "dynamic range unchanged");
        SDL_SetRenderDrawColor(renderer, 150, 255, 125, 255);
        require(SDL_RenderClear(renderer) == 0, "draw sRGB palette color");
        Uint8 pixels[32 * 32 * 4];
        require(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels, 32 * 4) == 0, "readback");
        require(pixels[0] == 150 && pixels[1] == 255 && pixels[2] == 125, "palette bytes unchanged");
        SDL_RenderPresent(renderer);
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        std::puts("Metal sRGB tag and unchanged palette/format/dynamic-range checks passed.");
    }
}
