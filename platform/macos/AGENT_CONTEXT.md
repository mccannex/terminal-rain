# macOS Agent Context

For whichever agent (Claude Code or otherwise) is working on this repo's macOS
target. Same purpose as `platform/linux/AGENT_CONTEXT.md` — tracked in git so
it travels between machines/sessions; update it with real status before
you're done.

## Project, in brief

**Glyph Rain**: a cross-platform rewrite of a classic Win32/GDI "Matrix
digital rain" screensaver (originally by Louai Munajim, CC BY 3.0) on top of
SDL2, with a platform-agnostic simulation core (`core/`) shared across all
targets. See the root `README.md` for licensing/credits, and
`platform/linux/AGENT_CONTEXT.md` for how the Linux integration reused the
same core.

## Integration approach — decided

A `.saver` bundle is a loadable `NSBundle` plugin (`CFBundlePackageType`
`BNDL`) hosted by `legacyScreenSaver`, not a standalone app — fundamentally
different lifecycle from Windows' `/s /c /p /a` shell (`src/win32/main.cpp`)
or Linux's powerdevil-launched standalone process
(`platform/linux/AGENT_CONTEXT.md`). Two consequences that shaped
`src/macos_saver/GlyphRainView.mm`:

- **No Xcode.app required.** Deliberately avoided — see the conversation that
  led here. Command Line Tools alone provide everything needed: `clang++`,
  `ScreenSaver.framework` (present in the CLT SDK), `codesign`, and
  `notarytool`. The one real gap is `ibtool` (Interface Builder's compiler,
  Xcode-only) — worked around by never using a XIB/storyboard for the config
  sheet. `hasConfigureSheet`/`configureSheet` currently return `NO`/`nil`
  (mirrors `src/win32/main.cpp`'s `runConfigure` stub — no settings are
  configurable yet); when that changes, build the sheet's UI in code.
- **No custom multi-monitor loop or exit-on-input logic needed.**
  `legacyScreenSaver` instantiates one `GlyphRainView` per display itself and
  owns idle-activation/teardown-on-input at the process level — unlike
  `core/app_loop.cpp`'s `runMultiDisplayStreamLoop` (Windows/Linux), this view
  never pumps SDL's event queue or decides when to exit. Each view just drives
  its own `StreamField` off `animateOneFrame`, called by the base class's
  internal timer (`animationTimeInterval`, set to `1/20` to match the
  `SDL_Delay(50)` cadence the other platforms use).

SDL window setup happens lazily in `-startAnimation` (via
`SDL_CreateWindowFrom((__bridge void*)self)`), not `-initWithFrame:isPreview:`
— at init time the view isn't necessarily attached to a real `NSWindow` yet
(the host installs it after construction), and SDL's Cocoa backend resolves
the window via `[nsview window]`, which needs that attachment to have already
happened.

## Build setup

```bash
brew install cmake sdl2   # sdl2 itself isn't actually linked against --
                           # CMake FetchContent vendors its own pinned SDL2
                           # source, same as every other platform -- but cmake
                           # is required and wasn't preinstalled on this
                           # machine.
cmake -S . -B build
cmake --build build --target glyph_rain_saver
```

Produces `build/Glyph Rain.saver`, ad-hoc signed as a `POST_BUILD` step
(`codesign --force --deep --sign -`) so Gatekeeper/AMFI accept a locally
built, non-notarized bundle for local install/testing. No Apple Developer
account or identity involved at this stage — see "Distribution" below for
what real distribution outside this machine additionally needed, and the
decision not to pursue it.

Install for local testing:

```bash
cp -R "build/Glyph Rain.saver" ~/Library/Screen\ Savers/
```

