# Windows Agent Context

## v1.1.1 validation

Windows, Mac and Linux density validation is complete within the recorded hardware limits. See `docs/releases/v1.1.1-validation.md` for source/build evidence, native observations and power-management checks carried forward after runtime review. v1.1.1 was published as the latest release on 2026-11-05 at final tag `5053664`; all final build jobs and native Mac installer checks passed.

Maintenance reference for the Windows target. See the root README for the project overview and the Linux/macOS context files for their integrations.

## Build and install

Entry point: `platform/windows/main.cpp`; CMake target: `terminal_rain`. CMake copies `terminal_rain.exe` to `Terminal Rain.scr`. SDL2 is fetched and linked statically, and the font atlas is embedded. The MSVC release uses the dynamic Microsoft C/C++ runtime and requires the [Microsoft Visual C++ v14 Redistributable (x64)](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/); statically linking SDL2 does not remove that requirement.

With Visual Studio C++ Build Tools, from the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Release --target terminal_rain
& ".\build\Release\Terminal Rain.scr" /s
```

MinGW is also supported using the `MinGW Makefiles` generator and a separate build directory. Its compiler runtime DLLs may be required alongside the binary even though SDL2 is linked statically.

Right-click the `.scr` and choose **Install**. Windows derives the picker name from the filename.

For argument-specific process tests, bypass the `.scr` shell association with `ProcessStartInfo.UseShellExecute = false`; that association forces `/S`. Use normal window visibility for a fullscreen visual test. Hidden startup can hide the first display's window while the other three remain visible. A direct normal-visibility launch resolved that diagnostic artifact on the test machine.

## Screensaver integration

| Argument | Behavior |
| --- | --- |
| `/s` | Fullscreen on all displays. |
| `/c`, or no argument | Show the no-configurable-settings message. |
| `/p <HWND>` | Embedded preview in the Screen Saver dialog. |
| `/a <HWND>` | Legacy password-change request; no-op. |

Parsing accepts uppercase/lowercase, `/` or `-`, and space/colon before the handle. The Windows `.scr` association supplies `/S` on double-click.

Fullscreen uses `runMultiDisplayStreamLoop(nullptr)` in `core/app_loop.cpp`: one window per display, shared input handling, and SDL's automatic drawable scaling. Negative desktop coordinates are valid.

The preview creates its own SDL window and reparents its HWND with `SetParent` and `WS_CHILD`. Do not adopt the foreign-process host HWND with `SDL_CreateWindowFrom`; that approach deadlocked the dialog. Preview ignores input and leaves the cursor alone; its lifetime belongs to the host.

The dialog thumbnail uses an aspect-matched virtual canvas at least 320 pixels wide, scaled down with linear filtering. Its simulation warms up 600 ticks before the first presentation so it starts populated. This keeps native glyph/trail sizes from making a tiny preview mostly empty; fullscreen rendering is unchanged. Preview cleanup/trail distances also scale with the canvas's glyph-row count: otherwise streams linger up to 200 rows below a roughly 19-row preview, filling its stream cap and producing long dark periods before replacements can spawn. The user approved the tuned thumbnail. Historical measurements and rendering checks are in `docs/optimization_validation.md`.

## Dismissal and testing

Key presses, mouse clicks, scrolling and debounced mouse movement dismiss all fullscreen windows together. Keep `SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH` set to `"1"` in `runShow`: otherwise SDL consumes the click that focuses a window, requiring a second click on an unfocused monitor.

Test the actual `.scr` launch as well as the dialog preview. Check full coverage and first-click dismissal on each monitor, then test keyboard, scrolling and movement in separate runs. Recheck real idle activation when changing integration code.

Current Windows release validation is complete for the density change: all four MinGW Release suites passed (including native Direct3D 11), and the user approved density, four-display coverage, and normal dismissal. The pre-density MSVC CI artifact also passed real preview, duplicate exclusion, and automatic idle / display-off / physical wake checks. Timeouts were restored and read back as 300 seconds for saver activation and 3,600 seconds for AC display-off.

The approved density build is installed locally under `build-installed-windows`, with its three MinGW runtime DLLs. It loads without the compiler PATH. Keep those DLLs beside the `.scr`; the public release still comes from MSVC CI. Details: `docs/stream_density.md`, `docs/fullscreen_ownership.md`.

Actual GPU resource loss and individually enumerated keyboard/scroll/first-click dismissal on every monitor remain coverage limits, not newly observed defects.

For a short interactive display-off check on AC power:

```powershell
& .\tools\windows\test-display-idle.ps1
```

The helper snapshots the current plan's AC display timeout, temporarily sets it to 60 seconds, starts the built `.scr` fullscreen after a 10-second countdown, and restores the timeout when the saver exits or the three-minute test limit expires. Leave input alone until every display turns off, then wake with physical input and check dismissal. It changes no screensaver settings, DC timeout, or system-sleep timeout. This is a manually launched saver test; automatic idle activation can be checked with `-AutomaticActivation`: it temporarily sets the installed saver's activation timeout to 30 seconds, waits up to the configured test duration, and restores both settings. Keep the display timeout longer than 30 seconds for that mode. Other physical input modes require separate runs. Restoration runs in `finally`; terminating the PowerShell process itself can prevent it, so keep the printed original value if manually interrupting the host.

Window-position utilities can relocate fullscreen windows and generate synthetic motion. Exclude the screensaver from their restoration rules; for PersistentWindows, use `-ignore_process "Terminal Rain.scr"` ([help](https://github.com/kangyu-california/PersistentWindows/blob/master/Help.md)).

## Fullscreen ownership (2026-10-19)

Only /s takes a nonblocking session-local named mutex, before SDL initialization. A second fullscreen launch returns success without opening windows; configure and preview modes bypass the mutex. Normal exit releases it; abandoned ownership is accepted after a crash. The session-wide scope covers the saver and ordinary desktop within the same login. On 2026-11-04, a fresh Windows 10/MinGW Release build passed all four suites, including native rendering and cross-process lease checks. MSVC release CI also passed. Desktop duplicate rejection and normal dismissal were checked; the user confirmed the real settings thumbnail. Preview/fullscreen processes coexisted while duplicates exited successfully. See `docs/fullscreen_ownership.md` for evidence and remaining coverage limits.
