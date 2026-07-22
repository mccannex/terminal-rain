# macOS Agent Context

For whichever agent (Claude Code or otherwise) is working on this repo's macOS
target. Tracked in git so it travels between machines/sessions; update it with
real status before you're done. The app is essentially finished — this file is
now a reference for maintenance work, not a running build log.

## Project, in brief

**Glyph Rain**: a cross-platform rewrite of a classic Win32/GDI "Matrix
digital rain" screensaver (originally by Louai Munajim, CC BY 3.0) on top of
SDL2, with a platform-agnostic simulation core (`core/`) shared across all
targets. See the root `README.md` for licensing/credits, and
`platform/linux/AGENT_CONTEXT.md` for the Linux integration.

## Architecture

The macOS target (`platform/macos/GlyphRainView.mm`, CMake target
`glyph_rain_saver`) is a `.saver` bundle: a loadable `NSBundle` plugin hosted
by `legacyScreenSaver.appex`, not a standalone app like the Windows `.scr` or
Linux binary. Key decisions, all still in force:

- **No Xcode.app — Command Line Tools only.** Everything needed (clang++,
  `ScreenSaver.framework`, `codesign`) is in the CLT. The one gap is `ibtool`
  (Xcode-only), so never use a XIB/storyboard: if a config sheet is ever
  added (`hasConfigureSheet` currently returns `NO`), build its UI in code.
- **SDL window is created lazily in `-startAnimation`**, via
  `SDL_CreateWindowFrom((__bridge void*)self)` — at init time the view isn't
  attached to a real `NSWindow` yet, and SDL resolves the window via
  `[nsview window]`.
- `animationTimeInterval = 1/20` matches the `SDL_Delay(50)` cadence the
  other platforms use. Retina drawable sizing comes free from SDL's Cocoa
  backend — no manual scale handling.

## Host-process lifecycle — the non-obvious part

Two serious bugs lived here, both fixed and live-verified. The full design
rationale is in `GlyphRainView.mm`'s comments — trust those over this
summary:

1. **SDL blocked the idle-trigger.** `SDL_Init` defaults to holding a
   "prevent screensaver" power assertion — self-defeating when this process
   *is* the screensaver. Fixed with `SDL_SetHint(
   SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1")` before `SDL_Init`; verify with
   `pmset -g assertions` if it ever regresses.
2. **The host never stops the saver.** On this macOS (26.5), the host
   process is torn away at dismissal *without* delivering `-stopAnimation`,
   leaving the appex animating invisibly forever (observed: 6+ hours at
   100%+ CPU, 673MB GPU-backed footprint). The appex receives lifecycle
   *and input* over a ViewBridge/XPC connection from its host, so once the
   host dies no event can ever reach it — neither `NSResponder` overrides
   nor `SDL_PollEvent` can work (both were tried; don't re-attempt). The
   fix polls the global HID idle time
   (`CGEventSourceSecondsSinceLastEventType`) each frame and `exit(0)`s the
   whole process once user input is seen that a healthy host would have
   acted on (~2s window, retroactive so lock-flag lag can't hide a single
   input blip, suspended while the screen is locked), with a 4-hour
   watchdog as backstop. Note: PPID is *not* an orphan signal — a healthy
   appex is spawned by launchd and has PPID 1 from birth.

Debugging aids:

- `os_log` subsystem `com.mccannex.glyph-rain` logs start/stop/termination
  plus a ~10s heartbeat (frame count, HID idle, lock state). Watch with
  `/usr/bin/log stream --predicate 'subsystem == "com.mccannex.glyph-rain"'`
  — full path matters in zsh, whose `log` *builtin* shadows the CLI.
- Manufacture a zombie on demand: `open -a
  /System/Library/CoreServices/ScreenSaverEngine.app`, then any input
  dismisses the engine and strands the appex, which must self-terminate ~2s
  after real input. (Synthetic `CGEventPost` input dismisses the engine but
  doesn't clear the session's lock flag, so it can look a few seconds
  slower than real input — that's the test harness, not the fix.)
- Always verify process death directly (`ps aux | grep legacyScreenSaver`,
  CPU sampling, `sample <pid>` for stacks) — never from visuals alone.

## Build, install, test

```bash
brew install cmake        # SDL2 is vendored via FetchContent, no brew sdl2
cmake -S . -B build
cmake --build build --target glyph_rain_saver
rm -rf ~/Library/Screen\ Savers/Glyph\ Rain.saver && cp -R "build/Glyph Rain.saver" ~/Library/Screen\ Savers/
```

The bundle is ad-hoc signed as a `POST_BUILD` step — sufficient for locally
built bundles (no quarantine attribute). `~/Library/Screen Savers` is
auto-scanned by System Settings' Screen Saver picker.

## Distribution — decided: no notarization

Release artifacts are bare `.saver` zips; the documented install is the
`curl`-based snippet in `.github/RELEASE_NOTES.md`. The reasoning, all
confirmed live:

- Browser downloads get `com.apple.quarantine`, and Gatekeeper hard-refuses
  quarantined ad-hoc-signed bundles with a misleading "is damaged" error.
  The real fix is a paid Developer ID + notarization — **explicitly
  rejected** ($99/year).
- `curl` never sets the quarantine attribute (only browsers do), so the
  snippet sidesteps Gatekeeper entirely. A helper `.command` script shipped
  in the zip does *not* work — it arrives quarantined itself.
- The snippet is wrapped in `bash <<'INSTALL_GLYPH_RAIN' ...` because
  interactive zsh has `interactivecomments` off by default: pasted `#`
  comment lines are parsed as commands and break. Test paste-flows with
  `zsh -i -s < paste.txt`, not by running a script file — different code
  path.

## Known limitations (OS-level, not fixable from this bundle)

- **Picker grid thumbnail**: shows a generic icon. The legacy-bridge
  wallpaper appex on this macOS simply doesn't generate grid thumbnails for
  third-party `.saver` bundles. `thumbnail.png`/`@2x` (see
  `tools/generate_macos_thumbnail.py`) are still shipped in case a future
  macOS honors them. Cosmetic only; the large selected-preview animates
  correctly.
- **~2s of invisible animation after every dismissal** before the appex
  self-terminates — inherent to the lifecycle fix above, by design.

## Status

**Done and fully verified on this machine (macOS 26 "Tahoe")**: appears in
the picker, animates correctly full-screen (Preview and real idle-trigger)
with correct Retina sizing, exits on input, no longer blocks its own
idle-trigger, and self-terminates its host process ~2s after dismissal
instead of animating invisibly forever. Distribution ships via CI
(`.github/workflows/release.yml`) with the curl install flow. No open work
beyond an optional future config sheet.
