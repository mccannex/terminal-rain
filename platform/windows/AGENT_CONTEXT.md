# Windows Agent Context

Maintenance reference for the Windows target. See the root README for the
project overview and the Linux/macOS context files for their integrations.

## Build and install

Entry point: `platform/windows/main.cpp`; CMake target: `terminal_rain`.
CMake copies `terminal_rain.exe` to `Terminal Rain.scr`. SDL2 is fetched and
linked statically, and the font atlas is embedded.

With Visual Studio C++ Build Tools, from the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Release --target terminal_rain
& ".\build\Release\Terminal Rain.scr" /s
```

MinGW is also supported using the `MinGW Makefiles` generator and a separate
build directory. Its compiler runtime DLLs may be required alongside the
binary even though SDL2 is linked statically.

Right-click the `.scr` and choose **Install**. Windows derives the picker
name from the filename.

## Screensaver integration

| Argument | Behavior |
| --- | --- |
| `/s` | Fullscreen on all displays. |
| `/c`, or no argument | Show the no-configurable-settings message. |
| `/p <HWND>` | Embedded preview in the Screen Saver dialog. |
| `/a <HWND>` | Legacy password-change request; no-op. |

Parsing accepts uppercase/lowercase, `/` or `-`, and space/colon before the
handle. The Windows `.scr` association supplies `/S` on double-click.

Fullscreen uses `runMultiDisplayStreamLoop(nullptr)` in `core/app_loop.cpp`:
one window per display, shared input handling, and SDL's automatic drawable
scaling. Negative desktop coordinates are valid.

The preview creates its own SDL window and reparents its HWND with
`SetParent` and `WS_CHILD`. Do not adopt the foreign-process host HWND with
`SDL_CreateWindowFrom`; that approach deadlocked the dialog. Preview ignores
input and leaves the cursor alone; its lifetime belongs to the host.

The dialog thumbnail uses an aspect-matched virtual canvas at least 320 pixels
wide, scaled down with linear filtering. Its simulation warms up 600 ticks before
the first presentation so it starts populated. This keeps native glyph/trail
sizes from making a tiny preview mostly empty; fullscreen rendering is unchanged.
Preview cleanup/trail distances also scale with the canvas's glyph-row count:
otherwise streams linger up to 200 rows below a roughly 19-row preview, filling
its stream cap and producing long dark periods before replacements can spawn.
At 320x234, a deterministic diagnostic checked ten simulated minutes per seed
after the 600-tick warm-up (seeds 1, 7, 12345). Original trail distances produced
zero visible heads at times and runs of fewer than four heads lasting 4-4.45
seconds. Compact preview trails kept at least 32, 39, and 35 heads visible,
respectively. The full Windows Release build and both rendering suites passed.

## Dismissal and testing

Key presses, mouse clicks, scrolling and debounced mouse movement dismiss
all fullscreen windows together. Keep `SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH`
set to `"1"` in `runShow`: otherwise SDL consumes the click that focuses a
window, requiring a second click on an unfocused monitor.

Test the actual `.scr` launch as well as the dialog preview. Check full
coverage and first-click dismissal on each monitor, then test keyboard,
scrolling and movement in separate runs. Recheck real idle activation when
changing integration code.

The 2026-10-06 optimization pass fixed preview texture/renderer destruction
order, enabled SDL's allow-screensaver hint in both Windows modes, and added
failure/reset handling to the shared core. A reset ends the saver cleanly.
Off-screen glyphs and erase cells are culled individually with RNG consumption
preserved. Windows 10/MinGW Release tests passed using software and Direct3D 11
rendering, including repeated hidden preview loops, injected creation/binding
failures, reset notification across fields, and no thread display-required
request. The user approved the actual Settings preview. On the later Windows 10
follow-up, the real fullscreen `.scr` passed a physical display-off test with a
temporary 60-second AC timeout: every display powered off, and mouse movement
woke them and dismissed the saver to the desktops. The saver exited with code 0;
the helper restored the original AC timeout of Never. Physical keyboard, scroll,
and first-click dismissal across monitors and actual GPU device loss still need
separate checks. See `docs/optimization_validation.md` for results and reproduction.

Automatic idle activation was then verified with a temporary 30-second saver
timeout and 60-second display timeout. Windows launched the saver automatically;
all displays turned off and mouse input woke/dismissed normally. The helper
restored the original five-minute saver timeout and Never AC display timeout.

For a short interactive display-off check on AC power:

```powershell
& .\tools\windows\test-display-idle.ps1
```

The helper snapshots the current plan's AC display timeout, temporarily sets it
to 60 seconds, starts the built `.scr` fullscreen after a 10-second countdown,
and restores the timeout when the saver exits or the three-minute test limit
expires. Leave input alone until every display turns off, then wake with physical
input and check dismissal. It changes no screensaver settings, DC timeout, or
system-sleep timeout. This is a manually launched saver test; automatic idle
activation can be checked with `-AutomaticActivation`: it temporarily sets the
installed saver's activation timeout to 30 seconds, waits up to the configured
test duration, and restores both settings. Keep the display timeout longer than
30 seconds for that mode. Other physical input modes require separate runs. Restoration
runs in `finally`; terminating the PowerShell process itself can prevent it, so
keep the printed original value if manually interrupting the host.

Window-position utilities can relocate fullscreen windows and generate
synthetic motion. Exclude the screensaver from their restoration rules;
for PersistentWindows, use `-ignore_process "Terminal Rain.scr"`
([help](https://github.com/kangyu-california/PersistentWindows/blob/master/Help.md)).
