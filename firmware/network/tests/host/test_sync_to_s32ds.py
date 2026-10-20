"""Refusal and destination rules of tools/sync_to_s32ds.py on fake S32DS projects.

Three fake projects in a temporary folder (never a real S32DS workspace):
  bench-like   .cproject + src/
  VSS-like     .cproject + src/app/vss_main.c (+ a leftover src/net_bench file)
  bring-up     a folder named network_344_can_bringup
The script's manifests go to the same temporary folder (TMP/TEMP/TMPDIR).
"""
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

NETWORK = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = NETWORK / 'tools' / 'sync_to_s32ds.py'


def tree(root):
    return sorted(str(p.relative_to(root)).replace('\\', '/') for p in root.rglob('*') if p.is_file())


class SyncRules(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sync_rules_')
        self.root = pathlib.Path(self.tmp.name)
        self.env = dict(os.environ, TMP=str(self.root), TEMP=str(self.root), TMPDIR=str(self.root))
        self.bench = self.make('fake_bench')
        self.vss = self.make('fake_vss', 'src/app/vss_main.c', 'src/net_bench/old_bench.c')
        self.bringup = self.make('network_344_can_bringup')

    def tearDown(self):
        self.tmp.cleanup()

    def make(self, name, *files):
        p = self.root / 'ws' / name
        (p / 'src').mkdir(parents=True)
        (p / '.cproject').write_text('<cproject/>\n', encoding='utf-8')
        for f in files:
            (p / f).parent.mkdir(parents=True, exist_ok=True)
            (p / f).write_text('/* fake */\n', encoding='utf-8')
        return p

    def run_sync(self, project, *extra):
        r = subprocess.run([sys.executable, '-B', str(SCRIPT), '--project', str(project), *extra],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=self.env)
        return r.returncode, r.stdout

    def test_bringup_refused_for_every_board(self):
        before = tree(self.bringup)
        for board in ('bench', 'vss', 'none'):
            rc, out = self.run_sync(self.bringup, '--board', board, '--apply')
            self.assertEqual(rc, 1, out)
            self.assertIn('REFUSED', out)
        self.assertEqual(tree(self.bringup), before)

    def test_bench_board_on_vss_project_refused(self):
        before = tree(self.vss)
        rc, out = self.run_sync(self.vss, '--board', 'bench', '--apply')
        self.assertEqual(rc, 1, out)
        self.assertIn('REFUSED', out)
        rc, out = self.run_sync(self.vss, '--apply')  # default board is bench
        self.assertEqual(rc, 1, out)
        self.assertEqual(tree(self.vss), before)

    def test_vss_board_on_bench_project_refused(self):
        before = tree(self.bench)
        rc, out = self.run_sync(self.bench, '--board', 'vss', '--apply')
        self.assertEqual(rc, 1, out)
        self.assertIn('REFUSED', out)
        self.assertEqual(tree(self.bench), before)

    def test_dry_run_writes_nothing(self):
        for project, board in ((self.bench, 'bench'), (self.vss, 'vss'), (self.vss, 'none')):
            before = tree(project)
            rc, out = self.run_sync(project, '--board', board)
            self.assertEqual(rc, 0, out)
            self.assertIn('DRY RUN', out)
            self.assertEqual(tree(project), before)

    def test_vss_apply_destinations(self):
        rc, out = self.run_sync(self.vss, '--board', 'vss', '--apply')
        self.assertEqual(rc, 0, out)
        files = tree(self.vss)
        self.assertIn('src/net_board/net_vss_board.c', files)
        self.assertIn('src/network/src/node/net_entry.c', files)
        self.assertIn('src/network/include/network/app/net_if_vss.h', files)
        self.assertNotIn('src/network/include/network/net_domain_c2.h', files)
        self.assertFalse(any(f.startswith('src/net_bench/') and f != 'src/net_bench/old_bench.c' for f in files))
        self.assertFalse(any('sdk_mock' in f or f.endswith(('FlexCAN_Ip.h', 'OsIf.h', 'Std_Types.h'))
                             for f in files), files)
        # The leftover bench folder is reported as another board folder, never deleted.
        self.assertIn('src/app/vss_main.c', files)
        self.assertIn('src/net_bench/old_bench.c', files)
        self.assertIn('net_bench/old_bench.c  (board folder of another kind)', out)

    def test_bench_apply_and_none(self):
        rc, out = self.run_sync(self.bench, '--board', 'none', '--apply')
        self.assertEqual(rc, 0, out)
        self.assertFalse(any(f.startswith(('src/net_bench/', 'src/net_board/')) for f in tree(self.bench)))
        rc, out = self.run_sync(self.bench, '--board', 'bench', '--apply')
        self.assertEqual(rc, 0, out)
        files = tree(self.bench)
        self.assertIn('src/net_bench/net_board_time.c', files)
        self.assertFalse(any(f.startswith('src/net_board/') for f in files))


if __name__ == '__main__':
    unittest.main(verbosity=2)
