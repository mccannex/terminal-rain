See the [changelog](https://github.com/mccannex/terminal-rain/blob/main/CHANGELOG.md)
for release history, or download `CHANGELOG.md` from the assets below.

## License documents

Download `Terminal.Rain-Licenses.zip` alongside your platform's executable or
screensaver bundle. It contains the MIT license for original project contributions
and the preserved third-party notices and license texts. Extract it and keep
those documents with redistributed copies. The inherited simulation, SDL2 and
font retain their respective terms; see
[third-party notices](https://github.com/mccannex/terminal-rain/blob/main/THIRD_PARTY_NOTICES.md).

## Installing

### Windows

1. Download `Terminal.Rain.scr` from the assets below.
2. Rename it to `Terminal Rain.scr` so the screensaver picker shows the intended name.
3. Right-click it and choose **Install**. You can also double-click it to preview before installing.

### macOS 26 or later

> **Use Terminal to install.** Downloading the ZIP through a browser adds a quarantine flag. Because this screensaver isn't notarized by Apple, Gatekeeper blocks it with a "damaged" or verification error. The script below downloads it with `curl`, which avoids that flag.
> Read the script comments in the following code block to understand what it's doing and why this is safe.

The bundle supports Apple Silicon and Intel. Intel hardware remains untested.

1. Open **Terminal**.
2. Paste this entire block and press Return:

   ```bash
   bash <<'INSTALL_TERMINAL_RAIN'
   # Stop if a step fails.
   set -e

   # Use a temporary folder and remove it when finished.
   tmp=$(mktemp -d)
   trap 'rm -rf "$tmp"' EXIT

   # Download from this GitHub repo and install for your user account.
   repo=https://github.com/mccannex/terminal-rain
   asset=Terminal.Rain-macOS.zip
   dest="$HOME/Library/Screen Savers"

   # Download the latest screensaver without the browser quarantine flag.
   curl -fsSL "$repo/releases/latest/download/$asset" -o "$tmp/saver.zip"

   # Unpack the downloaded ZIP.
   ditto -x -k "$tmp/saver.zip" "$tmp"

   # Create your screensaver folder and copy Terminal Rain into it.
   mkdir -p "$dest"
   cp -R "$tmp/Terminal Rain.saver" "$dest/"
   echo "Installed Terminal Rain."
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
