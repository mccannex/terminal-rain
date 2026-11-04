# v1.1.1 release handoff

Branch: `codex/prevent-duplicate-fullscreen`. Release metadata and notes target
v1.1.1. No tag or release has been created for these changes.

## Completed on Windows, 2026-11-04

- Clean MinGW Release build and all four CTest suites passed, including native
  Direct3D 11, ownership, failure/reset handling, and fractional spawn rates.
- The user approved fullscreen density across monitors and normal dismissal.
  A direct launch subsequently reconfirmed all four displays and exited zero.
- Before the density change, the MSVC CI artifact passed settings-thumbnail,
  process coexistence, duplicate rejection, and automatic idle/display-off/wake
  checks. The original 300-second saver and 3,600-second AC display timeouts
  were restored and independently read back; DC display-off remained 300 seconds.
- The approved density binary is now selected as the local saver at
  `build-installed-windows/Terminal Rain.scr`, alongside `libgcc_s_seh-1.dll`,
  `libstdc++-6.dll`, and `libwinpthread-1.dll`. This local MinGW installation is
  separate from the public MSVC CI artifact and loads without the compiler PATH.

Detailed evidence: `docs/stream_density.md` and `docs/fullscreen_ownership.md`.

## Remaining on Linux

Build the current branch as Release with `TERMINAL_RAIN_BUILD_TESTS=ON`, run
CTest (including `fullscreen_launch` when Python is available), then compare
density on the real mixed-resolution/HiDPI Wayland displays. Verify coverage,
normal dismissal, duplicate rejection/relaunch, and the configured KDE idle
integration. Preserve and restore any temporary power settings.

## Remaining on macOS

Build Release arm64 and universal (`arm64;x86_64`) bundles with tests enabled.
Run all five CTest suites in a desktop session, plus
`terminal_rain_rendering_tests --native`. Verify the bundle signature and the
release workflow's ditto ZIP/extraction signature round trip.

Install and visually check the new density in the real host, including Retina
sizing, Settings/fullscreen coexistence, and normal dismissal/cleanup. The Mac
view now derives its unchanged 50-ms cadence from the shared scheduler constant.
Do not restore the rejected per-view fullscreen lease: ViewBridge Settings
previews can have full-display bounds and `isPreview=NO`.

## Final release steps

After the final source is available remotely, rerun `release.yml` on this branch
and check all three platform artifacts. The successful run
https://github.com/mccannex/terminal-rain/actions/runs/41930548026 tested
`2045168`, before the density and v1.1.1 metadata edits. It does not validate
the final source. Tag/create the draft only after the remaining native checks;
publishing is still a separate user action.

## Windows launch pitfall

The Windows `.scr` shell association is `"%1" /S`. PowerShell `Start-Process`
using shell execution can therefore launch fullscreen even when a caller
supplies `/a`, `/p`, or `/c`. A hidden-window startup flag can hide the first
display window while other display windows remain visible. This caused a
three-screen diagnostic run; a direct normal-visibility launch covered all four.

For argument-specific tests, use `ProcessStartInfo.UseShellExecute = false`
with explicit arguments. Do not request hidden startup for a visual fullscreen
test. The source `.exe` is also suitable for argument-specific diagnostics.
