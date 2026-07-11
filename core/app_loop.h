#pragma once
#include <SDL.h>
#include <functional>

// Runs the full stream-simulation loop against an already-created window:
// creates a renderer, loads the glyph atlas + config, drives a StreamField,
// and pumps events until closed. Does not create or destroy the window
// itself -- the caller owns that.
//
// isPreview: when true, disables the mouse-motion-to-close debounce (used
// for Windows' /p preview-embedded window, which shouldn't disappear just
// because the cursor happens to pass over its small thumbnail area -- its
// lifetime is governed by the parent process instead).
//
// contentScale: multiplies the glyph cell size so content stays a
// perceptually consistent physical size across displays of differing
// pixel density -- the caller is responsible for figuring out the right
// value for the window's display (e.g. by querying the desktop
// environment's per-output scale factor); this loop just applies it.
int runStreamLoop(SDL_Window* window, bool isPreview, float contentScale = 1.0f);

// Runs one independent stream simulation per connected display: creates a
// fullscreen-on-that-display window/renderer/StreamField for every display
// SDL enumerates, ticks and presents them together each frame, and closes
// all of them together on the first real input -- a single shared debounce
// counter across every window's events, not one per window (SDL's event
// queue is already process-wide/shared across windows, so this falls out of
// pumping it once). Unlike runStreamLoop, this owns creation and destruction
// of every window itself.
//
// getContentScale: optional per-display scale lookup, called once per
// display with SDL's display index; return 1.0 for no scaling if omitted.
// Kept as a callback rather than a core/ dependency so platform-specific
// scale queries (e.g. Linux/KDE's D-Bus lookup in
// src/sdl_app/kde_display_scale.*) don't need to live in core/, which is
// shared with the Windows build.
int runMultiDisplayStreamLoop(std::function<float(int displayIndex)> getContentScale = nullptr);
