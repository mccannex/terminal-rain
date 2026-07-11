# Linux Agent Context

For whichever agent (Claude Code or otherwise) is working on this repo from the
Fedora/KDE Plasma machine. Unlike `planning/` (gitignored, local-only on the
Windows dev machine by choice), **this file is tracked in git** specifically so
it travels between machines — read it when you start, and update it with real
status/decisions before you're done, so the next session (on either machine)
has current context.

## Project, in brief

**Glyph Rain**: a cross-platform rewrite of a classic Win32/GDI "Matrix digital
rain" screensaver (originally by Louai Munajim, CC BY 3.0) on top of SDL2, with
a platform-agnostic simulation core (`core/`) shared across all targets. SDL2 is
vendored via CMake `FetchContent` (pinned `release-2.30.9`), statically linked,
so built binaries are self-contained — no companion `.so`/`.dll` or asset files
needed alongside them. See the root `README.md` for licensing/credits.

## What's already done (Windows)

The Windows `.scr` shell (`src/win32/main.cpp`) is fully implemented and
verified: hand-rolled `WinMain` + `/s /c /p /a` argument handling (reusing the
SDL init/event-loop shape from `src/sdl_app/main.cpp`), installs via
`InstallScreenSaver`, appears in the Windows screensaver picker with a live
preview thumbnail, activates on real idle-trigger, exits cleanly on input.
That's the reference implementation for "what a working platform integration
looks like" — not code to port directly (Windows' `.scr` convention has no
Linux equivalent), but the shape of the problem (idle detection → fullscreen
render → clean exit on input, config path, packaging) is the same.

## This machine's task

Implement Linux support — item 2 of the (local-only) platform/distribution
roadmap. This item is **not considered meaningfully underway until it's been
live-tested and iterated on real Fedora/KDE Plasma** — a WSL build on the
Windows machine only ever proved the CMake/SDL2 core *compiles* on Linux, not
that any of the actual integration work here is done.

**Open decision — make this first:** how Glyph Rain actually integrates as a
Plasma screensaver. Three candidates, not yet chosen between:

1. **XScreenSaver-compat binary** — implement the conventional xscreensaver
   command-line/window-embedding protocol; works under X11, and KDE has an
   xscreensaver-compatibility path. Won't work under a pure Wayland session.
2. **Idle-triggered fullscreen app via `swayidle`/systemd-logind hooks** — no
   "real" screensaver framework integration, just a fullscreen SDL window
   launched by an idle timer and killed on input. Simplest, most portable
   (works regardless of X11/Wayland), but isn't a "real" Plasma screensaver
   the user can pick from System Settings' screen-locking UI.
3. **Native KDE `kscreenlocker` QML plugin** — the "real" integration, shows
   up in Plasma's own Screen Locking settings like a first-class screensaver.
   Most work, most KDE-version-coupling risk, best end-user experience.

Check what Plasma/session type (X11 vs Wayland) this machine actually runs
before deciding — that alone may rule out option 1. Document the decision and
reasoning here once made, the same way `SCR_SHELL_PLAN.md` documented the
Windows `scrnsave.c`-vs-hand-rolled decision.

**Update:** the user believes this machine's Plasma session is Wayland
(unconfirmed by direct inspection yet — worth double-checking with
`echo $XDG_SESSION_TYPE` on first login there). If so, option 1
(XScreenSaver-compat) is ruled out, narrowing the real choice to option 2
(`swayidle`/logind, simpler, no native picker entry) vs. option 3
(`kscreenlocker` QML plugin, real Plasma integration, more work).

## Build setup (Fedora)

Dev packages needed (X11 *and* Wayland, since this machine's session type
isn't confirmed yet, and the project already hit "SDL2 configures successfully
but silently builds with every video driver off" once from missing dev
headers — don't repeat that):

```bash
sudo dnf install -y gcc-c++ cmake make git
sudo dnf install -y libX11-devel libXext-devel libXrandr-devel libXcursor-devel \
    libXi-devel libXfixes-devel libXScrnSaver-devel libxkbcommon-devel
sudo dnf install -y wayland-devel wayland-protocols-devel mesa-libEGL-devel \
    mesa-libGL-devel libdrm-devel
```

Build (SDL2 is fetched automatically via CMake `FetchContent` — no system SDL2
package needed or wanted):

```bash
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j$(nproc)
```

This currently builds `glyph_rain_core` (static lib) and `glyph_rain_dev` (the
plain SDL app — same binary the WSL sanity build produces). There is no
Linux-specific packaging target yet (the `glyph_rain` Windows `.scr` target in
`CMakeLists.txt` is gated `if(WIN32)`) — that gets added once the integration
approach above is chosen, mirroring how `src/win32/main.cpp` wraps
`src/sdl_app/main.cpp`'s core loop for the Windows-specific shell.

Sanity-check the dev binary runs and closes cleanly on input before starting
any integration work:

```bash
./build/linux/glyph_rain_dev
```

## Verification gate (adapt from the Windows one)

Mirroring `SCR_SHELL_PLAN.md`'s gate for the `.scr` shell — this item is done
when, on **this real machine** (not WSL):

1. The build installs/registers through whatever the chosen integration
   approach's normal mechanism is (not just manually running a binary).
2. It's selectable through Plasma's normal screen-locking/screensaver UI for
   that approach (or documented why not, if option 2 was chosen and there's
   deliberately no such UI entry).
3. It actually activates via the real idle-trigger mechanism, not just a
   manual launch.
4. It exits cleanly on real input (mirror the mouse-motion debounce note in
   `core/app_loop.cpp` / the Windows self-contained-binary notes — window
   managers can synthesize a spurious motion event on window creation/focus).

## Status

Not yet started. Waiting on the integration-approach decision above.
