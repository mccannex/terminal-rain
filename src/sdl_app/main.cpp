#include <SDL.h>
#include <string>
#include "../../core/app_loop.h"
#include "kde_display_scale.h"

int main(int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Glyph Rain",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // SDL's display name is "<connector> <diagonal>\"" (e.g. "eDP-1 13\""),
    // not the bare connector name KWin/KScreen identifies outputs by --
    // strip everything from the first space onward.
    std::string displayName = SDL_GetDisplayName(SDL_GetWindowDisplayIndex(window));
    std::string connectorName = displayName.substr(0, displayName.find(' '));

    float contentScale = queryKdeOutputScale(connectorName.c_str());
    int result = runStreamLoop(window, false, contentScale);

    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
