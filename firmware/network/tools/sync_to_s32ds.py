"""Copy the Network product sources and a board folder into an S32DS board project.

Two bundles:
  (1) the C sources of tools/s32ds_sources.txt, include/network/** (except the
      headers no board build uses) and the *.h files of the listed source
      folders -> <project>/src/network/<same path relative to firmware/network>
  (2) the board folder chosen with --board (default bench):
        bench  bench/s32k344/*.c|*.h        -> <project>/src/net_bench/  (S32K344 bench clone)
        vss    board/s32k344_vss/*.c|*.h    -> <project>/src/net_board/  (VSS board project)
        none   nothing (Network sources only)

Default is a dry run: it prints what would be copied (new / changed /
unchanged) and writes nothing into the project. --apply copies.

Refused:
  - a bring-up reference project as --project (folder name
    network_344_can_bringup or network_144_can_debuger): read-only references
  - any source under sdk_mock, tests, tools, host_*, s32k144, any .md / .csv /
    CMake file, and any RTD-named stand-in header (FlexCAN_Ip.h, *_Sa_PBcfg.h ...)
    -- these must never enter an S32DS workspace
  - a destination outside <project>/src/network or the board folder
  - --board bench on a VSS project (src/app/vss_main.c present): the bench
    files define VssApp_On*, SysTick_Handler and Board_* that VSS already has
  - --board vss on a project without src/app/vss_main.c

Files already in the destination folders that are not in the bundles are
reported, never deleted (also a board folder of the other kind). Every run writes a manifest (relative path, SHA-256,
action) to a new folder under the temp directory, never into the project.

Exit: 0 ok, 1 refused or copy error, 2 usage / missing input.
Example:
  python firmware/network/tools/sync_to_s32ds.py --project C:/Users/me/ws/network_344_net_bench
  python firmware/network/tools/sync_to_s32ds.py --project C:/Users/me/ws/network_344_net_bench --apply
  python firmware/network/tools/sync_to_s32ds.py --project C:/Users/me/ws/S32K344_WB_Audio_Bringup --board vss
"""
from __future__ import annotations
import argparse, datetime, hashlib, json, os, pathlib, re, shutil, sys, tempfile

NETWORK = pathlib.Path(__file__).resolve().parents[1]
SOURCES_LIST = NETWORK / 'tools' / 's32ds_sources.txt'
# --board: source folder, destination folder under <project>/src.
BOARDS = {'bench': (NETWORK / 'bench' / 's32k344', 'net_bench'),
          'vss': (NETWORK / 'board' / 's32k344_vss', 'net_board')}
BOARD_DESTS = ('net_bench', 'net_board')
VSS_MARKER = 'src/app/vss_main.c'
REFERENCE_PROJECTS = ('network_344_can_bringup', 'network_144_can_debuger')
# Headers no board build uses: host-only ones, and net_domain_c2.h, which only
# compiles together with the DOMAIN application headers.
EXCLUDED_HEADERS = ('include/network/net_host_mock.h', 'include/network/net_virtual.h',
                    'include/network/net_domain_c2.h')
FORBIDDEN_PARTS = ('sdk_mock', 'tests', 'tools', 's32k144')
FORBIDDEN_SUFFIXES = ('.md', '.csv', '.cmake')
# File names of the mock SDK that shadow real RTD headers.
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
    """Reason a source path (relative to firmware/network) may not be copied, else None."""
    parts = rel.lower().split('/')
    name = parts[-1]
    for part in parts[:-1]:
        if part in FORBIDDEN_PARTS or part.startswith('host_'):
            return 'forbidden folder %s' % part
    if name.endswith(FORBIDDEN_SUFFIXES) or name == 'cmakelists.txt':
        return 'forbidden file type'
    if name in STAND_IN_NAMES or re.search(r'_sa_pbcfg\.[ch]$', name):
        return 'RTD-named stand-in'
    if not name.endswith(('.c', '.h')):
        return 'not a .c/.h file'
    return None


def read_sources():
    if not SOURCES_LIST.is_file():
        raise FileNotFoundError(fwd(SOURCES_LIST))
    out = []
    for line in SOURCES_LIST.read_text(encoding='utf-8').splitlines():
        line = line.split('#', 1)[0].strip()
        if line:
            out.append(line.replace('\\', '/'))
    return out


def plan(project, board):
    """[(source path, destination path, bundle relative path)], [refusals]."""
    items, refused = [], []
    net_dst = project / 'src' / 'network'
    rels = set()
    for rel in read_sources():
        if not (NETWORK / rel).is_file():
            refused.append('%s: listed in s32ds_sources.txt but missing' % rel)
            continue
        rels.add(rel)
    for folder in sorted({r.rsplit('/', 1)[0] for r in rels if '/' in r}):
        for h in sorted((NETWORK / folder).glob('*.h')):
            rels.add(fwd(h.relative_to(NETWORK)))
    for h in sorted((NETWORK / 'include' / 'network').rglob('*.h')):
        rel = fwd(h.relative_to(NETWORK))
        if rel not in EXCLUDED_HEADERS:
            rels.add(rel)
    for rel in sorted(rels):
        reason = forbidden(rel)
        if reason:
            refused.append('%s: %s' % (rel, reason))
        else:
            items.append((NETWORK / rel, net_dst / rel, 'network/' + rel))
    roots = [net_dst.resolve()]
    if board in BOARDS:
        src_dir, dest = BOARDS[board]
        board_dst = project / 'src' / dest
        roots.append(board_dst.resolve())
        if src_dir.is_dir():
            for f in sorted(src_dir.iterdir()):
                if f.is_file() and f.suffix in ('.c', '.h'):
                    items.append((f, board_dst / f.name, dest + '/' + f.name))
        else:
            refused.append('%s: board folder missing' % fwd(src_dir.relative_to(NETWORK)))
    checked = []
    for src, dst, rel in items:
        target = dst.resolve()
        if not any(target == r or r in target.parents for r in roots):
            refused.append('%s: destination outside src/network and the board folder' % rel)
        else:
            checked.append((src, dst, rel))
    return checked, refused


