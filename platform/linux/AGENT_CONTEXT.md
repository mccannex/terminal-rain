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

**Integration approach — decided.** Confirmed this machine's Plasma session is
Wayland (`echo $XDG_SESSION_TYPE` → `wayland`), which ruled out the
XScreenSaver-compat option outright. Of the two remaining candidates (a
`swayidle`/logind-triggered fullscreen app, vs. a native `kscreenlocker` QML
plugin), neither is what actually shipped — a simpler native option surfaced
instead:

**KDE's own Power Management → Energy Saving → "Run custom script" idle
hook.** System Settings has a built-in "after a period of inactivity, run
script X" action (`powerdevil`), configured entirely through its own KCM —
no `swayidle`, no systemd timer units, no `kscreenlocker` QML/KPackage
plugin, no locking involved at all (this machine doesn't want session
locking, just the visual). It's genuinely native to Plasma, uses Plasma's own
idle detection, and needed zero new runtime dependencies beyond the script
itself.

Concretely:
- Launcher script: `~/.local/bin/glyph-rain-screensaver.sh` — a one-line
  `exec` wrapper around the built `glyph_rain_dev` binary (currently pointing
  at the `build/linux/` output directly; there's still no separate installed
  `glyph_rain` binary/target for Linux, see below).
- Configured via System Settings → Power Management → Energy Saving →
  "Other Settings" → "Run custom script" → path to the launcher script above,
  with an inactivity timer (tested at 1 min, should be turned up to a normal
  value for daily use).
- The existing mouse-motion debounce in `core/app_loop.cpp` (already written
  for the Windows `.scr` case) handles clean exit-on-input with no changes
  needed — powerdevil just launches the script once at the idle threshold and
  doesn't manage its lifecycle otherwise; the app exits itself on real input.

This satisfies the spirit of verification-gate item 2 below (native Settings
UI, not a manual script) even though it's a different KCM page than "Screen
Locking" — there's no meaningful separate "screensaver" concept left in
modern Plasma's Screen Locking UI to integrate with anyway (see the
`kscreenlocker` note further down for why that route was never pursued).

## Build setup (Fedora)

Dev packages needed (X11 *and* Wayland — this machine's session is confirmed
Wayland, but the project already hit "SDL2 configures successfully but
silently builds with every video driver off" once from missing dev headers,
so both sets stay installed rather than trimming to Wayland-only):

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
plain SDL app — same binary the WSL sanity build produces). **`glyph_rain_dev`
is also what's actually running as the live screensaver right now** — the
powerdevil launcher script points straight at `build/linux/glyph_rain_dev`.
There's still no dedicated Linux packaging target/binary the way
`src/win32/main.cpp` wraps `src/sdl_app/main.cpp` for Windows (gated
`if(WIN32)` in `CMakeLists.txt`) — that split is still a real loose end, not
yet done, worth doing before this is genuinely "shipped" rather than "working
off a dev build."

Sanity-check the dev binary runs and closes cleanly on input before starting
any integration work:

```bash
./build/linux/glyph_rain_dev
```

## Display scaling (done, superseded once — see history)

The glyph atlas renders at a fixed `8x12` raw pixels (`core/glyph_atlas.h`),
with no DPI/scale awareness by default. On this machine's multi-monitor setup
(4 panels of very different physical sizes, individually KDE-scaled from
100% up to ~206% to normalize perceived size), that made the screensaver
render at wildly inconsistent physical glyph sizes across monitors.

**First attempt (abandoned): querying KWin's per-output scale over D-Bus**
(`org.kde.KScreen` → `getConfig`), then manually multiplying it into a
`contentScale` that scales up the glyph cell size. This actually needed two
fixes before it produced correct numbers at all (the `/backend` object is
lazily instantiated — needs a `requestBackend` call first, or `getConfig`
fails outright; and SDL was auto-selecting the `x11` video driver, i.e.
XWayland, whose virtual screen has its own global supersampling scale
distinct from KWin's true per-output geometry, so even a correct D-Bus scale
value produced wrong on-screen sizing when multiplied against XWayland's
already-transformed window geometry). Once both of those were fixed the
scale values were provably correct (matched `kscreen-doctor -j` exactly) but
the on-screen result was *still* wrong for 3 of 4 monitors, confirming the
XWayland coordinate-space mismatch was the real, deeper problem, not
something fixable by getting the D-Bus number right.

