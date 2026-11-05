See the [changelog](https://github.com/mccannex/terminal-rain/blob/v1.1.1/CHANGELOG.md) for release history, or download `CHANGELOG.md` from the assets below.

## License documents

Download `Terminal.Rain-Licenses.zip` alongside your platform's executable or screensaver bundle. It contains the MIT license for original project contributions and the preserved third-party notices and license texts. Extract it and keep those documents with redistributed copies. The inherited simulation, SDL2 and font retain their respective terms; see [third-party notices](https://github.com/mccannex/terminal-rain/blob/v1.1.1/THIRD_PARTY_NOTICES.md).

## Installing

### Windows

The Windows x64 build requires the [Microsoft Visual C++ v14 Redistributable (x64)](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/). Install it if it is not already present, or if Windows reports a missing `VCRUNTIME140` or `MSVCP140` DLL.

1. Download `Terminal.Rain.scr` from the assets below.
2. Rename it to `Terminal Rain.scr` so the screensaver picker shows the intended name.
3. Right-click it and choose **Install**. You can also double-click it to preview before installing.

### macOS 26 or later

> **Use Terminal to install.** Downloading the ZIP through a browser adds a quarantine flag. Because this screensaver isn't notarized by Apple, Gatekeeper blocks it with a "damaged" or verification error. The script below downloads it with `curl`, which avoids that flag. Expand the installer below and review its comments before running it. It installs into your account without administrator access; its signature checks verify integrity, not Apple notarization or publisher identity.

The bundle supports Apple Silicon and Intel. Intel hardware remains untested. The installer downloads v1.1.1, checks its signature, and replaces the existing bundle without merging files. If installation fails, it restores the previous copy; after success, it removes the temporary backup. Signature checks confirm bundle integrity, not Apple notarization. The download becomes available when this release is published.

1. Close **System Settings** and dismiss any running screensaver, then open **Terminal**.
2. Expand the installer, review it, then paste the entire block into Terminal and press Return.

   <details>
   <summary>Show macOS install / upgrade script</summary>

   ```bash
   bash <<'INSTALL_TERMINAL_RAIN'
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
   INSTALL_TERMINAL_RAIN
   ```

   </details>

3. Open **System Settings > Screen Saver** and select **Terminal Rain**.

### Linux (KDE Plasma)

1. Download `terminal-rain` from the assets below and place it at a permanent location.
2. Make it executable with `chmod +x /path/to/terminal-rain`, substituting its actual path.
3. Configure KDE's Power Management "run script" idle action using the [Linux integration instructions](https://github.com/mccannex/terminal-rain/blob/main/platform/linux/AGENT_CONTEXT.md).

This is a standalone executable, with no installer. Tested on Wayland; X11 remains untested.

### Windows troubleshooting

Window-position utilities such as PersistentWindows can move the screensaver windows, leaving a monitor uncovered or causing an immediate exit. Pause automatic restoration to check for this conflict, then exclude `Terminal Rain.scr` from restoration. For PersistentWindows, use `-ignore_process "Terminal Rain.scr"`; see its [documentation](https://github.com/kangyu-california/PersistentWindows/blob/master/Help.md).
