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

    // SDL's display name is "<connector> <diagonal>\"" (e.g. "eDP-1 13\""),
    // not the bare connector name KWin/KScreen identifies outputs by --
    // strip everything from the first space onward.
    auto contentScaleForDisplay = [](int displayIndex) -> float {
        std::string displayName = SDL_GetDisplayName(displayIndex);
        std::string connectorName = displayName.substr(0, displayName.find(' '));
        return queryKdeOutputScale(connectorName.c_str());
    };

    int result = runMultiDisplayStreamLoop(contentScaleForDisplay);

    SDL_Quit();
    return result;
}
