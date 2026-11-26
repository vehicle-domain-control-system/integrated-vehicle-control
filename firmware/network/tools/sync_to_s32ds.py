"""Copy the Network product sources into an S32DS board project, or generate a node bundle.

Board project mode (--project):
  (1) the C sources of tools/s32ds_sources.txt except the DOMAIN-only ones,
      include/network/** and nodes/<node>/include/network/** (except the
      headers no board build uses) and the *.h files of the listed source
      folders, in firmware/network and in the node folders (node tags of
      headers are ignored)
      -> <project>/src/network/<logical path>: the path relative to
      firmware/network, without nodes/<node>/ for the files of one node
  (2) the board folder chosen with --board (default bench):
        bench     nodes/vss/bench/s32k344/*.c|*.h      -> <project>/src/net_bench/  (S32K344 bench clone)
        bench144  nodes/bcm/bench/s32k144_bcm/*.c|*.h  -> <project>/src/net_bench/  (S32K144 BCM bench clone)
        cis144    nodes/cis/bench/s32k144_cis/*.c|*.h  -> <project>/src/net_bench/  (S32K144 CIS bench clone)
        vss       nodes/vss/board/s32k344_vss/*.c|*.h  -> <project>/src/net_board/  (VSS board project)
        none      nothing (Network sources only)
  Default is a dry run: it prints what would be copied (new / changed /
  unchanged) and writes nothing into the project. --apply copies.
  Refused:
    - a bring-up reference project as --project (folder name
      network_344_can_bringup or network_144_can_debuger): read-only references
    - any source under sdk_mock, tests, tools, host_*, s32k144, any .md / .csv /
      CMake file, and any RTD-named stand-in header (FlexCAN_Ip.h, *_Sa_PBcfg.h ...)
      -- these must never enter an S32DS workspace
    - a destination outside <project>/src/network or the board folder
    - two sources with the same logical path
    - --board bench, bench144 or cis144 on a VSS project (src/app/vss_main.c
      present): the bench files define SysTick_Handler and Board_* (bench: also
      VssApp_On*) that VSS already has
    - --board bench144 or cis144 on a project whose .cproject has no CPU_S32K144
      define: the bench boards share src/net_bench/, so a wrong board would mix
      the S32K344 and S32K144 bench files
    - --board vss on a project without src/app/vss_main.c
  Files already in the destination folders that are not in the bundles are
  reported, never deleted (also a board folder of the other kind).

Node bundle mode (--node VSS|CIS|BCM|DOMAIN): the Network copy kept in a node's
folder. It is generated, never edited by hand: change firmware/network and
generate it again.
  --out <dir>/network    dry run: what would be written, changed, deleted
                         (writes nothing); --apply writes the bundle
  --check <dir>/network  compares a bundle with its record and with the current
                         sources (writes nothing)
  Contents, at their logical paths (relative to firmware/network, without
  nodes/<node>/ for the files of that node):
    - the .c files of tools/s32ds_sources.txt without a tag or tagged with the node
    - include/network/** without net_host_mock.h and net_virtual.h,
      nodes/<node>/include/network/**, and the *.h files of the folders of
      those .c files (in firmware/network and in the node folders); a header
      tagged in the list goes only to the nodes it names, and a header in
      nodes/<node>/ only to that node
    - nodes/<node>/board/s32k344_<node>/ (.c, .h) when that folder exists
    - README.md: a copy of nodes/<node>/<NODE>_NODE_INTEGRATION.md (refused
      when missing)
    - NETWORK_BUNDLE.txt: node, profile, source branch / commit / state, and the
      SHA-256 of every other file computed with CRLF read as LF (so a Windows
      checkout gives the same value); files are copied byte for byte
  --apply is refused, and nothing is written, when the folder is not named
  network; is firmware/network, inside it or contains it; the sources of the
  bundle are not committed (source_state dirty, or git not usable) unless
  --allow-dirty (tests); or the folder is not empty and has no valid
  NETWORK_BUNDLE.txt of the same node, or holds an edited file or a file the
  record does not list. Write order: new and changed files (hash checked after
  the copy), files of the old record that left the bundle are deleted, empty
  folders removed, NETWORK_BUNDLE.txt last. With the same sources a second run
  changes nothing.
  --check: integrity first (record missing or malformed, another node, a
  recorded file missing or edited, a file not recorded -> EDITED), then
  freshness (the current sources give other paths or hashes -> STALE). The
  last line is CHECK <NODE> OK, CHECK <NODE> EDITED or CHECK <NODE> STALE.

Every run writes a manifest (relative path, SHA-256, action) to a new folder
under the temp directory, never into the project or the bundle folder.

Exit: 0 ok, 1 refused or copy error (--check: EDITED), 2 usage / missing input /
malformed tools/s32ds_sources.txt, 3 (--check) STALE.
Example:
  python firmware/network/tools/sync_to_s32ds.py --project C:/Users/me/ws/network_344_net_bench
  python firmware/network/tools/sync_to_s32ds.py --project C:/Users/me/ws/network_344_net_bench --apply
  python firmware/network/tools/sync_to_s32ds.py --project C:/Users/me/ws/network_144_net_bench --board bench144
  python firmware/network/tools/sync_to_s32ds.py --project C:/Users/me/ws/network_144_net_cis --board cis144
  python firmware/network/tools/sync_to_s32ds.py --project C:/Users/me/ws/S32K344_WB_Audio_Bringup --board vss
  python firmware/network/tools/sync_to_s32ds.py --node DOMAIN --out ../NET_domain_pr/firmware/network_nodes/DOMAIN/network --apply
  python firmware/network/tools/sync_to_s32ds.py --node DOMAIN --check ../NET_domain_pr/firmware/network_nodes/DOMAIN/network
"""
from __future__ import annotations
import argparse, datetime, hashlib, json, os, pathlib, re, shutil, subprocess, sys, tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import source_list  # noqa: E402  (tools/source_list.py, the reader of tools/s32ds_sources.txt)

