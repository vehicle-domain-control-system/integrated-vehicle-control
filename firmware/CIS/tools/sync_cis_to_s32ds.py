"""Put the Network sources and the CIS application into the CIS S32DS project.

The Network team's own tool does the Network part (their CIS bench for the S32K144,
nodes/cis/bench/s32k144_cis); this script runs it with --board cis144 and adds the CIS side:

  1. firmware/network/tools/sync_to_s32ds.py --board cis144   (Network sources -> src/network,
     bench clock and node hook -> src/net_bench)
  2. removes src/net_bench/cis_bench_app.c|h   (the bench stand-in defines the same CisApp_On* and
     publish helpers as the real CIS application)
  3. copies firmware/CIS/src -> src/cis   (not main.c, pi_uart.c, net_api_shim.h)

Default is a dry run. The Network checkout is only read.

Example:
  python firmware/CIS/tools/sync_cis_to_s32ds.py --network C:/ws/NET_git/firmware/network --project C:/ws/network_144_net_cis
  python firmware/CIS/tools/sync_cis_to_s32ds.py --network C:/ws/NET_git/firmware/network --project C:/ws/network_144_net_cis --apply
"""
from __future__ import annotations
import argparse, hashlib, pathlib, shutil, subprocess, sys

CIS_SRC = pathlib.Path(__file__).resolve().parents[1] / 'src'
SKIP = {'main.c', 'pi_uart.c', 'net_api_shim.h'}
BENCH_APP = ('cis_bench_app.c', 'cis_bench_app.h')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    p.add_argument('--network', type=pathlib.Path, required=True, help='checkout of firmware/network (read only)')
    p.add_argument('--project', type=pathlib.Path, required=True, help='CIS S32DS project (a copy of the 144 debug node)')
    p.add_argument('--apply', action='store_true', help='copy (default: dry run)')
    args = p.parse_args()

    tool = args.network / 'tools' / 'sync_to_s32ds.py'
    if not tool.is_file():
        print('ERROR: %s not found (is --network the firmware/network folder?)' % tool)
        return 2
    cmd = [sys.executable, str(tool), '--project', str(args.project), '--board', 'cis144']
    if args.apply:
        cmd.append('--apply')
    print('== Network part: ' + ' '.join(cmd))
    rc = subprocess.call(cmd)
    if rc != 0:
        print('The Network tool refused (exit %d); nothing of the CIS part was done.' % rc)
        return rc

    bench = args.project / 'src' / 'net_bench'
    dest = args.project / 'src' / 'cis'
    print('== CIS part: %s -> %s' % (CIS_SRC, dest))
    for name in BENCH_APP:
        f = bench / name
        if f.is_file() or not args.apply:
            print('  %s %s' % ('remove' if args.apply else 'would remove', f))
            if args.apply and f.is_file():
                f.unlink()
    new = changed = same = 0
    for f in sorted(CIS_SRC.iterdir()):
        if not f.is_file() or f.suffix not in ('.c', '.h') or f.name in SKIP:
            continue
        d = dest / f.name
        if not d.is_file():
            action = 'new'
            new += 1
        elif sha(d) != sha(f):
            action = 'changed'
            changed += 1
        else:
            action = 'unchanged'
            same += 1
        if action != 'unchanged':
            print('  %-8s cis/%s' % (action, f.name))
            if args.apply:
                dest.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(f, d)
    print('  new %d, changed %d, unchanged %d' % (new, changed, same))
    print('done.' if args.apply else 'dry run: nothing written (use --apply).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
