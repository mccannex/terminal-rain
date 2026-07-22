# Linux Agent Context

For whichever agent (Claude Code or otherwise) is working on this repo from
the Fedora/KDE Plasma machine. Tracked in git so it travels between
machines/sessions; update it with real status before you're done. The app is
essentially finished — this file is now a reference for maintenance work,
not a running build log.

## Project, in brief

**Glyph Rain**: a cross-platform rewrite of a classic Win32/GDI "Matrix
digital rain" screensaver (originally by Louai Munajim, CC BY 3.0) on top of
SDL2, with a platform-agnostic simulation core (`core/`) shared across all
targets. SDL2 is vendored via CMake `FetchContent` (pinned
`release-2.30.9`), statically linked, so binaries are self-contained. See
the root `README.md` for licensing/credits and
`platform/macos/AGENT_CONTEXT.md` for the macOS integration.

## Integration — done, live on this machine

This machine's Plasma session is **Wayland**. No lock-screen/kscreenlocker
integration (deliberate: no session locking wanted) — instead:

- **KDE Power Management idle hook**: System Settings → Power Management →
  Energy Saving → "Run custom script" on inactivity, pointing at
  `~/.local/bin/glyph-rain-screensaver.sh` — a one-line `exec` wrapper
  around `build/linux/glyph_rain_dev`.
- powerdevil just launches it once at the idle threshold; the app exits
  itself on real input via the motion-event debounce in
  `core/app_loop.cpp` (shared with Windows). No lifecycle management
  needed.

## Build (Fedora)

```bash
sudo dnf install -y gcc-c++ cmake make git
sudo dnf install -y libX11-devel libXext-devel libXrandr-devel libXcursor-devel \
    libXi-devel libXfixes-devel libXScrnSaver-devel libxkbcommon-devel
sudo dnf install -y wayland-devel wayland-protocols-devel mesa-libEGL-devel \
    mesa-libGL-devel libdrm-devel

cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j$(nproc)
```

Keep *both* the X11 and Wayland dev-header sets installed: with headers
missing, SDL2 configures successfully but silently builds with every video
driver off. Sanity-check with `./build/linux/glyph_rain_dev` (should render
and exit cleanly on mouse movement).

## HiDPI / multi-monitor — done, don't regress

Verified live on this machine's real 4-monitor mixed-DPI (100%–206%)
Wayland layout:

- **Native Wayland is forced** (`setenv("SDL_VIDEODRIVER", "wayland", 1)`
  in `src/sdl_app/main.cpp` before `SDL_Init`). Without it SDL picks
  `x11`/XWayland, whose virtual screen has its own global supersampling
  scale — even *correct* per-output scale values render wrong when applied
  in XWayland's coordinate space. A KWin/KScreen D-Bus scale query was
  built, debugged, proven numerically correct, and still wrong on-screen
  for that reason; it was deleted. **Don't reintroduce compositor scale
  queries** — under native Wayland with `SDL_WINDOW_ALLOW_HIGHDPI`, the
  logical-to-drawable stretch in `runMultiDisplayStreamLoop`
  (`core/app_loop.cpp`) *is* the correct per-monitor scale, automatically,
  same as Windows. No platform supplies a `getContentScale` callback; that
  parameter is purely an extension point.
- `SDL_WINDOW_ALWAYS_ON_TOP` (alongside `FULLSCREEN_DESKTOP`) is required
  so KDE panels set to "Always Visible" stay covered — only windows on the
  WM's "above" layer may cover such panels.
- One window per display with a single unified exit-on-input across all of
  them, from `runMultiDisplayStreamLoop`.

## Status

**Done and live-tested on the real machine** (not WSL): installs through
the powerdevil KCM, activates on its real idle timer, animates correctly
across all four displays with consistent glyph size, exits cleanly on
input.

Open, non-blocking:

- No dedicated Linux packaging target — the powerdevil script still points
  at the `glyph_rain_dev` dev binary directly, unlike Windows'
  `src/win32/main.cpp` → `.scr` split.
- Idea only, not started: a user-configurable "master scale" multiplier on
  top of the per-monitor auto-scaling (the laptop panel's normalized size
  runs slightly larger than preferred).
