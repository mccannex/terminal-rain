## What's new in v1.1.1

- **Prevent duplicate fullscreen launches on Windows and Linux.** A second launch exits without opening another set of windows. Ownership releases after normal exit or a crash.
- **More consistent rain density.** Stream arrivals scale with each display's glyph-column count, preserving the denser 720p look on larger surfaces. Glyph sizes, speeds, and trail lengths are unchanged.
- **Preserve macOS Settings/fullscreen coexistence.** macOS retains its established host lifecycle and background cleanup; duplicate protection is limited to Windows and Linux.
- **Expanded regression coverage.** New process-ownership tests and a macOS bundle test cover simultaneous Settings/fullscreen views and stop/restart behavior.

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
