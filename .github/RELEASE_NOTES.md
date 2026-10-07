## What's new in v1.1.0

- **Terminal Rain replaces Glyph Rain.** Updated names, documentation, and an
  animated preview using the new palette.
- **Less rendering work.** Invisible glyphs and erase cells are skipped before
  batching and sorting, with visible trails and animation timing preserved.
  Release builds now explicitly enable compiler optimizations on every platform.
- **Display sleep works normally.** Windows and Linux no longer inhibit the
  configured display-off timeout while the screensaver runs.
- **More reliable cleanup.** Corrected Windows preview resource teardown and
  added clean shutdown on rendering failures and renderer-reset notifications.
  macOS background cleanup now completes even during continuous input.
- **Better previews and display support.** Improved Windows settings-preview
  density, display-area scaling, and universal macOS builds for Apple Silicon
  and Intel. Intel hardware remains untested.
- **Refined colors.** A tuned shared palette with explicit sRGB tagging for macOS
  Metal output and the animated preview.
- **Expanded regression coverage.** Automated rendering and cleanup checks,
  supplemented by native Windows, Linux Wayland, and Apple Silicon verification.
  Actual GPU resource loss and Linux X11 remain untested.

## Installing

- **Windows**: download `Terminal Rain.scr`. Right-click it and choose **Install**, or
  double-click to preview it first.
- **macOS 26 or later**: paste a short script into Terminal — see below. Don't download the zip
  through your browser first; that's what causes the "damaged" error some people hit.
  The bundle is universal (Apple Silicon and Intel), but the Intel build is untested
  on real hardware.
- **Linux (KDE Plasma)**: `terminal-rain` is a bare executable, not an installer yet — see
  [`platform/linux/AGENT_CONTEXT.md`](https://github.com/mccannex/terminal-rain/blob/main/platform/linux/AGENT_CONTEXT.md)
  for how it's wired into KDE's Power Management "run script" idle action. Tested on Wayland.

### Windows troubleshooting

Window-position utilities such as PersistentWindows can relocate the screensaver
windows, leaving a monitor uncovered or causing an immediate exit through synthetic
mouse movement. If this happens, pause the utility's automatic restoration to confirm
the conflict, then exclude `Terminal Rain.scr` from restoration. PersistentWindows
supports the `-ignore_process` option; see its
[documentation](https://github.com/kangyu-california/PersistentWindows/blob/master/Help.md).

### macOS install

**Why not just download the zip normally?** This build isn't notarized by Apple
(notarization requires a paid $99/year Developer Program membership, deliberately
not used for this project). A file downloaded through a browser gets tagged with a
`com.apple.quarantine` flag, and Gatekeeper refuses to open anything quarantined
that isn't signed with a real Apple Developer identity — showing a misleading
"is damaged and can't be opened" error. Nothing is actually broken; that's
Gatekeeper rejecting the quarantine flag itself, not a corrupt file.

`curl` never sets that flag — only browsers do, as part of their own
download-safety UI — so fetching the release this way avoids the problem
entirely instead of needing to work around it afterward.

Open **Terminal** (Applications → Utilities → Terminal, or Spotlight search for
"Terminal"), paste the block below as-is, and press Return. It's wrapped in
`bash <<'INSTALL_TERMINAL_RAIN' ... INSTALL_TERMINAL_RAIN` so it runs correctly no
matter what shell or settings your Terminal happens to have (some default zsh
setups don't treat `#` as a comment when text is pasted directly into an
interactive prompt, which otherwise breaks a script like this one) — the
block is still just plain shell underneath, and the comments (the `#` lines)
explain what each step is doing before you run it:

```bash
bash <<'INSTALL_TERMINAL_RAIN'
# Stop immediately if any step below fails, rather than pressing on with a
# broken/partial install.
set -e

# Make a scratch directory to download and unzip into, so nothing gets left
# behind afterward.
d=$(mktemp -d)

# Download the latest macOS build directly from this GitHub repo's Releases.
# Using curl (not a browser) is the whole point -- see the explanation above.
curl -fsSL https://github.com/mccannex/terminal-rain/releases/latest/download/Terminal.Rain-macOS.zip -o "$d/terminalrain.zip"

# Unzip it. ditto (Apple's own archive tool) is used instead of unzip because
# it correctly preserves the .saver bundle's code signature and internal
# structure.
ditto -x -k "$d/terminalrain.zip" "$d"

# macOS scans ~/Library/Screen Savers for installed screensavers -- create it
# if it doesn't already exist, then copy the bundle there.
mkdir -p ~/Library/"Screen Savers"
cp -R "$d/Terminal Rain.saver" ~/Library/"Screen Savers"/

# Clean up the scratch directory now that the bundle's been copied out of it.
rm -rf "$d"

echo "Installed -- open System Settings > Screen Saver and select Terminal Rain."
INSTALL_TERMINAL_RAIN
```

Then open **System Settings → Screen Saver** and select "Terminal Rain."
