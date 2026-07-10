#include <SDL.h>
#include <cstdlib>
#include <ctime>
#include "../../core/glyph_atlas.h"
#include "../../core/stream_field.h"
#include "../../core/config.h"

int main(int argc, char* argv[])
{
    srand(static_cast<unsigned int>(time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Matrix",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);

    if (!window)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    SDL_Texture* atlas = loadGlyphAtlas(renderer, "cp437_vga_8x16.bmp");
    if (!atlas)
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
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
            if (event.type == SDL_MOUSEMOTION)
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
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
