## Installing

- **Windows**: download `Glyph Rain.scr`. Right-click it and choose **Install**, or
  double-click to preview it first.
- **macOS**: download `Glyph Rain-macOS.zip`, unzip it, then **run
  `Install Glyph Rain.command`** (double-click it). Don't just drag
  `Glyph Rain.saver` into `~/Library/Screen Savers` yourself — see why below.
- **Linux**: `glyph_rain_dev` is the raw dev binary, not an installer yet — see
  [`platform/linux/AGENT_CONTEXT.md`](https://github.com/mccannex/glyph-rain/blob/main/platform/linux/AGENT_CONTEXT.md)
  for how it's wired into KDE's Power Management idle-script hook.

### A note for macOS users: why the extra script?

This build isn't notarized by Apple — that requires a paid $99/year Developer Program
membership, which this project doesn't use. If you try to install `Glyph Rain.saver`
directly (drag it into `~/Library/Screen Savers` yourself), macOS will refuse to open
it and show a misleading error:

> "Glyph Rain.saver" is damaged and can't be opened. You should move it to the Trash.

Nothing is actually broken. That's Gatekeeper rejecting an unsigned bundle that was
downloaded from the internet — not real file corruption.

`Install Glyph Rain.command` fixes this for you: it copies the bundle into
`~/Library/Screen Savers` and clears the specific flag that's actually causing the
problem. Two things to expect:

1. The **first time** you double-click the script itself, macOS will probably also
   flag *it* as being from an "unidentified developer." Right-click the script and
   choose **Open** (instead of double-clicking) to get past that one-time prompt —
   after that it runs normally.
2. Once it finishes, open **System Settings → Screen Saver** and select "Glyph Rain"
   from the picker.
