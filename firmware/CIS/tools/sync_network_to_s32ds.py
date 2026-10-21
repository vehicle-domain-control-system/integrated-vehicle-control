"""Copy the Network product sources and the CIS board files into the CIS S32DS project.

The Network sources are read from a checkout of firmware/network (branch feat/vss-network-node,
owned by the Network team; it is only read). The file list is tools/s32ds_sources.txt of that
checkout. Nothing is copied from sdk_mock, tests, tools, host_* or the s32k144 scaffold, nor any
RTD-named stand-in header (these would shadow the real RTD headers of the project).

  <network>/<listed .c files>, include/network/**, the *.h of the listed folders
        -> <project>/src/network/<same path>
  firmware/CIS/board/*.c|*.h
        -> <project>/src/net_board/

Default is a dry run. --apply copies. Files already in the destination that are not in the
bundle are reported and left in place.

Exit: 0 ok, 1 refused, 2 usage / missing input.
Example:
  python firmware/CIS/tools/sync_network_to_s32ds.py --network C:/ws/NET_git/firmware/network --project C:/ws/CIS_RTD
  python firmware/CIS/tools/sync_network_to_s32ds.py --network C:/ws/NET_git/firmware/network --project C:/ws/CIS_RTD --apply
"""
from __future__ import annotations
import argparse, hashlib, pathlib, re, shutil, sys

BOARD_DIR = pathlib.Path(__file__).resolve().parents[1] / 'board'
EXCLUDED_HEADERS = ('include/network/net_host_mock.h', 'include/network/net_virtual.h',
                    'include/network/net_domain_c2.h')
FORBIDDEN_PARTS = ('sdk_mock', 'tests', 'tools', 's32k144')
STAND_IN_NAMES = ('flexcan_ip.h', 'lpuart_uart_ip.h', 'osif.h', 'std_types.h')


def fwd(p):
    return str(p).replace('\\', '/')


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for chunk in iter(lambda: fh.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def forbidden(rel):
    parts = rel.lower().split('/')
    name = parts[-1]
    for part in parts[:-1]:
        if part in FORBIDDEN_PARTS or part.startswith('host_'):
            return 'forbidden folder %s' % part
    if name in STAND_IN_NAMES or re.search(r'_sa_pbcfg\.[ch]$', name):
        return 'RTD-named stand-in'
    if not name.endswith(('.c', '.h')):
        return 'not a .c/.h file'
    return None


def plan(network, project):
    listing = network / 'tools' / 's32ds_sources.txt'
    if not listing.is_file():
        raise FileNotFoundError(fwd(listing))
    rels, refused = set(), []
    for line in listing.read_text(encoding='utf-8').splitlines():
        line = line.split('#', 1)[0].strip().replace('\\', '/')
        if not line:
            continue
        if not (network / line).is_file():
            refused.append('%s: listed but missing' % line)
        else:
            rels.add(line)
    for folder in sorted({r.rsplit('/', 1)[0] for r in rels if '/' in r}):
        for h in sorted((network / folder).glob('*.h')):
            rels.add(fwd(h.relative_to(network)))
    for h in sorted((network / 'include' / 'network').rglob('*.h')):
        rel = fwd(h.relative_to(network))
        if rel not in EXCLUDED_HEADERS:
            rels.add(rel)
    items = []
    net_dst = project / 'src' / 'network'
    for rel in sorted(rels):
        reason = forbidden(rel)
        if reason:
            refused.append('%s: %s' % (rel, reason))
        else:
            items.append((network / rel, net_dst / rel, 'network/' + rel))
    board_dst = project / 'src' / 'net_board'
    for f in sorted(BOARD_DIR.iterdir()):
        if f.is_file() and f.suffix in ('.c', '.h'):
            items.append((f, board_dst / f.name, 'net_board/' + f.name))
    roots = [net_dst.resolve(), board_dst.resolve()]
    checked = []
    for src, dst, rel in items:
        target = dst.resolve()
        if not any(target == r or r in target.parents for r in roots):
            refused.append('%s: destination outside src/network and src/net_board' % rel)
        else:
            checked.append((src, dst, rel))
    return checked, refused


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    p.add_argument('--network', type=pathlib.Path, required=True, help='checkout of firmware/network (read only)')
    p.add_argument('--project', type=pathlib.Path, required=True, help='CIS S32DS project folder')
    p.add_argument('--apply', action='store_true', help='copy (default: dry run)')
    args = p.parse_args()
    if not (args.project / '.cproject').is_file() or not (args.project / 'src').is_dir():
        print('ERROR: %s is not an S32DS project (no .cproject or src/)' % fwd(args.project))
        return 2
    if not (args.network / 'tools' / 's32ds_sources.txt').is_file():
        print('ERROR: %s is not firmware/network (no tools/s32ds_sources.txt)' % fwd(args.network))
        return 2
    try:
        items, refused = plan(args.network, args.project)
    except FileNotFoundError as e:
        print('ERROR: missing %s' % e)
        return 2
    if refused:
        print('REFUSED (nothing copied):')
        for r in refused:
            print('  ' + r)
        return 1

    counts = {'new': 0, 'changed': 0, 'unchanged': 0}
    rows = []
    for src, dst, rel in items:
        if not dst.is_file():
            action = 'new'
        elif sha256(dst) != sha256(src):
            action = 'changed'
        else:
            action = 'unchanged'
        counts[action] += 1
        rows.append((src, dst, rel, action))
    print('%s: %s -> %s/src/{network,net_board}' % ('APPLY' if args.apply else 'DRY RUN', fwd(args.network), fwd(args.project)))
    for _, _, rel, action in rows:
        if action != 'unchanged':
            print('  %-9s %s' % (action, rel))
    print('  new %d, changed %d, unchanged %d (total %d)' % (counts['new'], counts['changed'], counts['unchanged'], len(rows)))
    wanted = {rel for _, _, rel, _ in rows}
    for sub in ('network', 'net_board'):
        root = args.project / 'src' / sub
        if root.is_dir():
            for f in sorted(root.rglob('*')):
                rel = sub + '/' + fwd(f.relative_to(root))
                if f.is_file() and rel not in wanted:
                    print('  left in place (not in the bundle): %s' % rel)
    if args.apply:
        for src, dst, _, action in rows:
            if action != 'unchanged':
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(src, dst)
        print('copied.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
