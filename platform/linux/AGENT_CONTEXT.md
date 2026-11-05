# Linux Agent Context

## v1.1.1 handoff

Read `docs/release_handoff.md` first for the current source, completed Windows/Mac validation, green cross-platform CI and the Linux continuation checklist and final Linux results. Native density validation is complete; prior power-management results were carried forward after reviewing the runtime diff. The branch is merged to main; tag v1.1.1 at `53b990c` passed all release workflow jobs, and its draft assets were verified. Publishing awaits final user approval. The local idle wrapper still uses the user-approved build below.

On 2026-11-05, checkout `d3be66d` (runtime `d5ad85d`) was rebuilt as Release in `build/linux` with tests enabled; all four expected suites passed. The idle wrapper points at this build. Saved AC delays remain 300 seconds for the script and 1,800 seconds for display-off; no settings were changed. Native Wayland/OpenGL rendering checks passed, and the user approved density, glyph sharpness, four-display/panel coverage and mouse dismissal. A visible duplicate exited zero without disrupting the owner; the owner then exited zero with no process left. KDE reports AC active. Actual idle/display-off/wake and suspend tests from October were carried forward after review: those paths and the 50-ms cadence are unchanged; only bounded stream arrivals changed. They were not repeated today. See the handoff for exact build, process evidence and remaining limits.

For any coding agent working on this repo from the Fedora/KDE Plasma machine. Tracked in git so it travels between machines and sessions; update it with real status before you're done. The app is essentially finished, so this file is a reference for maintenance work, not a running build log.

## Project, in brief

**Terminal Rain**: a cross-platform rewrite of a classic Win32/GDI "Matrix digital rain" screensaver (originally by Louai Munajim, CC BY 3.0) on top of SDL2, with a platform-agnostic simulation core (`core/`) shared across all targets. SDL2 is vendored via CMake `FetchContent` (pinned `release-2.30.9`) and statically linked, so binaries are self-contained. See the root `README.md` for credits and `platform/macos/AGENT_CONTEXT.md` for the macOS integration.

The Linux target is `platform/linux/main.cpp`, CMake target `terminal_rain` (only defined on non-Apple Unix), output binary `terminal-rain`. CI builds it on Ubuntu and attaches it to each release as the `Terminal Rain-linux` artifact.

## Integration: done, live on this machine

This machine's Plasma session is **Wayland**. No lock-screen/kscreenlocker integration (deliberate: no session locking wanted). Instead:

- **KDE Power Management idle hook**: System Settings → Power Management → Energy Saving → "Run custom script" on inactivity, pointing at `~/.local/bin/terminal-rain-screensaver.sh`, a one-line `exec` wrapper around `build/linux/terminal-rain`.
- powerdevil launches it once at the idle threshold; the app exits itself on key presses, mouse clicks, scrolling or debounced mouse movement through `core/app_loop.cpp` (shared with Windows). No lifecycle management needed.

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

Keep *both* the X11 and Wayland dev-header sets installed: with headers missing, SDL2 configures successfully but silently builds with every video driver off. Sanity-check with `./build/linux/terminal-rain` (should render and exit cleanly on key presses, mouse clicks, scrolling and mouse movement, tested in separate launches).

## HiDPI / multi-monitor: done, don't regress

Verified live on this machine's real 4-monitor mixed-DPI (100% to 206%) Wayland layout:

- **Native Wayland is preferred.** `platform/linux/main.cpp` calls `setenv("SDL_VIDEODRIVER", "wayland", 0)` before `SDL_Init`, but only when `WAYLAND_DISPLAY` is set; plain X11 sessions keep SDL's auto-detection, and an explicit `SDL_VIDEODRIVER` from the caller wins. Without it, SDL picks `x11`/XWayland even in a Wayland session, and XWayland's virtual screen has its own global supersampling scale, so even *correct* per-output scale values render wrong in its coordinate space. A KWin/KScreen D-Bus scale query was built, debugged, proven numerically correct, and still wrong on-screen for that reason; it was deleted. **Don't reintroduce compositor scale queries.** Under native Wayland with `SDL_WINDOW_ALLOW_HIGHDPI`, the logical-to-drawable stretch in `runMultiDisplayStreamLoop` (`core/app_loop.cpp`) *is* the correct per-monitor scale, automatically, same as Windows. No platform supplies a `getContentScale` callback; that parameter is purely an extension point.
- `SDL_WINDOW_ALWAYS_ON_TOP` (alongside `FULLSCREEN_DESKTOP`) is required so KDE panels set to "Always Visible" stay covered; only windows on the WM's "above" layer may cover such panels.
- One window per display with a single unified exit-on-input across all of them, from `runMultiDisplayStreamLoop`.

## Native validation baseline

The guarded build passed Fedora Release tests and real four-display Wayland coverage, duplicate rejection, dismissal/relaunch, repeated KDE idle activation, display-off/wake, and one system suspend/resume cycle on 2026-10-19. See `docs/fullscreen_ownership.md` for evidence and incident boundaries. Those checks predate the new shared density scheduler; revalidate it on this machine using `docs/release_handoff.md`.

Remaining limitations: X11 has not been tested in a real session; the earlier SDL display-lookup cleanup crash was not reproduced or fixed by duplicate protection. There is no RPM/DEB packaging; the powerdevil wrapper launches the built executable.

## Fullscreen ownership (2026-10-19)

Linux now acquires a nonblocking per-user fullscreen lease before SDL video initialization. Duplicate launches log and return success; lock errors log and return failure. The kernel releases ownership on normal exit or crash; never unlink the lock file while a saver may be running. The lock lives in XDG_RUNTIME_DIR, with a user-qualified /tmp fallback. This prevents accumulation but does not diagnose why KDE might launch overlapping copies.

Investigation: the prior binary allowed two simultaneous processes; deliberate mouse movement dismissed both normally. A saved October 15 crash occurred in SDL 2.30.9 window destruction after display lookup failed. Severe system swap use was recorded around that time, but no retained process census establishes screensaver accumulation as its cause. That display-cleanup crash remains a separate unresolved issue. Automated lease tests cover process exclusion, independent scopes, normal/crash release, and same-process display exclusion.

The separate Fedora Release build passed fullscreen_lease, fullscreen_launch, rendering, and zombie_confirmation on 2026-10-19. The Linux entry-point test rejected eight duplicate launches before video initialization, kept the owner alive, and reacquired after SIGKILL. No visible window was opened by these checks. See docs/fullscreen_ownership.md for reproduction and native-platform gaps.
