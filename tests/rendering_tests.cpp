#define SDL_MAIN_HANDLED
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include "core/app_loop.h"
#include "core/glyph_atlas.h"
#include "core/stream_field.h"

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAIL: %s (%s)\n", message, SDL_GetError());
            std::exit(1);
        }
    }

    struct SoftwareContext
    {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 256, 192, 32,
                                                            SDL_PIXELFORMAT_RGBA8888);
        SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
        SDL_Texture* atlas = renderer ? loadGlyphAtlas(renderer) : nullptr;

        SoftwareContext() { require(atlas != nullptr, "software setup"); }
        ~SoftwareContext()
        {
            SDL_DestroyTexture(atlas);
            SDL_DestroyRenderer(renderer);
            SDL_FreeSurface(surface);
        }
    };

    Uint32 SDLCALL pushTimedEvent(Uint32, void* userdata)
    {
        SDL_Event event{};
        event.type = *static_cast<Uint32*>(userdata);
        SDL_PushEvent(&event);
        return 0;
    }

#ifdef TERMINAL_RAIN_TEST_FAULTS
    bool failCreation = false;
    bool failBinding = false;
    bool ownershipViolation = false;
    std::map<SDL_Texture*, SDL_Renderer*> ownedTargets;
#endif
}

#ifdef TERMINAL_RAIN_TEST_FAULTS
extern "C"
{
    SDL_Texture* SDLCALL __real_SDL_CreateTexture(SDL_Renderer*, Uint32, int, int, int);
    int SDLCALL __real_SDL_SetRenderTarget(SDL_Renderer*, SDL_Texture*);
    void SDLCALL __real_SDL_DestroyTexture(SDL_Texture*);
    void SDLCALL __real_SDL_DestroyRenderer(SDL_Renderer*);

    SDL_Texture* SDLCALL __wrap_SDL_CreateTexture(SDL_Renderer* renderer, Uint32 format,
                                               int access, int width, int height)
    {
        if (failCreation && access == SDL_TEXTUREACCESS_TARGET)
        {
            SDL_SetError("injected texture allocation failure");
            return nullptr;
        }
        auto* texture = __real_SDL_CreateTexture(renderer, format, access, width, height);
        if (texture && access == SDL_TEXTUREACCESS_TARGET) ownedTargets[texture] = renderer;
        return texture;
    }

    int SDLCALL __wrap_SDL_SetRenderTarget(SDL_Renderer* renderer, SDL_Texture* target)
    {
        if (failBinding && target)
        {
            failBinding = false;
            return SDL_SetError("injected target binding failure");
        }
        return __real_SDL_SetRenderTarget(renderer, target);
    }

    void SDLCALL __wrap_SDL_DestroyTexture(SDL_Texture* texture)
    {
        ownedTargets.erase(texture);
        __real_SDL_DestroyTexture(texture);
    }

    void SDLCALL __wrap_SDL_DestroyRenderer(SDL_Renderer* renderer)
    {
        for (const auto& target : ownedTargets)
            if (target.second == renderer) ownershipViolation = true;
        __real_SDL_DestroyRenderer(renderer);
    }
}
#endif

int main(int argc, char* argv[])
{
    const bool native = argc == 2 && SDL_strcmp(argv[1], "--native") == 0;
    SDL_SetMainReady();
    if (!native) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0, "SDL init");
    require(SDL_IsScreenSaverEnabled() == SDL_TRUE, "idle inhibition disabled");
#ifdef _WIN32
    if (native)
    {
        // Query/restore this test thread's execution-state request, rather
        // than needing administrator access to inspect global power requests.
        EXECUTION_STATE previous = SetThreadExecutionState(ES_CONTINUOUS);
        require(previous != 0 && !(previous & ES_DISPLAY_REQUIRED), "no SDL display-required request");
        require(SetThreadExecutionState(previous) != 0, "restore execution state");
    }
#endif

    // Ordinary rendering and restoration of a caller's pre-existing target.
    {
        SoftwareContext context;
        auto* previous = SDL_CreateTexture(context.renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, 16, 16);
        require(previous != nullptr, "previous target allocation");
        require(SDL_SetRenderTarget(context.renderer, previous) == 0, "previous target bind");
        {
            StreamField field(context.renderer, context.atlas, 256, 192);
            require(field.valid(), "field initialized");
            require(SDL_GetRenderTarget(context.renderer) == previous, "constructor restores target");
            for (int frame = 0; frame < 30; ++frame) require(field.tick(), "render tick");
            require(SDL_GetRenderTarget(context.renderer) == previous, "tick restores target");
        }
        SDL_DestroyTexture(previous);
    }

    // Real invalid dimensions exercise the portable allocation-failure path.
    {
        SoftwareContext context;
        StreamField field(context.renderer, context.atlas, 0, 192);
        require(!field.valid() && !field.tick(), "invalid allocation stops field");
        require(SDL_GetRenderTarget(context.renderer) == nullptr, "failed setup leaves target alone");
    }

    // Both reset types invalidate every field, even after another consumer
    // removes the event from SDL's queue. New fields can start afterward.
    for (Uint32 type : { Uint32(SDL_RENDER_TARGETS_RESET), Uint32(SDL_RENDER_DEVICE_RESET) })
    {
        SoftwareContext first, second;
        {
            StreamField a(first.renderer, first.atlas, 256, 192);
            StreamField b(second.renderer, second.atlas, 256, 192);
            require(a.tick() && b.tick(), "both fields initially render");
            SDL_Event reset{};
            reset.type = type;
            require(SDL_PushEvent(&reset) == 1, "enqueue reset");
            SDL_Event drained;
            while (SDL_PollEvent(&drained)) {}
            require(!a.valid() && !b.valid(), "reset reaches sibling fields");
            require(!a.tick() && !b.tick(), "reset prevents further rendering");
        }
        StreamField replacement(first.renderer, first.atlas, 256, 192);
        require(replacement.tick(), "field can restart after teardown");
    }

