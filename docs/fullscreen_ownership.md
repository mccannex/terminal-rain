# Fullscreen ownership

Terminal Rain refuses overlapping fullscreen rendering before creating graphics
resources on Linux and Windows. Linux reserves the user's presentation and
Windows reserves fullscreen `/s` mode in the login session; Windows `/p` and
`/c` do not acquire fullscreen ownership. The initial macOS per-view lease was
removed after real-host testing exposed a black fullscreen presentation when
Settings' preview was open. macOS retains its existing host lifecycle cleanup.
Historical macOS lease design/testing below describes that rejected approach.

POSIX builds use nonblocking `flock` on separate file descriptors. Windows uses a
session-local named mutex with a zero-timeout wait and accepts abandoned mutexes.
Ownership lasts through graphics cleanup. Duplicate launches return success on
Linux/Windows. Lock errors fail closed. Kernel ownership disappears after process
termination, including SIGKILL. Empty POSIX lock files intentionally persist:
unlinking a live lock would allow a new inode and break mutual exclusion.

Linux uses `XDG_RUNTIME_DIR`, falling back to a UID-qualified file in `/tmp`.
The removed macOS guard used the host's `NSTemporaryDirectory`. Files are
opened without following
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
- macOS: the unsafe per-view guard has been removed. Verify actual Settings
  preview/fullscreen coexistence and dismissal after changes. Multiple physical
  monitors and Intel hardware require separate native coverage; per-display
  lease migration is no longer part of the macOS implementation.

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

## macOS build and automated validation, 2026-10-22

Checked branch `codex/prevent-duplicate-fullscreen` at `373b7f1` on the
M1 MacBook Air running macOS 26.6.2 with AppleClang 21.0.0. Native arm64
and universal (`arm64;x86_64`) Release builds passed with tests and the
optional capture tool enabled. Both builds passed all four macOS CTest
suites: `fullscreen_lease`, `zombie_confirmation`, `rendering`, and
`color_macos`. Both also passed the separate rendering executable's
`--native` checks using Metal, including readback and synthetic resets.

The native graphics checks require desktop access: the sandbox-only attempt
could not enumerate displays; rerunning with desktop access passed. Both
bundles passed strict deep signature verification. The universal bundle
contains both architectures and also passed verification after the release
workflow's ditto ZIP/extraction round trip. SDL emitted existing deprecation
and extension warnings; no project source changes were needed.

These runs execute the arm64 slice. Intel hardware, actual ViewBridge host
lease behavior, shared host temporary-directory access, duplicate-view
visibility, preview/fullscreen coexistence, monitor migration, and real
host stop/start remain unverified. No installed saver or system settings
were changed during this validation. The build/test checks are ready for
the next interactive macOS host validation step.

## macOS real-host regression found, 2026-10-22

The verified universal bundle was backed up/installed locally for interactive
testing. The first launch reused the previous build's existing host despite
SIGTERM, so that run is not evidence for the new ownership code. After the
old host exited, PID 15905 loaded the new 3,848,800-byte universal executable
and held `display-1.lock` in the legacyScreenSaver container temporary
directory. The user confirmed normal fullscreen animation and dismissal;
the host logged cleanup and disappeared after unlock.

With the System Settings preview open, the host reused one view and created
a second display-sized view in PID 16187. The second was rejected as busy
and the user observed a black fullscreen presentation. This is a release
blocking regression. A diagnostic build then established that the embedded
Settings preview has `isPreview=NO`, bounds and window frame 1440x900,
matching NSScreen.frame, in an NSServiceViewControllerWindow. Its level was
-2147483625 and XPC_SERVICE_NAME was
`com.apple.ScreenSaver.Engine.legacyScreenSaver`. The size-based test cannot
distinguish this preview from a legitimate fullscreen view.

