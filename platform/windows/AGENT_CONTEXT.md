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

## Dismissal and testing

Key presses, mouse clicks, scrolling and debounced mouse movement dismiss
all fullscreen windows together. Keep `SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH`
set to `"1"` in `runShow`: otherwise SDL consumes the click that focuses a
window, requiring a second click on an unfocused monitor.

Test the actual `.scr` launch as well as the dialog preview. Check full
coverage and first-click dismissal on each monitor, then test keyboard,
scrolling and movement in separate runs. Recheck real idle activation when
changing integration code.

Window-position utilities can relocate fullscreen windows and generate
synthetic motion. Exclude the screensaver from their restoration rules;
for PersistentWindows, use `-ignore_process "Terminal Rain.scr"`
([help](https://github.com/kangyu-california/PersistentWindows/blob/master/Help.md)).