#ifdef TERMINAL_RAIN_TEST_FAULTS
    {
        SoftwareContext context;
        failCreation = true;
        {
            StreamField field(context.renderer, context.atlas, 256, 192);
            require(!field.valid() && !field.tick(), "injected creation failure");
        }
        failCreation = false;

        auto* previous = SDL_CreateTexture(context.renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, 16, 16);
        require(SDL_SetRenderTarget(context.renderer, previous) == 0, "fault-test previous bind");
        failBinding = true;
        {
            StreamField field(context.renderer, context.atlas, 256, 192);
            require(!field.valid() && !field.tick(), "injected initial binding failure");
            require(SDL_GetRenderTarget(context.renderer) == previous, "failed init restores target");
        }
        {
            StreamField field(context.renderer, context.atlas, 256, 192);
            require(field.valid(), "setup before runtime failure");
            failBinding = true;
            require(!field.tick() && !field.valid(), "runtime binding failure stops field");
            require(SDL_GetRenderTarget(context.renderer) == previous, "runtime failure restores target");
        }
        SDL_DestroyTexture(previous);
    }
#endif

    // A texture from a different renderer causes a genuine SDL draw failure.
    {
        SoftwareContext first, second;
        StreamField field(first.renderer, second.atlas, 256, 192);
        require(field.valid(), "setup before draw failure");
        require(!field.tick() && !field.valid(), "draw failure stops field");
        require(SDL_GetRenderTarget(first.renderer) == nullptr, "draw failure restores target");
    }

    if (native)
    {
        auto* window = SDL_CreateWindow("GPU test", 0, 0, 256, 192, SDL_WINDOW_HIDDEN);
        require(window != nullptr, "native hidden window");
        auto* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        require(renderer != nullptr, "accelerated renderer");
        SDL_RendererInfo info{};
        require(SDL_GetRendererInfo(renderer, &info) == 0, "renderer diagnostics");
        std::printf("Native accelerated backend: %s\n", info.name);
        auto* atlas = loadGlyphAtlas(renderer);
        require(atlas != nullptr, "native atlas");
        {
            StreamField field(renderer, atlas, 256, 192);
            require(field.valid(), "native field setup");
            for (int frame = 0; frame < 60; ++frame) require(field.tick(), "native draw");
            Uint32 pixels[256 * 192];
            require(SDL_SetRenderTarget(renderer, field.targetTexture()) == 0, "native read target");
            require(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA8888,
                                         pixels, 256 * 4) == 0, "native readback");
            bool coloredPixel = false;
            for (Uint32 pixel : pixels) coloredPixel |= (pixel & 0xffffff00u) != 0;
            require(coloredPixel, "native rain produces visible pixels");
            require(SDL_SetRenderTarget(renderer, nullptr) == 0, "native restore target");
            SDL_Event reset{};
            reset.type = SDL_RENDER_DEVICE_RESET;
            require(SDL_PushEvent(&reset) == 1, "native synthetic reset");
            require(!field.tick(), "native reset dispatch stops drawing");
        }
        SDL_DestroyTexture(atlas);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Event drained;
        while (SDL_PollEvent(&drained)) {}
    }

    // Exercise the actual preview loop repeatedly, including teardown order.
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    for (int repeat = 0; repeat < 12; ++repeat)
    {
        auto* window = SDL_CreateWindow("test", 0, 0, 256, 192, SDL_WINDOW_HIDDEN);
        require(window != nullptr, "preview test window");
        SDL_Event quit{};
        quit.type = SDL_QUIT;
        require(SDL_PushEvent(&quit) == 1, "enqueue preview quit");
        require(runStreamLoop(window, true) == 0, "preview loop closes cleanly");
        SDL_DestroyWindow(window);
    }

    // Input must not dismiss a preview; a subsequent reset must terminate it.
    {
        auto* window = SDL_CreateWindow("test", 0, 0, 256, 192, SDL_WINDOW_HIDDEN);
        SDL_Event key{};
        key.type = SDL_KEYDOWN;
        require(SDL_PushEvent(&key) == 1, "enqueue preview key");
        Uint32 type = SDL_RENDER_DEVICE_RESET;
        auto timer = SDL_AddTimer(120, pushTimedEvent, &type);
        require(timer != 0, "reset timer");
        Uint32 start = SDL_GetTicks();
        require(runStreamLoop(window, true) == 1, "preview exits on reset");
        require(SDL_GetTicks() - start >= 100, "preview ignores keyboard dismissal");
        SDL_RemoveTimer(timer);
        SDL_DestroyWindow(window);
    }

#ifdef TERMINAL_RAIN_TEST_FAULTS
    require(!ownershipViolation && ownedTargets.empty(), "targets destroyed before renderers");
#endif
    SDL_Quit();
    std::puts("Rendering, reset, failure, and preview lifecycle checks passed.");
    return 0;
}
