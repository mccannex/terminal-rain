#include "app_loop.h"
#include "glyph_atlas.h"
#include "stream_field.h"
#include <algorithm>
#include <utility>
#include <vector>

namespace
{
    // Matches the original's WM_TIMER interval. The other platforms' cadence
    // (SDL) is driven from here; macOS uses ScreenSaverView's own timer.
    constexpr Uint32 kFrameIntervalMs = 50;

    // A thumbnail needs a miniature of the rain, including its glyphs and
    // trail lengths, rather than native-size cells on a tiny simulation.
    constexpr int kPreviewCanvasWidth = 320;
    constexpr int kPreviewWarmupTicks = 600;

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
    // should stop: on SDL_QUIT, explicit keyboard/mouse input, or real mouse
    // motion past the debounce. Preview windows ignore input -- their lifetime
    // is owned by the host dialog, and interacting with a small thumbnail
    // shouldn't dismiss it.
    bool shouldStop(bool isPreview, int& motionCount)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) return true;
            if (!isPreview)
            {
                if (event.type == SDL_KEYDOWN ||
                    event.type == SDL_MOUSEBUTTONDOWN ||
                    event.type == SDL_MOUSEWHEEL)
                {
                    return true;
                }
                if (event.type == SDL_MOUSEMOTION &&
                    ++motionCount > kMotionEventThreshold)
                {
                    return true;
                }
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
    // half-built instance whose scope exits early (a setup failure)
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

    int result = 0;
    // The field owns a renderer texture, so it must leave scope before the
    // renderer (which also destroys every texture still associated with it).
    {
        int windowWidth, windowHeight;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        const int canvasWidth = isPreview ? std::max(kPreviewCanvasWidth, windowWidth) : windowWidth;
        const int canvasHeight = isPreview
            ? std::max(1, static_cast<int>(static_cast<long long>(windowHeight) * canvasWidth /
                                          std::max(1, windowWidth)))
            : windowHeight;
        StreamField field(renderer, atlas, canvasWidth, canvasHeight, contentScale, isPreview);

        // Preview mode is embedded in someone else's dialog; leave the
        // process-global system cursor alone there.
        if (!field.valid()) result = 1;
        if (isPreview && result == 0 &&
            SDL_SetTextureScaleMode(field.targetTexture(), SDL_ScaleModeLinear) != 0)
        {
            SDL_Log("Setting preview texture scale mode failed: %s", SDL_GetError());
            result = 1;
        }
        if (!isPreview && result == 0) SDL_ShowCursor(SDL_DISABLE);

        int motionCount = 0;
        bool stopped = false;
        if (isPreview)
        {
            // Prime only the thumbnail, without frame delays. Keep pumping
            // close events so dismissing the dialog during startup is safe.
            for (int i = 0; i < kPreviewWarmupTicks && result == 0; ++i)
            {
                if (i % 50 == 0 && shouldStop(true, motionCount))
                {
                    stopped = true;
                    break;
                }
                if (!field.tick()) result = 1;
            }
        }
        while (result == 0 && !stopped && !shouldStop(isPreview, motionCount))
        {
            Uint32 frameStart = SDL_GetTicks();

            if (!field.tick())
            {
                result = 1;
                break;
            }
            if (SDL_SetRenderTarget(renderer, nullptr) != 0 ||
                SDL_RenderCopy(renderer, field.targetTexture(), nullptr, nullptr) != 0)
            {
                SDL_Log("Presenting stream field failed: %s", SDL_GetError());
                result = 1;
                break;
            }
            SDL_RenderPresent(renderer);

            paceFrame(frameStart);
        }

        if (!isPreview) SDL_ShowCursor(SDL_ENABLE);
    }

    SDL_DestroyTexture(atlas);
    SDL_DestroyRenderer(renderer);
    return result;
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
            "Terminal Rain",
            SDL_WINDOWPOS_UNDEFINED_DISPLAY(i), SDL_WINDOWPOS_UNDEFINED_DISPLAY(i),
            1024, 768,
            SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI |
                SDL_WINDOW_ALWAYS_ON_TOP);
        if (!instance.window)
        {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            return 1;
        }

        instance.renderer = createRenderer(instance.window);
        if (!instance.renderer) return 1; // instance's dtor frees the window

        instance.atlas = loadGlyphAtlas(instance.renderer);
        if (!instance.atlas) return 1;    // dtor frees renderer + window

        int windowWidth, windowHeight;
        SDL_GetWindowSize(instance.window, &windowWidth, &windowHeight);
        float contentScale = getContentScale ? getContentScale(i) : 1.0f;
        instance.field = new StreamField(instance.renderer, instance.atlas,
                                         windowWidth, windowHeight, contentScale);
        if (!instance.field->valid()) return 1;

        instances.push_back(std::move(instance));
    }

    if (instances.empty()) return 1;

    // This entry point is only ever the real fullscreen show (never the
    // Windows /p preview), so the system cursor is always hidden here.
    // SDL_ShowCursor is a process-global setting, not per-window.
    SDL_ShowCursor(SDL_DISABLE);

    // Keyboard input, mouse clicks, scrolling or debounced mouse movement on
    // any display closes all windows together. SDL's event queue is shared,
    // so one poll and motion counter cover every window.
    int motionCount = 0;
    int result = 0;
    while (result == 0 && !shouldStop(false, motionCount))
    {
        Uint32 frameStart = SDL_GetTicks();

        for (DisplayInstance& instance : instances)
        {
            if (!instance.field->tick())
            {
                result = 1;
                break;
            }
            if (SDL_SetRenderTarget(instance.renderer, nullptr) != 0 ||
                SDL_RenderCopy(instance.renderer, instance.field->targetTexture(), nullptr, nullptr) != 0)
            {
                SDL_Log("Presenting stream field failed: %s", SDL_GetError());
                result = 1;
                break;
            }
            SDL_RenderPresent(instance.renderer);
        }

        if (result == 0) paceFrame(frameStart);
    }

    SDL_ShowCursor(SDL_ENABLE);
    return result; // instances' destructors tear down every window/renderer/atlas/field
}
