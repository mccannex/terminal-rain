## What's new in v1.1.0

- **Terminal Rain replaces Glyph Rain.** Updated names, documentation, and an animated preview using the new palette.
- **Less rendering work.** Invisible glyphs and erase cells are skipped before batching and sorting, with visible trails and animation timing preserved. Release builds explicitly enable compiler optimizations on every platform.
- **Display sleep works normally.** Windows and Linux allow the configured display-off timeout while the screensaver runs.
- **More reliable cleanup.** Fixed Windows preview resource teardown and added clean shutdown on rendering failures and renderer-reset notifications. macOS background cleanup completes even during continuous input.
- **Better previews and display support.** Improved Windows settings-preview density, display-area scaling, and universal macOS builds for Apple Silicon and Intel. Intel hardware remains untested.
- **Refined colors.** A tuned shared palette with explicit sRGB tagging for macOS Metal output and the animated preview.
- **Expanded regression coverage.** Automated rendering and cleanup checks, supplemented by native Windows, Linux Wayland, and Apple Silicon verification. Actual GPU resource loss and Linux X11 remain untested.

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