NETWORK = source_list.NETWORK
SOURCES_LIST = source_list.LIST_PATH
# --board: source folder, destination folder under <project>/src.
BOARDS = {'bench': (NETWORK / 'nodes' / 'vss' / 'bench' / 's32k344', 'net_bench'),
          'bench144': (NETWORK / 'nodes' / 'bcm' / 'bench' / 's32k144_bcm', 'net_bench'),
          'cis144': (NETWORK / 'nodes' / 'cis' / 'bench' / 's32k144_cis', 'net_bench'),
          'vss': (NETWORK / 'nodes' / 'vss' / 'board' / 's32k344_vss', 'net_board')}
BOARD_DESTS = ('net_bench', 'net_board')
VSS_MARKER = 'src/app/vss_main.c'
# --board bench144 / cis144 need this S32DS CPU define in the project's .cproject.
S32K144_MARKER = 'CPU_S32K144'
S32K144_BOARDS = ('bench144', 'cis144')
REFERENCE_PROJECTS = ('network_344_can_bringup', 'network_144_can_debuger')
# Host-only headers (logical paths): never in a board project copy or a node bundle.
HOST_HEADERS = ('include/network/net_host_mock.h', 'include/network/net_virtual.h')
# Headers no board project copy uses (logical paths): the host-only ones, and net_domain_c2.h,
# which only compiles together with the DOMAIN application headers (a node
# bundle takes it from its DOMAIN tag instead).
EXCLUDED_HEADERS = HOST_HEADERS + ('include/network/net_domain_c2.h',)
FORBIDDEN_PARTS = ('sdk_mock', 'tests', 'tools', 's32k144')
FORBIDDEN_SUFFIXES = ('.md', '.csv', '.cmake')
# File names of the mock SDK that shadow real RTD headers.
STAND_IN_NAMES = ('flexcan_ip.h', 'lpuart_uart_ip.h', 'osif.h', 'std_types.h')
# Node bundle record.
RECORD = 'NETWORK_BUNDLE.txt'
RECORD_KEYS = ('format', 'node', 'profile', 'source', 'source_branch', 'source_commit', 'source_state', 'files')
BUNDLE_PATH = re.compile(r'^[A-Za-z0-9_][A-Za-z0-9_./-]*$')
FILE_LINE = re.compile(r'^([0-9a-f]{64})  (\S+)$')


