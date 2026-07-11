## Installing

- **Windows**: download `Glyph Rain.scr`. Right-click it and choose **Install**, or
  double-click to preview it first.
- **macOS**: paste a short script into Terminal — see below. Don't download the zip
  through your browser first; that's what causes the "damaged" error some people hit.
- **Linux**: `glyph_rain_dev` is the raw dev binary, not an installer yet — see
  [`platform/linux/AGENT_CONTEXT.md`](https://github.com/mccannex/glyph-rain/blob/main/platform/linux/AGENT_CONTEXT.md)
  for how it's wired into KDE's Power Management idle-script hook.

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
`bash <<'INSTALL_GLYPH_RAIN' ... INSTALL_GLYPH_RAIN` so it runs correctly no
matter what shell or settings your Terminal happens to have (some default zsh
setups don't treat `#` as a comment when text is pasted directly into an
interactive prompt, which otherwise breaks a script like this one) — the
block is still just plain shell underneath, and the comments (the `#` lines)
explain what each step is doing before you run it:

```bash
bash <<'INSTALL_GLYPH_RAIN'
# Stop immediately if any step below fails, rather than pressing on with a
# broken/partial install.
set -e

# Make a scratch directory to download and unzip into, so nothing gets left
# behind afterward.
d=$(mktemp -d)

# Download the latest macOS build directly from this GitHub repo's Releases.
# Using curl (not a browser) is the whole point -- see the explanation above.
curl -fsSL https://github.com/mccannex/glyph-rain/releases/latest/download/Glyph.Rain-macOS.zip -o "$d/glyphrain.zip"

# Unzip it. ditto (Apple's own archive tool) is used instead of unzip because
# it correctly preserves the .saver bundle's code signature and internal
# structure.
ditto -x -k "$d/glyphrain.zip" "$d"

# macOS scans ~/Library/Screen Savers for installed screensavers -- create it
# if it doesn't already exist, then copy the bundle there.
mkdir -p ~/Library/"Screen Savers"
cp -R "$d/Glyph Rain.saver" ~/Library/"Screen Savers"/

# Clean up the scratch directory now that the bundle's been copied out of it.
rm -rf "$d"

echo "Installed -- open System Settings > Screen Saver and select Glyph Rain."
INSTALL_GLYPH_RAIN
```

Then open **System Settings → Screen Saver** and select "Glyph Rain."
