# Changelog

## [1.1.1] — 2026-11-05

- Prevent overlapping fullscreen launches on Windows and Linux. A duplicate exits successfully without opening windows; ownership releases after normal exit or a crash.
- Make rain density more consistent across display sizes and scaling levels. Stream arrivals scale with glyph-column count and preserve fractional rates; glyph sizes, speeds and trail lengths retain their existing tuning.
- Preserve macOS Settings-preview/fullscreen coexistence by retaining its host lifecycle rather than applying the Windows/Linux fullscreen lease to views.
- Add process-ownership, fractional-arrival and macOS view-lifecycle regression coverage. Verify the new density on native Windows, macOS and Linux displays.
- Add MIT licensing for original project contributions, preserve third-party notices, and distribute license documents alongside release binaries.
- Introduce this changelog; keep release installation instructions separate.

## [1.1.0] — 2026-10-06

- Rename Glyph Rain to **Terminal Rain**, refresh documentation and add an animated preview.
- Reduce rendering work with batched drawing, color grouping and culling of invisible glyphs and erase cells. Use a per-field random generator and frame pacing that accounts for drawing time.
- Scale the active-stream capacity with surface area. This was a capacity change; column-based arrival scaling follows in v1.1.1.
- Improve Windows settings-preview glyph scaling, density and sustained rain.
- Dismiss fullscreen presentations on keyboard, click, scroll and debounced mouse movement; fix Windows first-click dismissal.
- Allow configured display sleep on Windows and Linux while the saver runs.
- Fix preview resource teardown and stop cleanly on rendering failures and renderer-reset notifications.
- Stop macOS screensaver hosts that remain running invisibly after dismissal, including during continued typing or mouse activity.
- Tune the shared green palette and explicitly tag macOS Metal output as sRGB.
- Provide universal macOS builds for Apple Silicon and Intel; Intel execution remains untested. Verify bundle signatures before and after ZIP packaging and copy resources before signing.
- Select optimized Release builds for Linux and macOS, remove unused runtime configuration, and add rendering/reset/cleanup regression checks and native platform validation.

## [1.0.0–1.0.4] — 2026-07-11

These rapid initial releases introduced the screensaver and refined macOS activation and installation:

- Rebuild the classic Matrix Screen Saver simulation around a shared SDL2 core with an embedded TerminalVector 8x12 glyph atlas.
- Add a native Windows `.scr`, a macOS `.saver` bundle and Linux/KDE idle-script integration, with SDL2 linked into the binaries.
- Cover multiple displays with coordinated dismissal and a hidden cursor; fix native Wayland display scaling and coverage of KDE panels.
- Add cross-platform build and release automation.
- **v1.0.1:** remove SDL's inhibition of automatic macOS screensaver activation.
- **v1.0.2–v1.0.4:** document macOS Gatekeeper behavior, switch installation to a direct `curl` download, and fix pasting the installer into an interactive zsh prompt.

[1.1.1]: https://github.com/mccannex/terminal-rain/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/mccannex/terminal-rain/compare/v1.0.4...v1.1.0
[1.0.0–1.0.4]: https://github.com/mccannex/terminal-rain/tree/v1.0.4