def fwd(p):
    return str(p).replace('\\', '/')


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for chunk in iter(lambda: fh.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def bundle_rel_ok(rel):
    """A bundle path: relative, '/'-separated, no empty, '.' or '..' part (nothing outside the folder)."""
    return bool(BUNDLE_PATH.match(rel)) and all(part not in ('', '.', '..') for part in rel.split('/'))


def lf_sha256(path):
    """SHA-256 of the content with CRLF read as LF (the hash of a bundle record)."""
    with open(path, 'rb') as fh:
        return hashlib.sha256(fh.read().replace(b'\r\n', b'\n')).hexdigest()


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


def read_list():
    """Entries of tools/s32ds_sources.txt, or (None, exit code 2) after printing why."""
    try:
        return source_list.read(), 0
    except OSError as e:
        print('ERROR: missing %s (%s)' % (fwd(SOURCES_LIST), e))
    except source_list.ListError as e:
        print('ERROR: tools/s32ds_sources.txt is malformed:')
        for x in e.problems:
            print('  ' + x)
    return None, 2


def public_header_roots():
    """include/network and the include/network folder of every node folder (that exists)."""
    roots = [NETWORK / 'include' / 'network']
    for folder in source_list.NODE_DIRS.values():
        root = NETWORK / folder / 'include' / 'network'
        if root.is_dir():
            roots.append(root)
    return roots


def folder_headers(sources):
    """*.h files of the logical folders of sources, from firmware/network and every node folder."""
    out = set()
    for folder in sorted({source_list.logical(r).rsplit('/', 1)[0] for r in sources if '/' in r}):
        for base in [NETWORK] + [NETWORK / d for d in source_list.NODE_DIRS.values()]:
            out.update(fwd(h.relative_to(NETWORK)) for h in (base / folder).glob('*.h'))
    return out


def logical_paths(rels):
    """({logical path: source path}, [refusals]) of source paths relative to firmware/network."""
    out, refused = {}, []
    for rel in sorted(rels):
        lrel = source_list.logical(rel)
        if lrel in out:
            refused.append('%s: same logical path %s as %s' % (rel, lrel, out[lrel]))
        else:
            out[lrel] = rel
    return out, refused


def missing_listed(entries):
    return ['%s: listed in s32ds_sources.txt but missing' % e.path for e in entries
            if not (NETWORK / e.path).is_file()]


# ---------------------------------------------------------------- board project

def plan(project, board, entries):
    """[(source path, destination path, bundle relative path)], [refusals]."""
    items, refused = [], missing_listed(entries)
    net_dst = project / 'src' / 'network'
    rels = {r for r in source_list.board_sources(entries) if (NETWORK / r).is_file()}
    rels.update(folder_headers(rels))
    for root in public_header_roots():
        for h in sorted(root.rglob('*.h')):
            rel = fwd(h.relative_to(NETWORK))
            if source_list.logical(rel) not in EXCLUDED_HEADERS:
                rels.add(rel)
    by_logical, refused_paths = logical_paths(rels)
    refused += refused_paths
    for lrel, rel in sorted(by_logical.items()):
        reason = forbidden(rel) or forbidden(lrel)
        if reason:
            refused.append('%s: %s' % (rel, reason))
        else:
            items.append((NETWORK / rel, net_dst / lrel, 'network/' + lrel))
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


def project_main(args):
    project, board = args.project, args.board or 'bench'
    if project.resolve().name.lower() in REFERENCE_PROJECTS or project.name.lower() in REFERENCE_PROJECTS:
        print('REFUSED: %s is a bring-up reference project (read only)' % fwd(project))
        return 1
    if not (project / '.cproject').is_file() or not (project / 'src').is_dir():
        print('ERROR: %s is not an S32DS project (no .cproject or src/)' % fwd(project))
        return 2
    is_vss = (project / VSS_MARKER).is_file()
    if board == 'bench' and is_vss:
        print('REFUSED: %s is a VSS project (%s): the bench files duplicate VssApp_On*, SysTick_Handler '
              'and Board_*; use --board vss' % (fwd(project), VSS_MARKER))
        return 1
    if board in S32K144_BOARDS and is_vss:
        print('REFUSED: %s is a VSS project (%s): the %s files duplicate SysTick_Handler '
              'and Board_*; use --board vss' % (fwd(project), VSS_MARKER, board))
        return 1
    if board in S32K144_BOARDS and S32K144_MARKER not in (project / '.cproject').read_text(encoding='utf-8',
                                                                                             errors='replace'):
        print('REFUSED: %s: .cproject has no %s define: --board %s is for an S32K144 project '
              '(src/net_bench/ would mix S32K344 and S32K144 bench files)' % (fwd(project), S32K144_MARKER, board))
        return 1
    if board == 'vss' and not is_vss:
        print('REFUSED: %s has no %s: --board vss is for the VSS board project' % (fwd(project), VSS_MARKER))
        return 1
    entries, rc = read_list()
    if entries is None:
        return rc
    items, refused = plan(project, board, entries)
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
    dests = 'network' + (',' + BOARDS[board][1] if board in BOARDS else '')
    print('%s (--board %s): %s -> %s/src/{%s}' % (mode, board, fwd(NETWORK), fwd(project), dests))
    for r in rows:
        if r['action'] != 'unchanged':
            print('  %-9s %s' % (r['action'], r['path']))
    print('  new %d, changed %d, unchanged %d (total %d)'
          % (counts['new'], counts['changed'], counts['unchanged'], len(rows)))
    if extra:
        print('  not in the bundles (left in place, not deleted):')
        own = BOARDS[board][1] if board in BOARDS else None
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
        stamp, board, 'apply' if args.apply else 'dry')))
    manifest = {'mode': mode.lower(), 'board': board, 'project': fwd(project.resolve()), 'network': fwd(NETWORK),
                'created': datetime.datetime.now().isoformat(timespec='seconds'),
                'files': [{k: r[k] for k in ('path', 'sha256', 'action')} for r in rows],
                'not_in_bundles': extra}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=1), encoding='utf-8')
    print('manifest: %s' % fwd(out / 'manifest.json'))
    return 1 if errors else 0


