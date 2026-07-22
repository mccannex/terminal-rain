#include <SDL.h>
#include <cstdlib>
#include "core/app_loop.h"

int main(int /*argc*/, char* /*argv*/[])
{
    // Force native Wayland: SDL's auto-detection picks x11 (XWayland) on this
    // machine even with a live Wayland session available, and XWayland
    // composites through its own global virtual-screen scale rather than
    // true per-output geometry -- multiplying that against KWin's real
    // per-output scale is what caused wrong (mismatched-per-monitor) glyph
    // sizing. Native Wayland doesn't have that problem: see the contentScale
    // comment on runMultiDisplayStreamLoop's call below.
    setenv("SDL_VIDEODRIVER", "wayland", 1);

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // No per-display content-scale query needed here (unlike an earlier
    // version of this file, which queried KWin's per-output scale over
    // D-Bus): under native Wayland with SDL_WINDOW_ALLOW_HIGHDPI, a window's
    // drawable (backbuffer) size already differs from its logical size by
    // exactly the compositor's per-output scale, and runMultiDisplayStreamLoop
    // draws into a logical-sized off-screen texture that SDL_RenderCopy then
    // stretches to fill the full drawable on present -- that stretch already
    // applies the right per-monitor scale automatically, so an explicit
    // contentScale multiplier on top would double it. Same reasoning the
    // Windows build already relies on (src/win32/main.cpp).
    int result = runMultiDisplayStreamLoop();

    SDL_Quit();
    return result;
}