**What actually shipped:** force native Wayland instead of letting SDL fall
back to `x11` (`setenv("SDL_VIDEODRIVER", "wayland", 1)` in
`src/sdl_app/main.cpp`, before `SDL_Init`), and delete the D-Bus scale query
entirely rather than fix it further. Under native Wayland with
`SDL_WINDOW_ALLOW_HIGHDPI`, a window's drawable (backbuffer) size already
differs from its logical size by exactly the compositor's real per-output
scale. `runMultiDisplayStreamLoop` (`core/app_loop.cpp`) draws into a
logical-sized off-screen texture and `SDL_RenderCopy`s it stretched to fill
the full drawable on present — that stretch *is* the correct per-monitor
scale, automatically, no manual multiplier needed. This is the same
mechanism the Windows build already relied on (it's always passed `nullptr`
for the `getContentScale` callback — see `src/win32/main.cpp`), so Linux and
Windows now share one HiDPI story instead of Linux carrying its own
KDE-specific one. `src/sdl_app/kde_display_scale.*` and its `sd-bus`/
`systemd-devel` link in `CMakeLists.txt` were deleted along with it — no
platform currently supplies a `getContentScale` callback, that parameter
exists purely as an extension point (see `core/app_loop.h`).

Confirmed live on this machine's real 4-monitor mixed-DPI layout: glyph size
now looks visually consistent across all 4 displays.

## Verification gate (adapt from the Windows one)

Mirroring `SCR_SHELL_PLAN.md`'s gate for the `.scr` shell — this item is done
when, on **this real machine** (not WSL):

1. ✅ The build installs/registers through whatever the chosen integration
   approach's normal mechanism is (not just manually running a binary) — the
   powerdevil "run script after inactivity" hook, configured through System
   Settings, not a manual launch.
2. ✅ (with a caveat) It's selectable through Plasma's normal screen-locking/
   screensaver UI for that approach — not literally the "Screen Locking" KCM
   (deliberate: no session locking wanted here), but the equally-native Power
   Management → Energy Saving KCM's own idle-script hook. Documented here per
   the original gate's own allowance for "option 2, no such UI entry."
3. ✅ It actually activates via the real idle-trigger mechanism — live-tested,
   confirmed working via powerdevil's own inactivity timer.
4. ✅ It exits cleanly on real input — live-tested with real mouse movement,
   process exits cleanly, no crash/error output. (The motion-event debounce
   in `core/app_loop.cpp`, ported from the Windows version, handles the
   window-manager-synthesizes-a-spurious-motion-event-on-creation case
   correctly — confirmed by testing.)

## Status

**Both single-display and multi-monitor Linux screensaver integration are
done and live-tested on this real machine** (4-monitor mixed-DPI Wayland/KDE
setup, not WSL). All four verification-gate items above are checked off.

`core/app_loop.*`'s `runMultiDisplayStreamLoop()` (landed from the Windows
side — see `FEATURE_PARITY_PLAN.md` Phase 2,
`PLATFORM_AND_DISTRIBUTION_PLAN.md` item 3) enumerates every SDL display and
opens one correctly-sized window per screen with a single unified
exit-on-input across all of them; `src/sdl_app/main.cpp` calls it. Confirmed
live on this machine:
- One fullscreen window per display, correctly sized. ✅
- Consistent glyph size across all 4 differently-scaled monitors — see
  "Display scaling" above for what that took (forcing native Wayland,
  dropping the D-Bus scale query entirely). ✅
- The KDE panel/task manager and system cursor stay hidden underneath every
  window — needed adding `SDL_WINDOW_ALWAYS_ON_TOP` alongside
  `FULLSCREEN_DESKTOP` in `runMultiDisplayStreamLoop`'s window creation
  (`core/app_loop.cpp`), since a panel set to "Always Visible" is designed by
  KDE to stay above normal fullscreen windows — only windows requesting the
  WM's "above" layer get to cover it. ✅
- Moving the mouse closes all four windows together cleanly, no crash. ✅

One loose end still open, not blocking the above: the missing dedicated
Linux packaging target (see "Build setup" above) — still running off
`glyph_rain_dev` directly via the powerdevil launcher script, no separate
installed `glyph_rain` binary for Linux yet, unlike Windows'
`src/win32/main.cpp` → `.scr` split.

Idea floated for later, not started: a user-configurable "master scale"
multiplier on top of the per-monitor auto-scaling, to shift overall glyph
size up/down to taste after normalization (came up because the laptop
panel's auto-scaled result, while technically consistent with the other
three monitors, ran a bit larger than preferred).