# ---------------------------------------------------------------- node bundle

def node_bundle(node, entries):
    """({bundle path: source file}, [refusals]): the bundle of one node from the current sources."""
    refused = missing_listed(entries)
    sources = [r for r in source_list.node_sources(entries, node) if (NETWORK / r).is_file()]
    tagged = source_list.node_headers(entries)
    home = NETWORK / source_list.NODE_DIRS[node]
    headers = {fwd(h.relative_to(NETWORK)) for h in (NETWORK / 'include' / 'network').rglob('*.h')}
    headers.update(fwd(h.relative_to(NETWORK)) for h in (home / 'include' / 'network').rglob('*.h'))
    headers.update(folder_headers(sources))
    headers = {h for h in headers if source_list.logical(h) not in HOST_HEADERS}
    headers = {h for h in headers if h not in tagged or node in tagged[h]}
    # A header in another node's folder belongs to that node, listed or not.
    headers = {h for h in headers if source_list.node_of(h) in (None, node)}
    headers.update(h for h, nodes in tagged.items() if node in nodes and (NETWORK / h).is_file())
    rels = set(sources) | headers
    board = home / 'board' / ('s32k344_' + node.lower())
    if board.is_dir():
        for f in sorted(board.rglob('*')):
            if f.is_file() and f.suffix in ('.c', '.h'):
                rels.add(fwd(f.relative_to(NETWORK)))
    by_logical, refused_paths = logical_paths(rels)
    refused += refused_paths
    files = {lrel: NETWORK / rel for lrel, rel in by_logical.items()}
    for lrel, rel in sorted(by_logical.items()):
        reason = forbidden(rel) or forbidden(lrel)
        if reason:
            refused.append('%s: %s' % (rel, reason))
        elif not bundle_rel_ok(lrel):
            refused.append('%s: path not allowed in a bundle' % lrel)
    readme = home / ('%s_NODE_INTEGRATION.md' % node)
    if readme.is_file():
        files['README.md'] = readme
    else:
        refused.append('%s%s_NODE_INTEGRATION.md: missing (it is the README.md of the bundle)'
                       % (source_list.NODE_DIRS[node], node))
    return files, refused