`~/Library/Screen Savers` is auto-scanned by System Settings' Screen Saver
picker — no separate registration step (unlike Windows' `InstallScreenSaver`
or Linux's powerdevil KCM hookup).

## Distribution — decided against paid notarization

A file built and `cp -R`'d locally (as above) has no `com.apple.quarantine`
attribute, so ad-hoc signing is enough for Gatekeeper to accept it. A copy
downloaded through a browser (i.e. every release artifact) *does* get that
attribute, and Gatekeeper flatly refuses to open a quarantined ad-hoc-signed
bundle at all -- confirmed live: downloading `v1.0.1`'s release zip and
trying to install it produced `"Glyph Rain.saver" is damaged and can't be
opened`, a misleading message (nothing is actually corrupt) for what's
really just an ad-hoc signature not satisfying a quarantined file's stricter
check.

The real fix is a paid Apple Developer Program membership ($99/year): a
"Developer ID Application" certificate to sign with instead of ad-hoc, plus
`xcrun notarytool submit` + `xcrun stapler staple` as extra CI steps. **Not
pursued** -- explicitly rejected in favor of a free workaround.

**First attempt (abandoned): `scripts/macos/Install Glyph Rain.command`.**
Packaged alongside the `.saver` bundle in the release zip, it copied the
bundle into `~/Library/Screen Savers` and ran `xattr -dr
com.apple.quarantine` on the copy. Worked when tested by directly invoking
the script locally, but the actual end-to-end flow (download the zip via
browser, then double-click the script) failed: the script itself was also
quarantined after download, and on this macOS version double-clicking (or
even right-click → Open) a quarantined unsigned script shows the same hard
"Apple could not verify ... is free of malware" block with no bypass
button -- the older "right-click to open anyway" affordance most guidance
online still describes no longer reliably applies. This just moved the
Gatekeeper dead-end one level down rather than fixing it.

**What actually shipped:** `.github/RELEASE_NOTES.md` gives a single `curl`
command (`releases/latest/download/Glyph.Rain-macOS.zip`) that downloads,
extracts, and installs the bundle in one line. `curl` never sets
`com.apple.quarantine` in the first place -- only browsers do, as part of
their own download-safety UI -- so this sidesteps Gatekeeper entirely rather
than needing to bypass it after the fact. No script shipped in the release
zip at all; `.github/workflows/release.yml`'s macos job packages just the
bare `.saver` bundle again. Verified end-to-end live: running the documented
`curl` command installs a copy with zero quarantine-related extended
attributes, confirmed via `xattr -lr`.

## Verification gate (adapt from Windows/Linux)

Live-tested on this real machine (macOS 26 "Tahoe"), mirroring the other
platforms' gates:

1. ✅ Appears in System Settings → Screen Saver's picker list (confirms the
   bundle/`Info.plist`/`NSPrincipalClass` wiring is valid to the OS, not just
   "compiles").
2. ⚠️ Grid tile thumbnail does not render — see "Known limitation: picker
   grid thumbnail" below. Not a bug in this bundle.
3. ✅ Full-screen activation via System Settings' "Preview" *and* via a real
   idle-trigger (System Settings → Screen Saver → "Show screensaver after" →
   1 minute, left genuinely idle) both show the animation, correctly sized,
   on-screen.
4. ✅ Exits cleanly on real input — confirmed via mouse movement, both from
   a live "Preview" run and from a real idle-triggered activation. Falls out
   for free from `legacyScreenSaver` owning that lifecycle rather than us,
   as expected.
5. ✅ HiDPI/Retina sizing looks correct — confirmed on this machine's
   built-in Retina display (2560x1600). `SDL_CreateWindowFrom` on Cocoa gives
   the same automatic drawable-size scaling as a top-level window; no
   Linux-style manual `contentScale` handling was needed.
6. ✅ Doesn't block its own idle-trigger — see "Fixed bug: SDL's default
   screensaver-disable assertion" below. Was broken, now fixed and verified.

## Fixed bug: SDL's default screensaver-disable assertion

