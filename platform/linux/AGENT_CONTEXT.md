# Linux Agent Context

For any coding agent working on this repo from the Fedora/KDE Plasma
machine. Tracked in git so it travels between machines and sessions; update
it with real status before you're done. The app is essentially finished, so
this file is a reference for maintenance work, not a running build log.

## Project, in brief

**Terminal Rain**: a cross-platform rewrite of a classic Win32/GDI "Matrix
digital rain" screensaver (originally by Louai Munajim, CC BY 3.0) on top of
SDL2, with a platform-agnostic simulation core (`core/`) shared across all
targets. SDL2 is vendored via CMake `FetchContent` (pinned
`release-2.30.9`) and statically linked, so binaries are self-contained. See
the root `README.md` for credits and `platform/macos/AGENT_CONTEXT.md` for
the macOS integration.

The Linux target is `platform/linux/main.cpp`, CMake target `terminal_rain`
(only defined on non-Apple Unix), output binary `terminal-rain`. CI builds it
on Ubuntu and attaches it to each release as the `Terminal Rain-linux`
artifact.

## Integration: done, live on this machine

This machine's Plasma session is **Wayland**. No lock-screen/kscreenlocker
integration (deliberate: no session locking wanted). Instead:

- **KDE Power Management idle hook**: System Settings → Power Management →
  Energy Saving → "Run custom script" on inactivity, pointing at
  `~/.local/bin/terminal-rain-screensaver.sh`, a one-line `exec` wrapper
  around `build/linux/terminal-rain`.
- powerdevil launches it once at the idle threshold; the app exits itself on
  mouse movement via the motion-event debounce in `core/app_loop.cpp`
  (shared with Windows). Key presses and clicks don't dismiss it yet. No
  lifecycle management needed.

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
driver off. Sanity-check with `./build/linux/terminal-rain` (should render
and exit cleanly on mouse movement).

## HiDPI / multi-monitor: done, don't regress

Verified live on this machine's real 4-monitor mixed-DPI (100% to 206%)
Wayland layout:

- **Native Wayland is preferred.** `platform/linux/main.cpp` calls
  `setenv("SDL_VIDEODRIVER", "wayland", 0)` before `SDL_Init`, but only when
  `WAYLAND_DISPLAY` is set; plain X11 sessions keep SDL's auto-detection, and
  an explicit `SDL_VIDEODRIVER` from the caller wins. Without it, SDL picks
  `x11`/XWayland even in a Wayland session, and XWayland's virtual screen
  has its own global supersampling scale, so even *correct* per-output
  scale values render wrong in its coordinate space. A KWin/KScreen D-Bus
  scale query was built, debugged, proven numerically correct, and still
  wrong on-screen for that reason; it was deleted. **Don't reintroduce
  compositor scale queries.** Under native Wayland with
  `SDL_WINDOW_ALLOW_HIGHDPI`, the logical-to-drawable stretch in
  `runMultiDisplayStreamLoop` (`core/app_loop.cpp`) *is* the correct
  per-monitor scale, automatically, same as Windows. No platform supplies a
  `getContentScale` callback; that parameter is purely an extension point.
- `SDL_WINDOW_ALWAYS_ON_TOP` (alongside `FULLSCREEN_DESKTOP`) is required
  so KDE panels set to "Always Visible" stay covered; only windows on the
  WM's "above" layer may cover such panels.
- One window per display with a single unified exit-on-input across all of
  them, from `runMultiDisplayStreamLoop`.

## Status

**Done and live-tested on the real machine** (not WSL): installs through
the powerdevil KCM, activates on its real idle timer, animates correctly
across all four displays with consistent glyph size, exits cleanly on mouse
movement.

Pending on this machine (from the 2026-10-05 changes, made on macOS):

- History was rewritten on GitHub to remove commit attribution lines. Run
  `git fetch && git reset --hard origin/main` before working here.
- The binary was renamed from `terminal_rain_dev` to `terminal-rain`.
  Rebuild, then point `~/.local/bin/terminal-rain-screensaver.sh` at
  `build/linux/terminal-rain`.
- The Wayland preference logic changed (see above). Re-verify on the
  4-monitor layout after rebuilding.

Open, non-blocking:

- Exit on key presses and clicks, not only mouse movement (shared with
  Windows, in `shouldStop` in `core/app_loop.cpp`).
- The X11 fallback has never run on a real X11 session.
- No packaging (no .rpm/.deb or install script); the powerdevil script
  points at the built `terminal-rain` binary directly.
- Idea only, not started: a user-configurable "master scale" multiplier on
  top of the per-monitor auto-scaling (the laptop panel's normalized size
  runs slightly larger than preferred).