def profile_line(node):
    pid = source_list.profile_ids().get(node)
    return None if pid is None else 'NETWORK_UPPER_NODE_PROFILE=%d' % pid


def git_state(rels):
    """(branch, commit, state, note) of the sources rels (relative to firmware/network)."""
    def git(*cmd):
        r = subprocess.run(['git', '--literal-pathspecs', '-C', str(NETWORK)] + list(cmd), stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, text=True, encoding='utf-8', errors='replace', timeout=300)
        return r.returncode, r.stdout
    try:
        rc, inside = git('rev-parse', '--is-inside-work-tree')
        if rc != 0 or inside.strip() != 'true':
            return 'unknown', 'unknown', 'dirty', 'firmware/network is not in a git work tree'
        rc, commit = git('rev-parse', '--verify', '--quiet', 'HEAD')
        commit = commit.strip()
        if rc != 0 or not re.match(r'^[0-9a-f]{40}$', commit):
            return 'unknown', 'unknown', 'dirty', 'no HEAD commit'
        rc, branch = git('symbolic-ref', '--quiet', '--short', 'HEAD')
        branch = branch.strip() if rc == 0 and branch.strip() else 'unknown'
        rc, tracked = git('ls-files', '-z', '--', *rels)
        if rc != 0:
            return branch, commit, 'dirty', 'git ls-files failed'
        untracked = sorted(set(rels) - {t for t in tracked.split('\0') if t})
        if untracked:
            return branch, commit, 'dirty', 'not committed: %s' % ', '.join(untracked[:5])
        rc, _ = git('diff', '--quiet', '--no-ext-diff', '--no-textconv', 'HEAD', '--', *rels)
        if rc != 0:
            return branch, commit, 'dirty', 'uncommitted changes, git diff HEAD' if rc == 1 else 'git diff failed'
    except (OSError, subprocess.SubprocessError) as e:
        return 'unknown', 'unknown', 'dirty', 'git not usable: %s' % e
    return branch, commit, 'clean', ''


def render(head, digests):
    """NETWORK_BUNDLE.txt text (LF, no time stamp) for header values and {path: LF SHA-256}."""
    lines = ['# Network sources for the %s node, generated by firmware/network/tools/sync_to_s32ds.py.' % head['node'],
             '# Do not edit files in this folder. Change firmware/network and generate again.',
             'format: 1', 'node: ' + head['node'], 'profile: ' + head['profile'], 'source: firmware/network',
             'source_branch: ' + head['source_branch'], 'source_commit: ' + head['source_commit'],
             'source_state: ' + head['source_state'], 'files: %d' % len(digests)]
    lines += ['%s  %s' % (digests[r], r) for r in sorted(digests)]
    return '\n'.join(lines) + '\n'