`SDL_Init(SDL_INIT_VIDEO)` defaults to disabling the OS's idle/screensaver
detection (an `IOPMAssertionCreate` of type `PreventUserIdleDisplaySleep`,
literally named `"... using SDL_DisableScreenSaver"` in `pmset -g
assertions`) -- meant to stop games from being interrupted by some *other*
screensaver kicking in mid-session. Self-defeating here: this process *is*
the screensaver, so that default silently prevented macOS from ever
idle-triggering it in the first place. Confirmed live: with "Show
screensaver after" set to 1 minute, nothing happened, and `pmset -g
assertions` showed a `legacyScreenSaver` process holding that exact
assertion, `14:50:01` old -- far longer than the current session, meaning
even a single leftover instance (e.g. from a Preview run, or a brief
instantiation for a thumbnail attempt) keeps blocking every subsequent idle
trigger for as long as that process happens to stick around.

Fixed in `GlyphRainView.mm`'s `setUpIfNeeded`: `SDL_SetHint
(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1")` before `SDL_Init`, so SDL never
creates the assertion at all. Verified clean afterward via `pmset -g
assertions` across both a real idle-triggered activation-then-dismiss cycle
and a Preview activation-then-dismiss cycle -- no assertion left behind
either way.

## Known limitation: orphaned `legacyScreenSaver` host process

Independent of the assertion bug above (already confirmed fixed): after a
`GlyphRainView` instance's `-stopAnimation`/`teardown` correctly release all
SDL resources (verified -- no assertion, no leaked memory from our object),
the `legacyScreenSaver` *host process* itself doesn't always terminate. It
was observed, after both a real idle-triggered run and a Preview run, still
alive and reparented to `launchd` (PPID 1) -- i.e. its original spawning
parent (`WallpaperAgent`/System Settings' Wallpaper extension) had already
exited without first terminating this child.

This matches what `log stream` showed earlier while diagnosing the
thumbnail limitation: `runningboardd` explicitly logs this process as "not
memory-managed" / "not lifecycle managed" / "will not be managed" --
meaning macOS deliberately opts this process type out of its normal
automatic-reaping behavior. That log line appeared regardless of which
screensaver module was active (Apple's own "Random" module included), so
this looks like inherent behavior of the `legacyScreenSaver.appex`
compatibility shim on this macOS version, not something introduced by this
bundle's code -- there's no host-process-exit hook exposed to
`ScreenSaverView` subclasses to fix this from our side even if it were our
bug.

Practical effect: repeated activations (real or Preview) can accumulate
harmless-but-wasteful orphaned `legacyScreenSaver` processes (small,
idle, holding no assertions) until the user logs out/restarts, or manually
`kill`s them. Not a resource leak *per activation loop* (each instance's
own SDL/GL/atlas resources are released correctly), just a host-process
cleanup gap outside this bundle's control.

## Known limitation: picker grid thumbnail

The small tile System Settings' Screen Saver grid shows next to "Glyph Rain"
displays a generic icon instead of anything derived from the actual
animation — confirmed not fixable from the bundle side. Diagnosis (this
session, via `log stream` while opening the picker):

- This macOS version routes screensavers through a unified **Wallpaper**
  subsystem (`WallpaperAgent`, `com.apple.wallpaper:catalog`), not the
  classic screensaver picker the `thumbnail.png`/`thumbnail@2x.png`
  convention (90x58 @1x / 180x116 @2x, dropped loose in `Contents/Resources`,
  no `Info.plist` entry needed — see other open-source `.saver` projects)
  was designed for. Those two files are still shipped in the bundle (see
  `tools/generate_macos_thumbnail.py`) in case a future macOS version's
  picker honors them, but confirmed to have no effect here.
- The log shows `WallpaperAgent`'s "Initialize screen saver wallpaper for
  module" step completing in ~10ms for our module
  (`com.mccannex.glyphrain.screensaver`) with no errors — too fast to
  actually be spinning up SDL/a GL context and rendering a frame, meaning
  it's almost certainly not attempting a live-render snapshot for the tile
  at all.
- Third-party legacy `.saver` bundles are bridged into the new Wallpaper
  architecture via a dedicated `com.apple.wallpaper.extension.legacy`
  appex — a per-bundle cache dir exists at
  `~/Library/Containers/com.apple.wallpaper.agent/Data/Library/Caches/com.apple.wallpaper.caches/screenSaver-/<path-to-bundle>`,
  but it was empty for both this bundle and a since-removed unrelated legacy
  `.saver` (`Matrix.saver`) tested for comparison — indicating that
  directory is sandbox file-access bookkeeping, not actual cached thumbnail
  pixels. Deleting it and reinstalling the bundle from scratch (full delete
  + re-copy, killing `legacyScreenSaver`/`iconservicesagent`/System Settings
  in between) made no difference either.
- Conclusion: this looks like a genuine limitation of the `legacy` bridge
  extension in this macOS version — it likely just doesn't generate custom
  grid thumbnails for third-party legacy `.saver` bundles, full stop,
  regardless of bundle contents. The larger "selected" preview canvas (which
  does animate the real view) is unaffected and is the meaningful functional
  confirmation; the grid tile is cosmetic only.

## Status

**Fully verified working** on this real machine, including a real
idle-trigger (not just Preview): builds and ad-hoc signs cleanly (`cmake
--build build --target glyph_rain_saver`), installs into `~/Library/Screen
Savers`, appears in System Settings' picker, animates correctly full-screen
with correct Retina sizing, exits cleanly on real input, and no longer
blocks its own idle-trigger (see "Fixed bug" above). Two open items, both
OS-level and not fixable from this bundle: the picker grid thumbnail (see
above), and orphaned `legacyScreenSaver` host processes accumulating across
activations (see above) -- neither affects the screensaver's actual
functionality.

Release packaging exists via CI (see "Distribution" above): as of `v1.0.3`,
the documented install path is a single `curl` command that downloads and
installs the plain `.saver` bundle, sidestepping Gatekeeper's browser-
download quarantine entirely rather than needing to work around it. No DMG,
no notarization (deliberately not pursued).
