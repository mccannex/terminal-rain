#include "app_loop.h"
#include "glyph_atlas.h"
#include "stream_field.h"
#include <utility>
#include <vector>

namespace
{
    // Matches the original's WM_TIMER interval. The other platforms' cadence
    // (SDL) is driven from here; macOS uses ScreenSaverView's own timer.
    constexpr Uint32 kFrameIntervalMs = 50;

    // A spurious mouse-motion event is commonly synthesized by the window
    // manager right when a window is created/focused, so require more than a
    // couple of motion events before treating it as real user input -- same
    // debounce the original Win32 version used.
    constexpr int kMotionEventThreshold = 2;

    SDL_Renderer* createRenderer(SDL_Window* window)
    {
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return renderer;
    }

    // Drains SDL's (process-wide) event queue and reports whether the loop
    // should stop: on SDL_QUIT, or real mouse motion past the debounce.
    // Preview windows ignore motion -- their lifetime is owned by the host
    // dialog, and the cursor merely passing over a small thumbnail shouldn't
    // dismiss it.
    bool shouldStop(bool isPreview, int& motionCount)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) return true;
            if (!isPreview && event.type == SDL_MOUSEMOTION)
            {
                if (++motionCount > kMotionEventThreshold) return true;
            }
        }
        return false;
    }

    // Sleeps out the remainder of a frame's target interval, accounting for
    // how long the frame's own work took (a bare SDL_Delay(50) would make the
    // real period 50ms + work, drifting slower under load).
    void paceFrame(Uint32 frameStartMs)
    {
        Uint32 elapsed = SDL_GetTicks() - frameStartMs;
        if (elapsed < kFrameIntervalMs) SDL_Delay(kFrameIntervalMs - elapsed);
    }

    // Owns everything one display's simulation needs, and tears it down in
    // destruction order (field, then atlas/renderer/window). Move-only, so a
    // half-built instance whose scope exits early (a mid-setup `continue`)
    // cleans up its partial resources automatically.
    struct DisplayInstance
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* atlas = nullptr;
        StreamField* field = nullptr;

        DisplayInstance() = default;
        DisplayInstance(const DisplayInstance&) = delete;
        DisplayInstance& operator=(const DisplayInstance&) = delete;
        DisplayInstance(DisplayInstance&& other) noexcept { *this = std::move(other); }
        DisplayInstance& operator=(DisplayInstance&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                window = other.window;
                renderer = other.renderer;
                atlas = other.atlas;
                field = other.field;
                other.window = nullptr;
                other.renderer = nullptr;
                other.atlas = nullptr;
                other.field = nullptr;
            }
            return *this;
        }
        ~DisplayInstance() { reset(); }

        void reset()
        {
            delete field;
            field = nullptr;
            if (atlas) { SDL_DestroyTexture(atlas); atlas = nullptr; }
            if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
            if (window) { SDL_DestroyWindow(window); window = nullptr; }
        }
    };
}

int runStreamLoop(SDL_Window* window, bool isPreview, float contentScale)
{
    SDL_Renderer* renderer = createRenderer(window);
    if (!renderer) return 1;

    SDL_Texture* atlas = loadGlyphAtlas(renderer);
    if (!atlas)
    {
        SDL_DestroyRenderer(renderer);
        return 1;
    }

    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    StreamField field(renderer, atlas, windowWidth, windowHeight, contentScale);

    // Preview mode is a small embedded thumbnail in someone else's dialog,
    // not an exclusive fullscreen surface -- leave the system cursor alone
    // there. SDL_ShowCursor is a process-global setting, not per-window.
    if (!isPreview) SDL_ShowCursor(SDL_DISABLE);

    int motionCount = 0;
    while (!shouldStop(isPreview, motionCount))
    {
        Uint32 frameStart = SDL_GetTicks();

        field.tick();
        SDL_SetRenderTarget(renderer, nullptr);
        SDL_RenderCopy(renderer, field.targetTexture(), nullptr, nullptr);
        SDL_RenderPresent(renderer);

        paceFrame(frameStart);
    }

    if (!isPreview) SDL_ShowCursor(SDL_ENABLE);

    SDL_DestroyTexture(atlas);
    SDL_DestroyRenderer(renderer);
    return 0;
}

int runMultiDisplayStreamLoop(std::function<float(int)> getContentScale)
{
    int displayCount = SDL_GetNumVideoDisplays();
    if (displayCount < 1) displayCount = 1;

    std::vector<DisplayInstance> instances;
    instances.reserve(displayCount);

    for (int i = 0; i < displayCount; ++i)
    {
        DisplayInstance instance;

        // SDL2's standard idiom for "fullscreen on a specific display": an
        // undefined position scoped to that display index, plus the
        // FULLSCREEN_DESKTOP flag, which then sizes the window to that
        // display's actual desktop resolution automatically. ALWAYS_ON_TOP is
        // needed on top of FULLSCREEN_DESKTOP because KDE panels set to
        // "Always Visible" are designed to stay above normal fullscreen
        // windows -- only windows requesting the WM's "above" layer cover them.
        instance.window = SDL_CreateWindow(
            "Glyph Rain",
            SDL_WINDOWPOS_UNDEFINED_DISPLAY(i), SDL_WINDOWPOS_UNDEFINED_DISPLAY(i),
            1024, 768,
            SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI |
                SDL_WINDOW_ALWAYS_ON_TOP);
        if (!instance.window) continue;

        instance.renderer = createRenderer(instance.window);
        if (!instance.renderer) continue; // instance's dtor frees the window

        instance.atlas = loadGlyphAtlas(instance.renderer);
        if (!instance.atlas) continue;    // dtor frees renderer + window

        int windowWidth, windowHeight;
        SDL_GetWindowSize(instance.window, &windowWidth, &windowHeight);
        float contentScale = getContentScale ? getContentScale(i) : 1.0f;
        instance.field = new StreamField(instance.renderer, instance.atlas,
                                         windowWidth, windowHeight, contentScale);

        instances.push_back(std::move(instance));
    }

    if (instances.empty()) return 1;

    // This entry point is only ever the real fullscreen show (never the
    // Windows /p preview), so the system cursor is always hidden here.
    // SDL_ShowCursor is a process-global setting, not per-window.
    SDL_ShowCursor(SDL_DISABLE);

    // One shared debounce across every window: real mouse movement on any
    // display closes all of them together (SDL's event queue is already
    // process-wide, so a single poll covers every window).
    int motionCount = 0;
    while (!shouldStop(false, motionCount))
    {
        Uint32 frameStart = SDL_GetTicks();

        for (DisplayInstance& instance : instances)
        {
            instance.field->tick();
            SDL_SetRenderTarget(instance.renderer, nullptr);
            SDL_RenderCopy(instance.renderer, instance.field->targetTexture(), nullptr, nullptr);
            SDL_RenderPresent(instance.renderer);
        }

        paceFrame(frameStart);
    }

    SDL_ShowCursor(SDL_ENABLE);
    return 0; // instances' destructors tear down every window/renderer/atlas/field
}
