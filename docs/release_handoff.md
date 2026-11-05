# v1.1.1 release handoff

## Start here

Continue on `codex/prevent-duplicate-fullscreen`. The validated runtime source is
`d5ad85d` (stream-density scheduler and v1.1.1 metadata); later documentation-only
commits do not change those binaries. Windows, Mac and Linux validation is complete
within the limits below. The user authorized merging to main and creating the
v1.1.1 tag/draft on 2026-11-05. Verify the generated release assets, then obtain
the user’s final publishing approval.

## Linux validation procedure (completed within the limits below)

1. Fetch the branch and fast-forward the checkout after checking for local changes.
   Read `platform/linux/AGENT_CONTEXT.md` for the Fedora dependencies, native
   Wayland/HiDPI design and existing powerdevil integration.
2. Build/test the current source, using the directory the idle wrapper launches:

   ```sh
   cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release -DTERMINAL_RAIN_BUILD_TESTS=ON
   cmake --build build/linux -j$(nproc)
   ctest --test-dir build/linux --output-on-failure
   ```

   Expect `fullscreen_lease`, `fullscreen_launch`, `rendering`, and
   `zombie_confirmation`. If `fullscreen_launch` is absent, install Python 3
   and reconfigure before rerunning CTest.
3. Inspect `~/.local/bin/terminal-rain-screensaver.sh` and the active KDE profile;
   confirm the wrapper points to the rebuilt binary rather than a stale build.
   Check for an existing saver before launching. Never unlink a live lease file.
4. With the user present and ready, compare density on all four mixed-resolution/
   HiDPI Wayland displays. Ask for visual confirmation of glyph sizing, density,
   full coverage (including panels), and physical-input dismissal. While the first
   launch is visible, verify a duplicate exits zero without changing coverage;
   then verify normal relaunch after dismissal. Capture PIDs and exit status.
5. Verify actual KDE idle activation and display-off/physical wake with the rebuilt
   binary. Snapshot the active power profile and effective settings before any
   temporary changes; restore and independently read them back afterward. Prior
   preferences were 300-second AC saver activation and 1,800-second AC display-off;
   inspect current settings rather than assuming they are unchanged. Distinguish
   display power-off from system suspend/resume. Prior suspend/resume passed;
   repeat if relevant to observed behavior, without claiming it from display wake.
6. Record the exact source/build, user observations, process cleanup and restored
   settings here. If runtime source changes, rerun affected native checks and
   cross-platform CI before tagging.

The earlier Linux four-display, duplicate/idle/display-power and suspend results
are a baseline from 2026-10-19, before the density change. They do not validate the
new scheduler's appearance. Details and the separate unresolved SDL display-lookup
cleanup crash are in `docs/fullscreen_ownership.md`.

## Completed validation

- **Linux, 2026-11-05:** clean checkout fast-forwarded to `d3be66d`
  (runtime source `d5ad85d`). Release rebuilt in `build/linux` with tests enabled;
  all four expected suites passed: `fullscreen_lease`, `fullscreen_launch`,
  `rendering`, and `zombie_confirmation`. SDL configured both Wayland and X11
  video support. Binary SHA-256:
  `dbbb58db03e8b1fffabe12f6b90ee94ab23fbc8d29a795ac2dcdc91b322b6ad5`.
  The native Wayland/OpenGL rendering suite also passed accelerated drawing,
  pixel readback, synthetic reset, failure handling and preview lifecycle checks.
  The user approved density, sharp glyphs, all four displays including panels,
  and physical mouse dismissal. Current layout: three 1920x1080 displays at 100%
  and a 1920x1080 laptop panel at 125%. Owner PID 1221163 exited zero after 43.6
  seconds without forced termination; a duplicate at ten seconds exited zero
  with the ownership message while the original remained alive. No saver process
  remained. Automated lease tests cover release/reacquisition; a second visible
  relaunch was not repeated today (it passed on 2026-10-19).
  The existing idle wrapper targets this exact build path. KDE reports AC active;
  saved settings show 300-second script activation, 1,800-second display-off and
  automatic suspend disabled. No power settings were changed. At the user's
  request, review compared the current Linux runtime with the tested October
  version: Linux startup, idle integration, input, cleanup and renderer-reset
  paths are unchanged, and the shared frame cadence remains 50 ms. Only stream
  arrivals change, within the existing capacity. The October actual idle,
  display-off/wake and suspend/resume results are carried forward; those were
  not repeated today. This does not claim the separate SDL cleanup crash is fixed.
- **Windows, 2026-11-04:** clean MinGW Release build; all four suites including
  native Direct3D 11; user-approved density, four-display coverage and dismissal.
  The approved local build remains selected under `build-installed-windows`
  with its three MinGW runtime DLLs. Earlier MSVC artifact checks covered settings
  preview, duplicate rejection/coexistence and actual idle/display-off/wake.
  Saver/AC display timeouts were restored to 300/3,600 seconds; DC stayed 300.
  See `docs/stream_density.md` and `platform/windows/AGENT_CONTEXT.md`.
- **Mac, 2026-11-05:** Release arm64 and universal builds; all five suites on both
  builds; separate native Metal rendering/readback/reset tests; strict/deep
  signatures and ditto ZIP/extraction round trip. The downloaded Mac CI bundle
  also passed the native view lifecycle test. Both slices target macOS 26.0 and
  report 1.1.1. The verified CI bundle remains installed, with the previous 1.1.0
  backed up under `build/macos-validation-2026-11-05/` on the Mac.
  The user approved real fullscreen density, sharp glyphs, built-in 1440x900
  coverage and dismissal, plus Settings-preview/fullscreen coexistence and
  preview recovery. Logs confirmed 20 fps and input-confirmed host termination;
  the standalone run left no engine/saver process. With Settings open, its shared
  host terminated after dismissal and Settings respawned the preview normally.
  Continued-input preview restarts are accepted behavior. Mac power settings
  were unchanged. Automatic idle/display-off/sleep was last checked 2026-10-22.
- **Current-source CI, 2026-11-05:** [run 42083438520](https://github.com/mccannex/terminal-rain/actions/runs/42083438520)
  passed Windows, Linux and Mac build/test/package jobs for `d5ad85d`.
  All uploaded artifacts were downloaded and inspected on the Mac; Windows/Linux
  executables were not run there. CI excludes native graphics tests. Manual
  dispatch created artifacts only; the release job was skipped.

## Release boundary and known limits

Linux results were reviewed with the user, who authorized merging and tagging
v1.1.1 on 2026-11-05. Publishing requires final approval of the verified draft. The
workflow and `.github/RELEASE_NOTES.md` already target v1.1.1. Current-source CI
above supersedes older pre-density runs; rerun if release source changes.

Real X11, Intel Mac execution, multiple physical Mac displays and actual GPU
resource loss remain untested. Do not add a Mac per-view fullscreen lease:
ViewBridge Settings previews can be display-sized with `isPreview=NO`; that
approach caused black fullscreen output and was removed. Duplicate protection
is intentionally limited to Windows/Linux. Keep the separate SDL cleanup crash
and unproven accumulation hypothesis distinct from the lease fix.
