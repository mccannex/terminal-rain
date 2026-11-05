"""Exercise the documented installer transaction without touching a real user profile."""
import os
from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / 'tools/macos/install.sh'
MOCK = r'''#!/usr/bin/env python3
import os, pathlib, shutil, sys
name=pathlib.Path(sys.argv[0]).name
args=sys.argv[1:]
faults=os.environ.get('INSTALL_FAULT','').split(',')
def fail(point):
 if point in faults: sys.exit(1)
if name=='curl':
 fail('download'); pathlib.Path(args[args.index('-o')+1]).write_text('zip fixture')
elif name=='ditto':
 if args[0]=='-x':
  fail('extract'); shutil.copytree(os.environ['INSTALL_FIXTURE'],pathlib.Path(args[-1])/'Terminal Rain.saver')
 else:
  fail('copy'); shutil.copytree(args[0],args[1],symlinks=True)
elif name=='codesign':
 target=pathlib.Path(args[-1])
 installed=pathlib.Path(os.environ['HOME'])/'Library/Screen Savers/Terminal Rain.saver'
 point='verify-installed' if target==installed else ('verify-stage' if target.name=='new.saver' else 'verify-source')
 fail(point)
 if not (target/'new-resource').exists(): sys.exit(1)
elif name=='mv':
 if pathlib.Path(args[0]).name=='new.saver': fail('move-new')
 if pathlib.Path(args[0]).name=='previous.saver': fail('restore')
 shutil.move(args[0],args[1])
'''


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.home = self.root / 'home'
        self.home.mkdir()
        self.target = self.home / 'Library/Screen Savers/Terminal Rain.saver'
        self.fixture = self.root / 'fixture.saver'
        self.fixture.mkdir()
        (self.fixture / 'new-resource').write_text('new')
        commands = self.root / 'bin'
        commands.mkdir()
        for name in ['curl', 'ditto', 'codesign', 'mv']:
            command = commands / name
            command.write_text(MOCK)
            command.chmod(0o755)
        self.env = dict(os.environ, HOME=str(self.home), INSTALL_FIXTURE=str(self.fixture),
                        PATH=str(commands)+os.pathsep+os.environ['PATH'], TMPDIR=str(self.root))

    def old_install(self):
        self.target.mkdir(parents=True)
        (self.target / 'obsolete-resource').write_text('old')

    def run_install(self, fault=''):
        return subprocess.run(['bash'], input=INSTALLER.read_text(),
                              env=dict(self.env, INSTALL_FAULT=fault),
                              capture_output=True, text=True, timeout=15)

    def test_fresh_install(self):
        self.assertEqual(self.run_install().returncode, 0)
        self.assertTrue((self.target / 'new-resource').exists())
        self.assertFalse(list(self.target.parent.glob('.terminal-rain-install.*')))

    def test_upgrade_removes_obsolete_resources(self):
        self.old_install()
        self.assertEqual(self.run_install().returncode, 0)
        self.assertTrue((self.target / 'new-resource').exists())
        self.assertFalse((self.target / 'obsolete-resource').exists())
        self.assertFalse(list(self.target.parent.glob('.terminal-rain-install.*')))

    def test_failures_preserve_or_restore_old_install(self):
        self.old_install()
        for fault in ['download', 'extract', 'verify-source', 'copy', 'verify-stage',
                      'move-new', 'verify-installed']:
            with self.subTest(fault=fault):
                result = self.run_install(fault)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual((self.target / 'obsolete-resource').read_text(), 'old')
                self.assertFalse((self.target / 'new-resource').exists())
                self.assertFalse(list(self.target.parent.glob('.terminal-rain-install.*')))

    def test_failed_fresh_install_leaves_no_bundle(self):
        self.assertNotEqual(self.run_install('verify-installed').returncode, 0)
        self.assertFalse(self.target.exists())

    def test_restore_failure_retains_backup_and_reports_path(self):
        self.old_install()
        result = self.run_install('verify-installed,restore')
        self.assertNotEqual(result.returncode, 0)
        backups = list(self.target.parent.glob('.terminal-rain-install.*/previous.saver'))
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / 'obsolete-resource').read_text(), 'old')
        self.assertIn(str(backups[0]), result.stderr)

    def test_release_notes_embed_exact_installer(self):
        notes = (ROOT / '.github/RELEASE_NOTES.md').read_text()
        embedded = notes.split("   bash <<'INSTALL_TERMINAL_RAIN'\n", 1)[1].split('   INSTALL_TERMINAL_RAIN', 1)[0]
        self.assertEqual(textwrap.dedent(embedded), INSTALLER.read_text().split('\n', 1)[1])


if __name__ == '__main__':
    unittest.main()
