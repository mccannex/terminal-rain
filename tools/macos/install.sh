#!/bin/bash
# Run after closing System Settings and dismissing any running screensaver.
# This installs only for your account; it does not use sudo or change settings.
# Review the download source and destination below before running it.

# Stop on command errors, unset variables, or a failed pipeline.
set -euo pipefail

# Download only the named release from this project's GitHub repository.
# The installed bundle is limited to your personal Screen Savers folder.
repo=https://github.com/mccannex/terminal-rain
version=v1.1.1
dest="$HOME/Library/Screen Savers"
target="$dest/Terminal Rain.saver"
# Create a temporary download folder. A separate staging folder is created later.
# These flags tell cleanup whether a replacement needs to be undone.
tmp=$(mktemp -d)
work=
placed=0
complete=0

# Run on exit, including errors and interruption. Preserve the original status
# and allow recovery commands to continue even if one cleanup command fails.
cleanup() {
    status=$?
    trap - EXIT
    set +e
    keep=0
    # If installation did not finish, remove only the replacement we placed.
    # The previous bundle, if any, is kept separately until verification succeeds.
    if [ "$complete" -eq 0 ] && [ -n "$work" ]; then
        if [ "$placed" -eq 1 ]; then
            rm -rf "$target"
        fi
        # Restore the previous bundle to its original location. If that fails,
        # keep its backup and print the exact path instead of deleting it.
        if [ -e "$work/previous.saver" ] || [ -L "$work/previous.saver" ]; then
            if [ ! -e "$target" ] && [ ! -L "$target" ] && mv "$work/previous.saver" "$target"; then
                echo "Installation failed; restored the previous screensaver." >&2
            else
                echo "Previous screensaver preserved at: $work/previous.saver" >&2
                keep=1
            fi
        fi
    fi
    # Delete our temporary downloads and staging files. After success, this also
    # deletes the old backup. A backup needed for manual recovery is retained.
    rm -rf "$tmp"
    if [ -n "$work" ] && [ "$keep" -eq 0 ]; then
        rm -rf "$work"
    fi
    exit "$status"
}
# Always run cleanup; Ctrl-C and termination also take the recovery path.
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Download the ZIP over HTTPS, then unpack it into our temporary folder.
# This does not execute any downloaded installer script. curl avoids the browser's
# quarantine attribute; it does not notarize the screensaver or prove it is safe.
curl -fsSL "$repo/releases/download/$version/Terminal.Rain-macOS.zip" -o "$tmp/saver.zip"
ditto -x -k "$tmp/saver.zip" "$tmp"
source_bundle="$tmp/Terminal Rain.saver"

# Verify that the extracted bundle matches its embedded ad-hoc code signature.
# This detects damaged/modified bundle contents, not a trusted publisher identity
# or Apple notarization. A failure stops here, leaving the installed copy intact.
codesign --verify --deep --strict "$source_bundle"
# Stage a separate copy beside the destination and verify it too. ditto preserves
# Mac bundle metadata. The current installation has not been moved or changed.
mkdir -p "$dest"
work=$(mktemp -d "$dest/.terminal-rain-install.XXXXXX")
ditto "$source_bundle" "$work/new.saver"
codesign --verify --deep --strict "$work/new.saver"

# Keep the existing bundle as a temporary backup, then move the staged bundle
# into place. No old resources are merged into the new installation.
if [ -e "$target" ] || [ -L "$target" ]; then
    mv "$target" "$work/previous.saver"
fi
mv "$work/new.saver" "$target"
placed=1

# Verify the final installed copy before declaring success. A failure invokes
# cleanup to remove the replacement and restore the backup if possible.
# On success, cleanup removes the temporary backup and download/staging folders.
codesign --verify --deep --strict "$target"
complete=1
echo "Installed Terminal Rain $version. Open System Settings > Screen Saver to select it."