def read_record(folder):
    """(header dict or None, {path: SHA-256}, [problems]) of <folder>/NETWORK_BUNDLE.txt."""
    path = folder / RECORD
    if not path.is_file():
        return None, {}, ['%s missing' % RECORD]
    try:
        text = path.read_bytes().decode('utf-8')
    except (OSError, UnicodeDecodeError) as e:
        return None, {}, ['%s unreadable: %s' % (RECORD, e)]
    # A checkout with CRLF line ends (core.autocrlf) reads like the LF original.
    lines = text.replace('\r\n', '\n').split('\n')
    if lines[-1] == '':
        lines.pop()
    head, digests, problems = {}, {}, []
    rest = [l for l in lines if not l.startswith('#')]
    for key, line in zip(RECORD_KEYS, rest):
        if line.startswith(key + ': '):
            head[key] = line[len(key) + 2:]
    if len(head) != len(RECORD_KEYS):
        return None, {}, ['%s: the header lines %s are not all present in this order'
                          % (RECORD, ', '.join(RECORD_KEYS))]
    for line in rest[len(RECORD_KEYS):]:
        m = FILE_LINE.match(line)
        if not m or not bundle_rel_ok(m.group(2)) or m.group(2) == RECORD or m.group(2) in digests:
            problems.append('%s: unexpected line %r' % (RECORD, line[:100]))
        else:
            digests[m.group(2)] = m.group(1)
    if head['format'] != '1' or head['node'] not in source_list.NODES or head['source'] != 'firmware/network' \
            or not re.match(r'^NETWORK_UPPER_NODE_PROFILE=\d+$', head['profile']) \
            or not re.match(r'^([0-9a-f]{40}|unknown)$', head['source_commit']) \
            or head['source_state'] not in ('clean', 'dirty') or head['files'] != str(len(digests)):
        problems.append('%s: header values not valid (format 1, a node, profile, source, commit, state, files %d)'
                        % (RECORD, len(digests)))
    if not problems and text.replace('\r\n', '\n') != render(head, digests):
        problems.append('%s is not in the generated form (edited, or file lines not sorted)' % RECORD)
    return (head if not problems else None), digests, problems


def audit(folder, node):
    """Integrity of a bundle folder: (record header, recorded {path: SHA-256}, record problems,
    edited, missing, not recorded)."""
    head, recorded, problems = read_record(folder)
    if head is not None and head['node'] != node:
        problems.append('%s is the record of a %s bundle, not %s' % (RECORD, head['node'], node))
    edited, missing, foreign = [], [], []
    if not problems:
        for rel, digest in sorted(recorded.items()):
            p = folder / rel
            if not p.is_file():
                missing.append(rel)
            elif lf_sha256(p) != digest:
                edited.append(rel)
        on_disk = {fwd(f.relative_to(folder)) for f in folder.rglob('*') if not f.is_dir()}
        foreign = sorted(on_disk - set(recorded) - {RECORD})
    return head, recorded, problems, edited, missing, foreign


def norm(p):
    return pathlib.Path(os.path.normcase(str(pathlib.Path(p).resolve())))


def node_manifest(data):
    stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    out = pathlib.Path(tempfile.mkdtemp(prefix='VSSNET_sync_%s_node_%s_%s_' % (stamp, data['node'], data['mode'])))
    data = dict(data, network=fwd(NETWORK), created=datetime.datetime.now().isoformat(timespec='seconds'))
    (out / 'manifest.json').write_text(json.dumps(data, indent=1), encoding='utf-8')
    print('manifest: %s' % fwd(out / 'manifest.json'))