A local hidden-window bundle harness passed same-process duplicate rejection,
true-preview and smaller-canvas coexistence, stop/restart, and resize-to-fullscreen
lease acquisition. Those checks did not model Apple's full-size ViewBridge
preview and therefore did not predict the observed regression. A subsequent
experimental cross-process harness check was invalid: passing a directory to
the harness did not change NSTemporaryDirectory used by the bundle. It is
not evidence for or against cross-host exclusion.

The previous installed saver is preserved under
`build/macos-host-validation/Terminal Rain.previous.saver`. Test logs and
the diagnostic harness are in the same ignored build directory. The original
five-minute saver activation setting was not changed.

## Corrected macOS implementation, 2026-10-22

The user approved removing the unsafe macOS per-view lease, while retaining
Linux/Windows duplicate protection and the established macOS zombie cleanup.
TerminalRainView.mm was restored to its pre-lease implementation.

Added `views_macos`, a native hidden-window test that loads the built bundle
and starts two display-sized `isPreview=NO` views plus a true preview. It
executes animation frames, verifies simultaneous animation, and checks that
stopping/restarting either non-preview view preserves the other. The test
failed against the guarded diagnostic bundle with "fullscreen starts alongside
display-sized Settings canvas" and passed after removing the guard.

Both corrected arm64 and universal Release builds passed all five CTest
suites and the universal build's separate native Metal rendering/reset check.
Strict bundle signing and ditto ZIP/extraction signature verification passed.
The release workflow builds the new test; it remains labeled native and is
excluded from headless CI execution with the other native graphics checks.

The user confirmed that the corrected installed universal build showed normal
rain with the Settings preview open and a second fullscreen launch request,
and dismissed normally. PID 18536 animated both display-sized views, logged
post-unlock zombie cleanup, and exited; Settings subsequently started a new
preview host. The recorded display-sleep prevention assertion was zero.
This verifies coexistence and dismissal on the built-in display, not multi-monitor
behavior or Intel execution.

## Corrected macOS automatic idle and sleep/wake, 2026-10-22

The user confirmed the corrected Settings/fullscreen coexistence run and
authorized temporary idle/display-off timeouts, with restoration afterward.
Original settings: saver activation 300 seconds, battery display-off 5 minutes,
AC display-off 60 minutes. The first attempt changed AC display-off to 2 minutes
but the machine was on battery, so that setting did not govern the run. Saver
activation occurred automatically; the user dismissed before the unchanged
five-minute battery display deadline. That attempt verifies activation/dismissal,
not display sleep.

After separate user authorization, battery display-off was shortened to 2 minutes
and AC restored to 60 minutes. With saver activation at 60 seconds and no
synthetic activation/input, PID 20888 started at 22:51:22 MDT with HID idle
61.04 seconds. It animated until macOS turned the display off at 22:52:31,
when the real host delivered stopAnimation (1,382 frames). powerd recorded
system idle sleep at 22:52:40 and physical trackpad/keyboard wake at 22:54:16,
with display-on at the same time. The monitoring process paused during sleep.
The user confirmed normal rain, complete display-off, and a responsive desktop
after physical wake/unlock, with no rain left over. This also supplies an
observed healthy real-host stop, distinct from orphan cleanup after dismissal.

Settings were restored and independently read back: saver idleTime 300, battery
displaysleep 5, AC displaysleep 60. Other power/security settings were not
changed. The installed corrected universal bundle's strict signature verified.
The test host may remain resident or be reused by Settings after normal stop;
process residency alone is not evidence of continued animation.

The M1 built-in-display build, automated, coexistence, automatic activation,
display power, sleep/wake and dismissal checks are complete. Multiple physical
Mac displays, Intel hardware, and actual GPU resource loss remain untested.
No macOS duplicate-prevention claim is made: that new guard was deliberately
removed; Linux/Windows retain it. Windows/Linux retesting of the updated
branch remains pending. Multi-monitor checks are deferred because no external
display is available on this Mac.
