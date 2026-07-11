#pragma once
#include <SDL.h>

// Runs the full stream-simulation loop against an already-created window:
// creates a renderer, loads the glyph atlas + config, drives a StreamField,
// and pumps events until closed. Does not create or destroy the window
// itself -- the caller owns that.
//
// isPreview: when true, disables the mouse-motion-to-close debounce (used
// for Windows' /p preview-embedded window, which shouldn't disappear just
// because the cursor happens to pass over its small thumbnail area -- its
// lifetime is governed by the parent process instead).
int runStreamLoop(SDL_Window* window, bool isPreview);
