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
what real distribution outside this machine would additionally need.

Install for local testing:

```bash
cp -R "build/Glyph Rain.saver" ~/Library/Screen\ Savers/
```

`~/Library/Screen Savers` is auto-scanned by System Settings' Screen Saver
picker — no separate registration step (unlike Windows' `InstallScreenSaver`
or Linux's powerdevil KCM hookup).

## Verification gate (adapt from Windows/Linux)

Live-tested on this real machine (macOS 26 "Tahoe"), mirroring the other
platforms' gates:

1. ✅ Appears in System Settings → Screen Saver's picker list (confirms the
   bundle/`Info.plist`/`NSPrincipalClass` wiring is valid to the OS, not just
   "compiles").
2. ⚠️ Grid tile thumbnail does not render — see "Known limitation: picker
   grid thumbnail" below. Not a bug in this bundle.
3. ✅ Full-screen activation via System Settings' "Preview" shows the
   animation, correctly sized, on-screen.
4. ✅ Exits cleanly on real input — confirmed via mouse movement during a
   live "Preview" run. Falls out for free from `legacyScreenSaver` owning
   that lifecycle rather than us, as expected.
5. ✅ HiDPI/Retina sizing looks correct — confirmed on this machine's
   built-in Retina display (2560x1600). `SDL_CreateWindowFrom` on Cocoa gives
   the same automatic drawable-size scaling as a top-level window; no
   Linux-style manual `contentScale` handling was needed.

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

**Fully verified working** on this real machine: builds and ad-hoc signs
cleanly (`cmake --build build --target glyph_rain_saver`), installs into
`~/Library/Screen Savers`, appears in System Settings' picker, animates
correctly full-screen via "Preview" with correct Retina sizing, and exits
cleanly on real input. The only open item is the picker grid thumbnail (see
above), believed to be an OS-level limitation rather than something fixable
here.

No dedicated packaging beyond the raw `.saver` bundle yet (no DMG/installer,
no notarization) — matches Windows/Linux both still being "copy the built
artifact into place by hand" at this stage of the project, not a real
distribution loose end unique to macOS.