def node_out(args, entries):
    node, out = args.node, args.out
    files, refused = node_bundle(node, entries)
    profile = profile_line(node)
    if profile is None:
        print('ERROR: include/network/net_upper_profile.h has no NET_UPPER_PROFILE_%s' % node)
        return 2
    if out.name != 'network':
        refused.append('--out %s: the bundle folder must be named network' % fwd(out))
    target, net = norm(out), norm(NETWORK)
    if target == net or net in target.parents or target in net.parents:
        refused.append('--out %s: is firmware/network, inside it or contains it' % fwd(out))
    if any(part.lower() in REFERENCE_PROJECTS for part in target.parts):
        refused.append('--out %s: inside a bring-up reference project (read only)' % fwd(out))
    rels = sorted({fwd(src.relative_to(NETWORK)) for src in files.values()} | {fwd(SOURCES_LIST.relative_to(NETWORK))})
    branch, commit, state, note = git_state(rels)
    if state != 'clean' and not args.allow_dirty:
        refused.append('source_state %s (%s): commit firmware/network first' % (state, note))
    old = {}
    if out.exists() and not out.is_dir():
        refused.append('--out %s exists and is not a folder' % fwd(out))
    elif out.is_dir() and any(out.iterdir()):
        _, old, problems, edited, _, foreign = audit(out, node)
        refused += ['%s: %s' % (fwd(out), x) for x in problems]
        refused += ['%s: edited after generation (hash differs from %s)' % (r, RECORD) for r in edited]
        refused += ['%s: not recorded in NETWORK_BUNDLE.txt (not written by this tool)' % r for r in foreign]

    digests = {rel: lf_sha256(src) for rel, src in files.items() if src.is_file()}
    rows, counts = [], {'new': 0, 'changed': 0, 'unchanged': 0}
    for rel in sorted(digests):
        dst = out / rel
        if not dst.is_file():
            action = 'new'
        elif lf_sha256(dst) != digests[rel]:
            action = 'changed'
        else:
            action = 'unchanged'
        counts[action] += 1
        rows.append({'path': rel, 'sha256': digests[rel], 'action': action})
    removed = sorted(set(old) - set(digests))
    head = {'node': node, 'profile': profile, 'source_branch': branch, 'source_commit': commit,
            'source_state': state}
    record = render(head, digests)
    record_path = out / RECORD
    try:
        same_record = record_path.read_bytes().decode('utf-8').replace('\r\n', '\n') == record
    except (OSError, UnicodeDecodeError):
        same_record = False

    mode = 'APPLY' if args.apply else 'DRY RUN'
    print('%s (--node %s): %s -> %s' % (mode, node, fwd(NETWORK), fwd(out)))
    for r in rows:
        if r['action'] != 'unchanged':
            print('  %-9s %s' % (r['action'], r['path']))
    for rel in removed:
        print('  %-9s %s' % ('delete', rel))
    print('  new %d, changed %d, unchanged %d, delete %d (%d files + %s %s)'
          % (counts['new'], counts['changed'], counts['unchanged'], len(removed), len(rows), RECORD,
             'unchanged' if same_record else 'written last'))
    print('  source: branch %s, commit %s, state %s%s' % (branch, commit, state, ' (%s)' % note if note else ''))
    manifest = {'mode': 'apply' if args.apply else 'dry', 'node': node, 'out': fwd(target), 'profile': profile,
                'source_branch': branch, 'source_commit': commit, 'source_state': state, 'files': rows,
                'delete': removed, 'refused': refused}
    if refused:
        print('REFUSED (nothing written):')
        for r in refused:
            print('  ' + r)
        node_manifest(manifest)
        return 1
    if not args.apply:
        node_manifest(manifest)
        return 0

    errors = 0
    for r in rows:
        if r['action'] == 'unchanged':
            continue
        src, dst = files[r['path']], out / r['path']
        try:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dst)
            if sha256(dst) != sha256(src):
                raise OSError('SHA-256 differs after copy')
        except OSError as e:
            errors += 1
            r['action'] = 'error'
            print('  ERROR %s: %s' % (r['path'], e))
    if not errors:
        for rel in removed:
            try:
                (out / rel).unlink()
            except FileNotFoundError:
                pass
            except OSError as e:
                errors += 1
                print('  ERROR deleting %s: %s' % (rel, e))
        for d in sorted((p for p in out.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
            if not any(d.iterdir()):
                d.rmdir()
    if errors:
        print('ERROR: %d file(s) failed; %s not written (the folder is not a valid bundle until a run succeeds)'
              % (errors, RECORD))
        print('  Fix the cause, then delete or empty %s and run --apply again: the next --apply refuses '
              'this folder (its files no longer match the old record).' % fwd(out))
        node_manifest(manifest)
        return 1
    if not same_record:
        record_path.write_bytes(record.encode('utf-8'))
    node_manifest(manifest)
    return 0


def node_check(node, folder, entries):
    print('CHECK (--node %s): %s against %s' % (node, fwd(folder), fwd(NETWORK)))
    head, recorded, problems, edited, missing, foreign = audit(folder, node)
    bad = (problems + ['edited      %s' % r for r in edited] + ['missing     %s' % r for r in missing]
           + ['not listed  %s' % r for r in foreign])
    manifest = {'mode': 'check', 'node': node, 'out': fwd(norm(folder)), 'integrity': bad}
    if bad:
        print('  the folder no longer matches its record (generated files must not be edited):')
        for x in bad:
            print('  ' + x)
        node_manifest(manifest)
        print('CHECK %s EDITED' % node)
        return 1
    files, refused = node_bundle(node, entries)
    profile = profile_line(node)
    if refused or profile is None:
        print('ERROR: the current sources give no %s bundle:' % node)
        for r in refused + ([] if profile else ['net_upper_profile.h: no NET_UPPER_PROFILE_%s' % node]):
            print('  ' + r)
        return 2
    digests = {rel: lf_sha256(src) for rel, src in files.items()}
    stale = [] if head['profile'] == profile else ['profile     %s -> %s' % (head['profile'], profile)]
    for rel in sorted(set(digests) | set(recorded)):
        if rel not in recorded:
            stale.append('new         %s' % rel)
        elif rel not in digests:
            stale.append('removed     %s' % rel)
        elif digests[rel] != recorded[rel]:
            stale.append('changed     %s' % rel)
    print('  record: %d files, source_commit %s (%s)' % (len(recorded), head['source_commit'], head['source_state']))
    manifest['stale'] = stale
    node_manifest(manifest)
    if stale:
        print('  intact, but the current sources differ; generate again:')
        for x in stale:
            print('  ' + x)
        print('CHECK %s STALE' % node)
        return 3
    print('CHECK %s OK' % node)
    return 0


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    p.add_argument('--project', type=pathlib.Path, help='S32DS board project folder (board project mode)')
    p.add_argument('--board', choices=('bench', 'bench144', 'cis144', 'vss', 'none'),
                   help='--project: board folder to copy with the Network sources (default: bench)')
    p.add_argument('--node', choices=source_list.NODES, help='node bundle mode')
    p.add_argument('--out', type=pathlib.Path, help='--node: bundle folder to generate (named network)')
    p.add_argument('--check', type=pathlib.Path, help='--node: bundle folder to check (writes nothing)')
    p.add_argument('--apply', action='store_true', help='write (default: dry run)')
    p.add_argument('--allow-dirty', action='store_true',
                   help='--node --out: accept sources with uncommitted changes or without git (tests)')
    args = p.parse_args()
    if args.node:
        if args.project is not None or args.board is not None:
            p.error('--node cannot be combined with --project or --board')
        if (args.out is None) == (args.check is None):
            p.error('--node needs exactly one of --out and --check')
        if args.check is not None and (args.apply or args.allow_dirty):
            p.error('--check writes nothing: --apply and --allow-dirty belong to --out')
        entries, rc = read_list()
        if entries is None:
            return rc
        if args.check is not None:
            return node_check(args.node, args.check, entries)
        return node_out(args, entries)
    if args.out is not None or args.check is not None or args.allow_dirty:
        p.error('--out, --check and --allow-dirty need --node')
    if args.project is None:
        p.error('one of --project (board project) or --node (node bundle) is required')
    return project_main(args)


if __name__ == '__main__':
    sys.exit(main())
