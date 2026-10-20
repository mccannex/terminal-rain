"""Exercise the real Linux entry point without opening a graphics connection."""
import os
import subprocess
import sys
import tempfile

app, helper = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="terminal-rain-integration-") as directory:
    environment = dict(os.environ, XDG_RUNTIME_DIR=directory,
                       SDL_VIDEODRIVER="terminal-rain-intentionally-unavailable")
    holder = subprocess.Popen([helper, "--hold", "session", directory],
                              stdout=subprocess.PIPE, text=True)
    try:
        assert holder.stdout.readline().strip() == "ready"
        for _ in range(8):
            result = subprocess.run([app], env=environment, capture_output=True,
                                    text=True, timeout=3)
            assert result.returncode == 0, result.stderr
            assert "another Terminal Rain presentation" in result.stderr, result.stderr
        assert holder.poll() is None, "Duplicate launch terminated the owner"
    finally:
        holder.kill()
        holder.wait()
        holder.stdout.close()

    result = subprocess.run([app], env=environment, capture_output=True,
                            text=True, timeout=3)
    assert result.returncode == 1 and "SDL_Init failed" in result.stderr, result.stderr
    result = subprocess.run([app], env=dict(environment, XDG_RUNTIME_DIR=directory + "/missing"),
                            capture_output=True, text=True, timeout=3)
    assert result.returncode == 1 and "cannot acquire fullscreen ownership" in result.stderr, result.stderr
