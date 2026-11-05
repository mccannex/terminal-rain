"""Run the actual installer with ditto/codesign against a locally packaged Mac ZIP."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

if sys.platform != 'darwin':
    raise SystemExit('This check requires macOS ditto and codesign')
archive = Path(sys.argv[1]).resolve()
installer = Path(__file__).resolve().parents[1] / 'tools/macos/install.sh'
with tempfile.TemporaryDirectory(prefix='terminal-rain-installer-') as temp:
    root = Path(temp)
    home = root / 'home'
    home.mkdir()
    commands = root / 'bin'
    commands.mkdir()
    curl = commands / 'curl'
    curl.write_text('''#!/bin/bash
set -eu
while [ "$1" != '-o' ]; do shift; done
cp "$INSTALLER_TEST_ZIP" "$2"
''')
    codesign = commands / 'codesign'
    codesign.write_text('''#!/bin/bash
set -eu
for arg; do target=$arg; done
if [ "${INSTALLER_TEST_FAIL:-}" = 1 ] && [ "$target" = "$HOME/Library/Screen Savers/Terminal Rain.saver" ]; then
    exit 1
fi
exec /usr/bin/codesign "$@"
''')
    curl.chmod(0o755)
    codesign.chmod(0o755)
    env = dict(os.environ, HOME=str(home), INSTALLER_TEST_ZIP=str(archive),
               PATH=str(commands)+os.pathsep+os.environ['PATH'])
    target = home / 'Library/Screen Savers/Terminal Rain.saver'
    def install(fail=False):
        result = subprocess.run(['bash', str(installer)],
                                env=dict(env, INSTALLER_TEST_FAIL='1' if fail else '0'),
                                capture_output=True, text=True, timeout=30)
        print(result.stdout, result.stderr, end='')
        assert (result.returncode != 0) == fail, result
    def verify():
        subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(target)], check=True)
    install()
    verify()
    (target / 'obsolete-resource').write_text('must not survive upgrade')
    install()
    assert not (target / 'obsolete-resource').exists()
    verify()
    executable = target / 'Contents/MacOS/Terminal Rain'
    marker = target / 'Contents/Resources/rollback-marker'
    marker.write_text('previous installation')
    subprocess.run(['/usr/bin/codesign', '--force', '--deep', '--sign', '-', str(target)], check=True)
    previous = executable.read_bytes()
    install(fail=True)
    assert executable.read_bytes() == previous
    assert marker.read_text() == 'previous installation'
    verify()
    assert not list(target.parent.glob('.terminal-rain-install.*'))
    print('Native installer fresh install, clean upgrade, and rollback passed.')
