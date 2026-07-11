# Glyph Rain

A cross-platform rewrite of a classic "digital rain" Matrix screensaver, built on
SDL2 with a platform-agnostic simulation core. Renamed from the original project's
"Matrix Screensaver" to avoid ambiguity with the legacy build it's descended from
(see `legacy/`).

## Platforms

- **Windows**: a real `.scr` screensaver shell (`src/win32/main.cpp`) — installs via
  the standard `/s /c /p /a` command-line convention, appears in the Display Settings
  screensaver picker with a live preview thumbnail, multi-monitor aware.
- **Linux (Fedora/KDE Plasma, Wayland)**: no `.scr`-equivalent convention exists, so
  it hooks into KDE's Power Management → Energy Saving "run script after inactivity"
  action instead. Multi-monitor aware, with glyph size normalized to KDE's per-output
  display scale. See [`platform/linux/AGENT_CONTEXT.md`](platform/linux/AGENT_CONTEXT.md)
  for the full integration story.
- **macOS**: a real `.saver` bundle (`src/macos_saver/GlyphRainView.mm`) — installs by
  copying into `~/Library/Screen Savers`, appears in System Settings' Screen Saver
  picker, built entirely with Command Line Tools (no Xcode.app required). One known
  cosmetic limitation: the picker's small grid thumbnail shows a generic icon rather
  than the shipped preview image, believed to be an OS-level limitation of this macOS
  version's screensaver-hosting architecture. See
  [`platform/macos/AGENT_CONTEXT.md`](platform/macos/AGENT_CONTEXT.md) for the full
  integration story.

## Credits

The falling-code concept and stream simulation logic (stream speed, trail length,
column spawning) trace back to the original **Matrix Screen Saver by Louai Munajim**
(elouai.com), released under CC BY 3.0. The original Win32/GDI source is preserved
unmodified in [`legacy/`](legacy/) for reference.

- Original author: Louai Munajim — http://elouai.com/the_matrix_screensaver.php
- License: [Creative Commons Attribution 3.0 Unported (CC BY 3.0)](https://creativecommons.org/licenses/by/3.0/)