def extras(project, wanted):
    """Files in the destination folders (and any board folder) that are not in the bundles."""
    out = []
    for sub in ('network',) + BOARD_DESTS:
        root = project / 'src' / sub
        if root.is_dir():
            for f in sorted(root.rglob('*')):
                rel = sub + '/' + fwd(f.relative_to(root))
                if f.is_file() and rel not in wanted:
                    out.append(rel)
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    p.add_argument('--project', type=pathlib.Path, required=True, help='S32DS board project folder (bench clone)')
    p.add_argument('--board', choices=('bench', 'vss', 'none'), default='bench',
                   help='board folder to copy with the Network sources (default: bench)')
    p.add_argument('--apply', action='store_true', help='copy (default: dry run)')
    args = p.parse_args()
    project = args.project
    if project.resolve().name.lower() in REFERENCE_PROJECTS or project.name.lower() in REFERENCE_PROJECTS:
        print('REFUSED: %s is a bring-up reference project (read only)' % fwd(project))
        return 1
    if not (project / '.cproject').is_file() or not (project / 'src').is_dir():
        print('ERROR: %s is not an S32DS project (no .cproject or src/)' % fwd(project))
        return 2
    is_vss = (project / VSS_MARKER).is_file()
    if args.board == 'bench' and is_vss:
        print('REFUSED: %s is a VSS project (%s): the bench files duplicate VssApp_On*, SysTick_Handler '
              'and Board_*; use --board vss' % (fwd(project), VSS_MARKER))
        return 1
    if args.board == 'vss' and not is_vss:
        print('REFUSED: %s has no %s: --board vss is for the VSS board project' % (fwd(project), VSS_MARKER))
        return 1
    try:
        items, refused = plan(project, args.board)
    except FileNotFoundError as e:
        print('ERROR: missing %s' % e)
        return 2
    if refused:
        print('REFUSED (nothing copied):')
        for r in refused:
            print('  ' + r)
        return 1

    rows, counts = [], {'new': 0, 'changed': 0, 'unchanged': 0}
    for src, dst, rel in items:
        s = sha256(src)
        if not dst.is_file():
            action = 'new'
        elif sha256(dst) != s:
            action = 'changed'
        else:
            action = 'unchanged'
        counts[action] += 1
        rows.append({'path': rel, 'sha256': s, 'action': action, 'src': src, 'dst': dst})
    extra = extras(project, {r['path'] for r in rows})

    mode = 'APPLY' if args.apply else 'DRY RUN'
    dests = 'network' + (',' + BOARDS[args.board][1] if args.board in BOARDS else '')
    print('%s (--board %s): %s -> %s/src/{%s}' % (mode, args.board, fwd(NETWORK), fwd(project), dests))
    for r in rows:
        if r['action'] != 'unchanged':
            print('  %-9s %s' % (r['action'], r['path']))
    print('  new %d, changed %d, unchanged %d (total %d)'
          % (counts['new'], counts['changed'], counts['unchanged'], len(rows)))
    if extra:
        print('  not in the bundles (left in place, not deleted):')
        own = BOARDS[args.board][1] if args.board in BOARDS else None
        for e in extra:
            top = e.split('/', 1)[0]
            print('    ' + e + ('  (board folder of another kind)' if top in BOARD_DESTS and top != own else ''))

    errors = 0
    if args.apply:
        for r in rows:
            if r['action'] == 'unchanged':
                continue
            try:
                r['dst'].parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(r['src'], r['dst'])
                if sha256(r['dst']) != r['sha256']:
                    raise OSError('SHA-256 differs after copy')
            except OSError as e:
                errors += 1
                r['action'] = 'error'
                print('  ERROR %s: %s' % (r['path'], e))

    stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    out = pathlib.Path(tempfile.mkdtemp(prefix='VSSNET_sync_%s_%s_%s_' % (
        stamp, args.board, 'apply' if args.apply else 'dry')))
    manifest = {'mode': mode.lower(), 'board': args.board, 'project': fwd(project.resolve()), 'network': fwd(NETWORK),
                'created': datetime.datetime.now().isoformat(timespec='seconds'),
                'files': [{k: r[k] for k in ('path', 'sha256', 'action')} for r in rows],
                'not_in_bundles': extra}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=1), encoding='utf-8')
    print('manifest: %s' % fwd(out / 'manifest.json'))
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
