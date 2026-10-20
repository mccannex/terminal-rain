# Fullscreen ownership

Terminal Rain refuses overlapping fullscreen rendering before creating graphics
resources. Linux reserves the user's presentation, Windows reserves fullscreen
`/s` mode in the login session, and macOS reserves each display-sized saver view
by `NSScreenNumber`. macOS preview views and smaller Settings canvases remain
independent; Windows `/p` and `/c` do not acquire fullscreen ownership.

POSIX builds use nonblocking `flock` on separate file descriptors. Windows uses a
session-local named mutex with a zero-timeout wait and accepts abandoned mutexes.
Ownership lasts through graphics cleanup. Duplicate launches return success on
Linux/Windows; macOS stops only the rejected view, preserving other displays in
the same host. Lock errors fail closed. Kernel ownership disappears after process
termination, including SIGKILL. Empty POSIX lock files intentionally persist:
unlinking a live lock would allow a new inode and break mutual exclusion.

Linux uses `XDG_RUNTIME_DIR`, falling back to a UID-qualified file in `/tmp`.
macOS uses the host's `NSTemporaryDirectory`. Files are opened without following
symlinks, with close-on-exec, and checked for owner, type, link count and private
permissions. A file error is reported rather than bypassing duplicate protection.
A busy owner is never killed or replaced: hidden/stuck-instance cleanup remains
separate from duplicate protection.

## Automated verification on Fedora, 2026-10-19

A separate Release build passed all four CTest suites: `fullscreen_lease`,
`fullscreen_launch`, `rendering`, and `zombie_confirmation`.

The lease suite checks cross-process exclusion, independent display scopes,
release after normal stop/destruction, restart, abrupt process exit, same-process
view exclusion on POSIX, and rejection of invalid paths/symlinks. The Linux entry
point suite attempts eight launches against a held lease with an intentionally
invalid video driver. All exit successfully before video initialization while the
owner remains alive. After SIGKILL of the owner, a new launch reaches video
initialization; a missing lock directory fails before video initialization.
These tests open no visible windows or graphics connections.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DTERMINAL_RAIN_BUILD_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure -LE native
```

The Linux entry-point suite uses Python 3 when available. The C++ lease suite is
included in all three release build jobs. Native Windows and macOS compilation
and execution were unavailable on Fedora; passing POSIX tests does not validate
the Windows mutex implementation or macOS host integration.

## Remaining native checks

- Linux: observed fullscreen coverage and dismissal with the guarded executable;
  second launch while first is visible, then relaunch after dismissal; repeated
  actual KDE idle activation and display sleep/wake. Visual tests require the
  user to be present and ready before starting.
- Windows: build and run lease tests; duplicate `/s` launches; preview/config
  coexistence; new launch after normal dismissal and forced termination.
- macOS: build/sign and run lease tests; confirm ViewBridge supplies accurate
  screen IDs and display-sized bounds; duplicate views in one and multiple host
  processes; shared temporary-directory access between hosts; different monitors;
  preview/fullscreen coexistence; view resizing/migration and stop/start. Confirm
  rejecting a duplicate view does not obscure the existing host's presentation.

## Incident investigation boundaries

The October 19 unguarded-binary test verified two simultaneous processes; physical
mouse movement dismissed both with exit code zero within the same one-second
sample. It did not reproduce accumulation. The October 15 core establishes a
separate SDL 2.30.9 cleanup crash: failed window-display lookup leads to a null
pointer access in `SDL_DestroyWindow`. Kernel logs establish severe swap use,
but retained records do not attribute it to multiple screensaver processes.
Duplicate protection does not fix that cleanup crash or prove the original
incident's trigger.

## Observed guarded Linux checks, 2026-10-19

At approximately 17:45 MDT, the installed guarded binary rendered on all four
displays. A second launch three seconds later returned zero with the duplicate
ownership message while the original PID stayed alive. The user observed no
flash/restart or coverage change. Physical mouse movement dismissed the original
normally, and no saver process remained. A separate relaunch at approximately
17:47 MDT again covered all displays and exited normally on mouse movement,
confirming release and reacquisition of ownership. The user confirmed both tests.

Automatic KDE idle-cycle observation is separate; those manual-launch results do
not establish automatic activation or display sleep/wake behavior.

## Actual KDE idle cycles, 2026-10-19

The user changed AC RunScriptIdleTimeoutSec to 60 seconds for testing. Initial
observation from 17:49:24 to 17:53:50 MDT found no running saver. A launcher
journal marker was added during that observation; no subsequent launcher attempt
was logged. KDE reported AC active and no ChangeScreenSettings inhibition, even
though Brave had a sleep-only "Playing audio" inhibitor.

After the user paused audio and minimized other windows, KDE reported no active
inhibitors. Actual idle activations were observed at 17:56:26 (PID 422828) and
17:58:33 (PID 423309), both in plasma-powerdevil.service's cgroup. The first
process disappeared at 17:57:01 and the second at 17:58:51, both before the
90-second safety cutoff; neither needed forced termination. The process lifetimes
did not overlap and no process remained. The user confirmed normal four-display
coverage and dismissal for both automatic cycles. Exits of these
detached processes were observed by disappearance, not captured exit codes.

Because audio state and window visibility changed together, this test does not
isolate which affected idle activation. The original multi-instance incident and
SDL display-cleanup crash were not reproduced. The one-minute AC test setting
must be restored to the user's preferred value after idle testing.

## Observed display sleep/wake, 2026-10-19

With AC saver activation at 60 seconds, display-off at 120 seconds and automatic
suspend disabled, KDE launched PID 424213 at 18:02:00 MDT. The user confirmed all
four displays turned off, then physical mouse movement woke them to a responsive
desktop with no rain remaining. The process disappeared at 18:03:29, without the
240-second safety cutoff, and no saver process remained. No matching saver crash
was recorded during this test. This verifies display power-off/wake, not system
suspend/resume or physical monitor disconnection, and does not reproduce or fix
the older SDL display-lookup cleanup crash.

The original AC display-off setting was 1800 seconds (30 minutes). The prior AC
saver delay was absent from powerdevilrc, so its effective original value was not
captured. Restore the user's preferred saver delay after testing; do not infer it
from the separate Battery profile's 300-second setting.

## Observed system suspend/resume, 2026-10-19

With AC idle activation at 60 seconds and the usual four connected displays,
KDE launched PID 426049 at 18:09:21 MDT. The user pressed the physical power
button after approximately 15 seconds of animation. systemd-suspend.service
records suspend beginning at 18:09:38 and return from suspend at 18:10:00.
The same saver PID remained observable after resume, then disappeared at
18:10:12 without the 240-second safety stop; no overlapping PID was observed.
The user confirmed normal visual recovery and dismissal. No matching saver
crash or memory-allocation/OOM event was recorded in the test interval, and
no saver process remained at the final check.

This single successful normal-use suspend/resume test increases confidence in
the guarded build but does not establish the root cause or fix of the older
SDL 2.30.9 display-lookup cleanup crash. Physical monitor disconnection remains
untested by agreement because it is exceedingly rare in the user's workflow.
Restore AC saver activation to the user's preferred 300 seconds after testing;
AC display-off is already back at the original 1800 seconds.
