#!/bin/bash
# Close System Settings and dismiss any running screensaver before installing.
set -euo pipefail

repo=https://github.com/mccannex/terminal-rain
version=v1.1.1
dest="$HOME/Library/Screen Savers"
target="$dest/Terminal Rain.saver"
tmp=$(mktemp -d)
work=
placed=0
complete=0

cleanup() {
    status=$?
    trap - EXIT
    set +e
    keep=0
    if [ "$complete" -eq 0 ] && [ -n "$work" ]; then
        if [ "$placed" -eq 1 ]; then
            rm -rf "$target"
        fi
        if [ -e "$work/previous.saver" ] || [ -L "$work/previous.saver" ]; then
            if [ ! -e "$target" ] && [ ! -L "$target" ] && mv "$work/previous.saver" "$target"; then
                echo "Installation failed; restored the previous screensaver." >&2
            else
                echo "Previous screensaver preserved at: $work/previous.saver" >&2
                keep=1
            fi
        fi
    fi
    rm -rf "$tmp"
    if [ -n "$work" ] && [ "$keep" -eq 0 ]; then
        rm -rf "$work"
    fi
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Download this release directly, without a browser quarantine attribute.
curl -fsSL "$repo/releases/download/$version/Terminal.Rain-macOS.zip" -o "$tmp/saver.zip"
ditto -x -k "$tmp/saver.zip" "$tmp"
source_bundle="$tmp/Terminal Rain.saver"

# Check bundle integrity before changing the installed copy.
# This checks the ad-hoc signature; it is not Apple notarization.
codesign --verify --deep --strict "$source_bundle"
mkdir -p "$dest"
work=$(mktemp -d "$dest/.terminal-rain-install.XXXXXX")
ditto "$source_bundle" "$work/new.saver"
codesign --verify --deep --strict "$work/new.saver"

# Move the old bundle aside rather than merging its contents with the new one.
if [ -e "$target" ] || [ -L "$target" ]; then
    mv "$target" "$work/previous.saver"
fi
mv "$work/new.saver" "$target"
placed=1

# A failed installed-signature check restores the previous bundle automatically.
codesign --verify --deep --strict "$target"
complete=1
echo "Installed Terminal Rain $version. Open System Settings > Screen Saver to select it."
