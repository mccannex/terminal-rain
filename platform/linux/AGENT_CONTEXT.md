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
  key presses, mouse clicks, scrolling or debounced mouse movement through
  `core/app_loop.cpp` (shared with Windows). No lifecycle management needed.

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
and exit cleanly on key presses, mouse clicks, scrolling and mouse movement,
tested in separate launches).

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
movement, key presses, mouse clicks, and scrolling. The local checkout was
resynced after the 2026-10-05 history rewrite, rebuilt under the new
`terminal-rain` binary name, and the powerdevil wrapper now points at it.

Open, non-blocking:

- The 2026-10-06 Windows/shared-core pass enabled the allow-screensaver hint
  here and added clean shutdown on drawing failure or renderer reset, plus
  off-screen culling. Linux CI now selects Release explicitly and runs the
  portable rendering checks. On 2026-10-06, the updated Release build and both
  opt-in regression tests passed on this Fedora machine. The real KDE launcher
  stayed active across the four-output Wayland layout (three 100% displays and
  the 125% laptop panel) and remained running through a forced five-second
  DPMS off/on cycle before being stopped manually. A person-observed follow-up
  confirmed correct four-display coverage and mixed-DPI rendering, plus clean
  dismissal by keyboard, deliberate mouse movement, click, scroll, and the
  first click on a non-primary monitor. With KDE's AC display-off timeout
  temporarily shortened from 30 minutes to one minute, all displays powered
  off while the saver was running; deliberate mouse movement woke the displays,
  dismissed the saver, and returned to a healthy desktop. Automatic and repeat
  idle activation were not directly re-observed; those KDE-managed launch paths
  were accepted based on the successful configured launcher checks. The older
  live verification above predates these changes.

- The X11 fallback has never run on a real X11 session.
- No packaging (no .rpm/.deb or install script); the powerdevil script
  points at the built `terminal-rain` binary directly.
- Idea only, not started: a user-configurable "master scale" multiplier on
  top of the per-monitor auto-scaling (the laptop panel's normalized size
  runs slightly larger than preferred).
