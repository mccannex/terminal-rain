#include "app_loop.h"
#include "glyph_atlas.h"
#include "stream_field.h"
#include "config.h"
#include <cstdlib>
#include <ctime>

int runStreamLoop(SDL_Window* window, bool isPreview)
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
    StreamField field(renderer, atlas, windowWidth, windowHeight, config);

    // A spurious mouse-motion event is commonly synthesized by the window
    // manager right when the window is created/focused. Require more than
    // a couple of motion events before treating it as real user input, same
    // debounce the original Win32 version used.
    int motionEventCount = 0;
    const int motionEventThreshold = 2;

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

    SDL_DestroyTexture(atlas);
    SDL_DestroyRenderer(renderer);
    return 0;
}
