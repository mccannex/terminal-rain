#include "app_loop.h"
#include "glyph_atlas.h"
#include "stream_field.h"
#include "config.h"
#include <cstdlib>
#include <ctime>
#include <vector>

int runStreamLoop(SDL_Window* window, bool isPreview, float contentScale)
{
    static bool seeded = false;
    if (!seeded)
    {
        srand(static_cast<unsigned int>(time(nullptr)));
        seeded = true;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Texture* atlas = loadGlyphAtlas(renderer);
    if (!atlas)
    {
        SDL_DestroyRenderer(renderer);
        return 1;
    }

    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    StreamFieldConfig config = loadConfig("matrix.cfg");
    config.contentScale = contentScale;
    StreamField field(renderer, atlas, windowWidth, windowHeight, config);

    // A spurious mouse-motion event is commonly synthesized by the window
    // manager right when the window is created/focused. Require more than
    // a couple of motion events before treating it as real user input, same
    // debounce the original Win32 version used.
    int motionEventCount = 0;
    const int motionEventThreshold = 2;

    // Preview mode is a small embedded thumbnail in someone else's dialog,
    // not an exclusive fullscreen surface -- leave the system cursor alone
    // there. SDL_ShowCursor is a process-global setting, not per-window.
    if (!isPreview) SDL_ShowCursor(SDL_DISABLE);

    bool running = true;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) running = false;
            if (!isPreview && event.type == SDL_MOUSEMOTION)
            {
                motionEventCount++;
                if (motionEventCount > motionEventThreshold) running = false;
            }
        }

        field.tick();

        SDL_SetRenderTarget(renderer, nullptr);
        SDL_RenderCopy(renderer, field.targetTexture(), nullptr, nullptr);
        SDL_RenderPresent(renderer);

        SDL_Delay(50); // matches the original's WM_TIMER interval
    }

    if (!isPreview) SDL_ShowCursor(SDL_ENABLE);

    SDL_DestroyTexture(atlas);
    SDL_DestroyRenderer(renderer);
    return 0;
}

namespace
{
    struct DisplayInstance
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* atlas = nullptr;
        StreamField* field = nullptr;
    };
}

int runMultiDisplayStreamLoop(std::function<float(int)> getContentScale)
{
    static bool seeded = false;
    if (!seeded)
    {
        srand(static_cast<unsigned int>(time(nullptr)));
        seeded = true;
    }

    int displayCount = SDL_GetNumVideoDisplays();
    if (displayCount < 1) displayCount = 1;

    StreamFieldConfig baseConfig = loadConfig("matrix.cfg");

    std::vector<DisplayInstance> instances;
    instances.reserve(displayCount);

    for (int i = 0; i < displayCount; ++i)
    {
        // SDL2's standard idiom for "fullscreen on a specific display": an
        // undefined position scoped to that display index, plus the
        // FULLSCREEN_DESKTOP flag, which then sizes the window to that
        // display's actual desktop resolution automatically.
        // ALWAYS_ON_TOP is needed on top of FULLSCREEN_DESKTOP because KDE
        // panels set to "Always Visible" are designed to stay above normal
        // fullscreen windows -- only windows requesting the WM's "above"
        // layer get to cover them.
        SDL_Window* window = SDL_CreateWindow(
            "Glyph Rain",
            SDL_WINDOWPOS_UNDEFINED_DISPLAY(i), SDL_WINDOWPOS_UNDEFINED_DISPLAY(i),
            1024, 768,
            SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI |
                SDL_WINDOW_ALWAYS_ON_TOP);
        if (!window) continue;

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer)
        {
            SDL_DestroyWindow(window);
            continue;
        }

        SDL_Texture* atlas = loadGlyphAtlas(renderer);
        if (!atlas)
        {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            continue;
        }

        int windowWidth, windowHeight;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);

        StreamFieldConfig config = baseConfig;
        config.contentScale = getContentScale ? getContentScale(i) : 1.0f;

        DisplayInstance instance;
        instance.window = window;
        instance.renderer = renderer;
        instance.atlas = atlas;
        instance.field = new StreamField(renderer, atlas, windowWidth, windowHeight, config);
        instances.push_back(instance);
    }

    if (instances.empty()) return 1;

    // Shared across every window: real mouse movement on any one display
    // closes all of them together, not just that one.
    int motionEventCount = 0;
    const int motionEventThreshold = 2;

    // This entry point is only ever the real fullscreen show (never the
    // Windows /p preview), so the system cursor is always hidden here.
    // SDL_ShowCursor is a process-global setting, not per-window.
    SDL_ShowCursor(SDL_DISABLE);

    bool running = true;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_MOUSEMOTION)
            {
                motionEventCount++;
                if (motionEventCount > motionEventThreshold) running = false;
            }
        }

        for (DisplayInstance& instance : instances)
        {
            instance.field->tick();
            SDL_SetRenderTarget(instance.renderer, nullptr);
            SDL_RenderCopy(instance.renderer, instance.field->targetTexture(), nullptr, nullptr);
            SDL_RenderPresent(instance.renderer);
        }

        SDL_Delay(50); // matches the original's WM_TIMER interval
    }

    SDL_ShowCursor(SDL_ENABLE);

    for (DisplayInstance& instance : instances)
    {
        delete instance.field;
        SDL_DestroyTexture(instance.atlas);
        SDL_DestroyRenderer(instance.renderer);
        SDL_DestroyWindow(instance.window);
    }

    return 0;
}
