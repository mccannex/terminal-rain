#!/bin/bash
set -euo pipefail

# Bundled into the macOS release zip alongside "Glyph Rain.saver" (see
# .github/workflows/release.yml's macos job) so a download+double-click
# install works without ever touching Terminal manually.
#
# Why this is needed at all: the .saver bundle is only ad-hoc signed --
# there's no paid Apple Developer Program identity/notarization behind it
# (see platform/macos/AGENT_CONTEXT.md for that decision). A file downloaded
# through a browser gets tagged com.apple.quarantine, and Gatekeeper flatly
# refuses to open a quarantined ad-hoc-signed bundle, surfacing a misleading
# "is damaged" error instead of a real allow/deny prompt. Copying the bundle
# into place and stripping that flag here is what actually fixes it.
#
# This script itself will also be quarantined after download -- double-click
# it once, and if macOS calls it "unidentified developer" rather than
# running it, right-click -> Open once to authorize it (a normal, working
# Gatekeeper bypass, unlike the .saver's own dead-end "damaged" message).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUNDLE_NAME="Glyph Rain.saver"
SRC="$SCRIPT_DIR/$BUNDLE_NAME"
DEST_DIR="$HOME/Library/Screen Savers"
DEST="$DEST_DIR/$BUNDLE_NAME"

if [ ! -d "$SRC" ]; then
    echo "Error: '$BUNDLE_NAME' not found next to this script." >&2
    exit 1
fi

mkdir -p "$DEST_DIR"
rm -rf "$DEST"
cp -R "$SRC" "$DEST"

# -r: quarantine can be set on files inside the bundle too, not just the
# top-level directory. || true: nothing to strip is not an error.
xattr -dr com.apple.quarantine "$DEST" 2>/dev/null || true

echo "Installed Glyph Rain to: $DEST"
echo "Open System Settings -> Screen Saver and select \"Glyph Rain\"."
