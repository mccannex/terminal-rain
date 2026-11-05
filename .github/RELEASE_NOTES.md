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

> **Use Terminal to install.** Downloading the ZIP through a browser adds a quarantine flag. Because this screensaver isn't notarized by Apple, Gatekeeper blocks it with a "damaged" or verification error. The script below downloads it with `curl`, which avoids that flag. Read the script comments in the following code block to understand what it's doing and why this is safe.

The bundle supports Apple Silicon and Intel. Intel hardware remains untested. The installer downloads v1.1.1, checks its signature, and replaces the existing bundle without merging files. If installation fails, it restores the previous copy; after success, it removes the temporary backup. Signature checks confirm bundle integrity, not Apple notarization. The download becomes available when this release is published.

1. Close **System Settings** and dismiss any running screensaver, then open **Terminal**.
2. Paste this entire block and press Return:

   ```bash
   bash <<'INSTALL_TERMINAL_RAIN'
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
   INSTALL_TERMINAL_RAIN
   ```

3. Open **System Settings > Screen Saver** and select **Terminal Rain**.

### Linux (KDE Plasma)

1. Download `terminal-rain` from the assets below and place it at a permanent location.
2. Make it executable with `chmod +x /path/to/terminal-rain`, substituting its actual path.
3. Configure KDE's Power Management "run script" idle action using the [Linux integration instructions](https://github.com/mccannex/terminal-rain/blob/main/platform/linux/AGENT_CONTEXT.md).

This is a standalone executable, with no installer. Tested on Wayland; X11 remains untested.

### Windows troubleshooting

Window-position utilities such as PersistentWindows can move the screensaver windows, leaving a monitor uncovered or causing an immediate exit. Pause automatic restoration to check for this conflict, then exclude `Terminal Rain.scr` from restoration. For PersistentWindows, use `-ignore_process "Terminal Rain.scr"`; see its [documentation](https://github.com/kangyu-california/PersistentWindows/blob/master/Help.md).
