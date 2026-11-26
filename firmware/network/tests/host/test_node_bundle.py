"""Per-node Network bundles (tools/sync_to_s32ds.py --node / --check), the node
tags of tools/s32ds_sources.txt and the required NETWORK_UPPER_NODE_PROFILE.

One case per process (ctest tools.node_bundle_<case>):
  python -B test_node_bundle.py <case> --cc <C compiler> --cmake <cmake>

Every case copies firmware/network (without build folders, tests and caches)
into a new temporary folder and runs the tools of that copy, so this tree is
never written. The copy is outside any git work tree (GIT_CEILING_DIRECTORIES),
so --apply needs --allow-dirty there; the record and apply_safety cases also
make a throwaway git repository in the temporary folder for the clean and
dirty source states.

The expected bundle contents are derived here from the bundle rules
(DOMAIN_NODE_INTEGRATION.md, tools/s32ds_sources.txt header) and the minimum
node tags below, not from the tool's own code. Paths in this test are logical
paths (the paths of a bundle); the files of one node live in nodes/<node>/ at
that path, and logical() / source_of() below map the two without the tool.
"""
import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

NETWORK = pathlib.Path(__file__).resolve().parents[2]
FIXTURE = NETWORK / 'tests' / 'host' / 'fixtures' / 'board_copy_sets.json'
PROBE = NETWORK / 'tests' / 'host' / 'node_bundle_list_probe.cmake'
NODES = ('VSS', 'CIS', 'BCM', 'DOMAIN')
NODE_DIRS = {n: 'nodes/%s/' % n.lower() for n in NODES}
RECORD = 'NETWORK_BUNDLE.txt'
M03 = 'src/l02_typed_handoff/m03_typed_handoff/'
ADAPTERS = tuple(M03 + n for n in ('net_domain_c2.c', 'net_domain_rx.c', 'net_domain_tx.c', 'net_domain_uart.c',
                                   'net_domain_query.c', 'net_domain_owner_binding.c'))
DOMAIN_HEADERS = ('include/network/net_domain_c2.h', M03 + 'net_domain_private.h', M03 + 'net_domain_enum_internal.h')
RTD = ('src/platform/rtd/net_hal_rtd.c', 'src/platform/rtd/net_hal_rtd_can.c', 'src/platform/rtd/net_hal_rtd_uart.c')
ROOT = ('src/node/net_node.c', 'src/node/net_entry.c', 'include/network/app/net_if_entry.h', 'include/network/net_node.h')
# The DOMAIN root (src/node/net_node_domain.c, C2 adapter as Owner) shares the
# public entry and the node header; the folder header of src/node comes along.
DOMAIN_ROOT = ('src/node/net_node_domain.c', 'src/node/net_entry.c', 'include/network/app/net_if_entry.h',
               'include/network/net_node.h')
DOMAIN_NODE_DIR = ('src/node/net_entry.c', 'src/node/net_node_domain.c', 'src/node/net_node_profile.h')
DOMAIN_BOARD = ('board/s32k344_domain/net_domain_board.c', 'board/s32k344_domain/net_domain_board.h')
OWN = {
    'VSS': ('src/node/net_node_profile_vss.c', M03 + 'net_app_port_vss.c', 'include/network/app/net_if_vss.h',
            'include/network/net_app_port_vss.h'),
    'CIS': ('src/node/net_node_profile_cis.c', M03 + 'net_app_port_cis.c', 'include/network/app/net_if_cis.h',
            'include/network/app/net_if_pi_data.h', 'include/network/net_app_port_cis.h'),
    'BCM': ('src/node/net_node_profile_bcm.c', M03 + 'net_app_port_bcm.c', 'include/network/app/net_if_bcm.h',
            'include/network/net_app_port_bcm.h'),
}
HOST_HEADERS = ('include/network/net_host_mock.h', 'include/network/net_virtual.h')
VSS_BOARD = ('board/s32k344_vss/net_vss_board.c', 'board/s32k344_vss/net_vss_board.h')
# The board copy no longer carries the S32K344 upper-port wrapper: the node root
# passes the MCU chosen in net_board_cfg.h to net_hal_adapter_init().
MCU_WRAPPER = ('network/src/platform/s32k344/net_s32k344_upper_port.c',
               'network/src/platform/s32k344/net_s32k344_port.h')
# Minimum node tags of tools/s32ds_sources.txt: path -> nodes.
MIN_TAGS = {p: {'DOMAIN'} for p in ADAPTERS + DOMAIN_HEADERS}
MIN_TAGS.update({p: {n} for n, files in OWN.items() for p in files})
MIN_TAGS.update({p: {'VSS', 'CIS', 'BCM'} for p in ROOT})
MIN_TAGS.update({p: {'VSS', 'CIS', 'BCM', 'DOMAIN'} for p in ROOT if p in DOMAIN_ROOT})
MIN_TAGS.update({p: {'DOMAIN'} for p in ('src/node/net_node_domain.c',) + DOMAIN_BOARD})
COMMON_PROBE = 'src/l04_frame_codec/net_wire_common.c'
# A file of one node, as listed (in its node folder).
NODE_PORT = NODE_DIRS['VSS'] + M03 + 'net_app_port_vss.c'
REGISTRY = 'src/l04_frame_codec/net_wire_registry.c'


def fwd(p):
    return str(p).replace('\\', '/')


def logical(path):
    """Bundle path of a path relative to firmware/network: without nodes/<node>/."""
    for folder in NODE_DIRS.values():
        if path.startswith(folder):
            return path[len(folder):]
    return path


def source_of(net, rel):
    """The file of the tree net behind the logical path rel (firmware/network or one node folder)."""
    hits = [p for p in [net / rel] + [net / d / rel for d in NODE_DIRS.values()] if p.is_file()]
    if len(hits) != 1:
        raise AssertionError('logical path %s: %d files in the tree' % (rel, len(hits)))
    return hits[0]


def integration(net, node):
    return net / NODE_DIRS[node] / ('%s_NODE_INTEGRATION.md' % node)


def lf_sha(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def files_of(folder):
    folder = pathlib.Path(folder)
    if not folder.is_dir():
        return {}
    return {fwd(p.relative_to(folder)): p for p in folder.rglob('*') if p.is_file()}


def snapshot(folder):
    """Every file (LF hash, mtime, size) and every folder below folder."""
    folder = pathlib.Path(folder)
    if not folder.exists():
        return None
    out = {}
    for p in folder.rglob('*'):
        rel = fwd(p.relative_to(folder))
        if p.is_dir():
            out[rel + '/'] = 'dir'
        else:
            st = p.stat()
            out[rel] = (lf_sha(p), st.st_mtime_ns, st.st_size)
    return out


def last_line(text):
    lines = [l for l in text.strip().splitlines() if l.strip()]
    return lines[-1].strip() if lines else ''


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, str(path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def list_lines(net):
    """[(path, set of tags)] of a source list, parsed here independently of tools/source_list.py."""
    out = []
    for raw in (net / 'tools' / 's32ds_sources.txt').read_text(encoding='utf-8').splitlines():
        tokens = raw.split('#', 1)[0].split()
        if tokens:
            out.append((tokens[0], {t[1:] for t in tokens[1:]}))
    return out


def profile_ids(net):
    text = (net / 'include' / 'network' / 'net_upper_profile.h').read_text(encoding='utf-8')
    ids = {}
    for name in NODES + ('HOST',):
        m = re.search(r'^#define NET_UPPER_PROFILE_%s (\d+)\s*$' % name, text, re.M)
        if m:
            ids[name] = int(m.group(1))
    return ids


def expected_bundle(net, node):
    """Bundle paths of node by the rules (logical paths): .c untagged or tagged with node;
    include/network/** without the host headers, nodes/<node>/include/network/** and the *.h of the
    folders of those .c (in firmware/network and in this node's folder: a header in another node's
    folder belongs to that node), minus headers tagged for other nodes only, plus headers tagged with
    node; nodes/<node>/board/s32k344_<node>/ .c/.h; README.md."""
    lines = list_lines(net)
    tagged_h = {logical(p): t for p, t in lines if p.endswith('.h')}
    cs = [logical(p) for p, t in lines if p.endswith('.c') and (not t or node in t)]
    home = net / NODE_DIRS[node]
    heads = {fwd(h.relative_to(net)) for h in (net / 'include' / 'network').rglob('*.h')}
    heads |= {fwd(h.relative_to(home)) for h in (home / 'include' / 'network').rglob('*.h')}
    for folder in {c.rsplit('/', 1)[0] for c in cs}:
        for base in (net, home):
            heads |= {fwd(h.relative_to(base)) for h in (base / folder).glob('*.h')}
    heads -= set(HOST_HEADERS)
    heads = {h for h in heads if h not in tagged_h or node in tagged_h[h]}
    heads |= {h for h, t in tagged_h.items() if node in t}
    board = home / 'board' / ('s32k344_' + node.lower())
    extra = {fwd(f.relative_to(home)) for f in board.rglob('*') if f.suffix in ('.c', '.h')} if board.is_dir() else set()
    return set(cs) | heads | extra | {'README.md'}


class Case:
    def __init__(self, args):
        self.args = args
        self.tmp = tempfile.TemporaryDirectory(prefix='node_bundle_')
        self.root = pathlib.Path(self.tmp.name)
        self.failures = 0
        (self.root / 'tmp').mkdir()
        self.env = {k: v for k, v in os.environ.items()
                    if k not in ('GIT_DIR', 'GIT_WORK_TREE', 'GIT_INDEX_FILE', 'GIT_OBJECT_DIRECTORY',
                                 'GIT_COMMON_DIR', 'GIT_PREFIX')}
        tmp = str(self.root / 'tmp')
        self.env.update(TMP=tmp, TEMP=tmp, TMPDIR=tmp, GIT_CEILING_DIRECTORIES=fwd(self.root))
        if args.cc and os.path.dirname(args.cc):
            self.env['PATH'] = os.path.dirname(args.cc) + os.pathsep + self.env.get('PATH', '')

    def close(self):
        self.tmp.cleanup()

    def check(self, cond, msg, detail=''):
        if not cond:
            self.failures += 1
            print('FAIL: %s' % msg)
            if detail:
                print('      ' + detail.strip().replace('\n', '\n      ')[-3000:])
        return cond

    def copy(self, where='fw/network'):
        dst = self.root / where
        shutil.copytree(str(NETWORK), str(dst), ignore=shutil.ignore_patterns('.build*', '__pycache__', '*.pyc',
                                                                              'tests'))
        return dst

    def run(self, cmd, cwd=None):
        r = subprocess.run([str(x) for x in cmd], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                           encoding='utf-8', errors='replace', env=self.env, cwd=str(cwd or self.root), timeout=600)
        return r.returncode, r.stdout

    def tool(self, net, *a):
        return self.run([sys.executable, '-B', net / 'tools' / 'sync_to_s32ds.py'] + list(a))

    def make(self, node, net, out):
        rc, o = self.tool(net, '--node', node, '--out', out, '--apply', '--allow-dirty')
        self.check(rc == 0 and (out / RECORD).is_file(), '%s: --apply --allow-dirty exit %d' % (node, rc), o)
        return o

    def git(self, repo, *a):
        return self.run(['git', '-c', 'user.name=bundle test', '-c', 'user.email=bundle@test.invalid',
                         '-c', 'commit.gpgsign=false', '-C', repo] + list(a))

    def git_repo(self, where):
        """firmware/network copy inside a new git repository with one commit, or None without git."""
        if not shutil.which('git'):
            print('note: git not found; git source-state part skipped')
            return None, None
        net = self.copy(where + '/firmware/network')
        repo = self.root / where
        for cmd in (('-c', 'init.defaultBranch=bundle-test', 'init', '-q'), ('add', '-A'), ('commit', '-q', '-m', 'x')):
            rc, o = self.git(repo, *cmd)
            if not self.check(rc == 0, 'temporary git repository: git %s' % ' '.join(cmd), o):
                return None, None
        return repo, net


def set_list(net, transform):
    path = net / 'tools' / 's32ds_sources.txt'
    path.write_text(transform(path.read_text(encoding='utf-8')), encoding='utf-8', newline='\n')


def replace_line(path, new):
    """List transform: replace the line of path (first token) by new."""
    def f(text):
        out = []
        for line in text.splitlines():
            tokens = line.split('#', 1)[0].split()
            out.append(new if tokens and tokens[0] == path else line)
        return '\n'.join(out) + '\n'
    return f


def drop_line(path):
    def f(text):
        return '\n'.join(l for l in text.splitlines() if (l.split('#', 1)[0].split() or [''])[0] != path) + '\n'
    return f


def append_line(new):
    return lambda text: text.rstrip('\n') + '\n' + new + '\n'


def forbidden_paths(paths):
    return [r for r in paths if re.search(r'(^|/)(sdk_mock|tests|tools|host_[^/]*)/', r)
            or (r.endswith('.md') and r != 'README.md') or r.endswith(('.csv', '.cmake'))
            or r.rsplit('/', 1)[-1] in ('CMakeLists.txt', 'FlexCAN_Ip.h', 'OsIf.h', 'Std_Types.h', 'Lpuart_Uart_Ip.h')]


# ------------------------------------------------------------------ cases

def case_tags(c):
    net = c.copy()
    sl = load_module(net / 'tools' / 'source_list.py', 'source_list_under_test')
    entries = sl.read()
    tags = {logical(e.path): set(e.tags) for e in entries}
    for path, nodes in sorted(MIN_TAGS.items()):
        c.check(tags.get(path) == nodes, 'list: %s should be tagged %s, is %s'
                % (path, ' '.join('@' + n for n in sorted(nodes)), tags.get(path)))
    for e in entries:
        c.check((net / e.path).is_file(), 'list: %s missing' % e.path)
        if e.path.endswith('.h'):
            c.check(bool(e.tags), 'list: header %s without a node tag' % e.path)
        homes = [n for n, d in NODE_DIRS.items() if e.path.startswith(d)]
        c.check(set(e.tags) == set(homes) if homes else len(e.tags) != 1,
                'list: %s with tags %s is not in its node folder' % (e.path, sorted(e.tags)))
    untagged = [e.path for e in entries if e.path.endswith('.c') and not e.tags]
    for node in NODES:
        want = [e.path for e in entries if e.path.endswith('.c') and (not e.tags or node in e.tags)]
        c.check(sl.node_sources(entries, node) == want, '%s: node_sources differs from the tag rule' % node)
    c.check(sl.board_sources(entries) == [e.path for e in entries if e.path.endswith('.c')
                                          and set(e.tags) != {'DOMAIN'}],
            'board_sources must drop only the DOMAIN-only .c')

    # CMake reads the same list with the same rules.
    list_file = net / 'tools' / 's32ds_sources.txt'
    for node in NODES:
        rc, o = c.run([c.args.cmake, '-DNETWORK_LIST=' + fwd(list_file), '-DNETWORK_ROOT=' + fwd(net),
                       '-DNETWORK_NODE=' + node, '-P', PROBE])
        selected = [l[len('SELECTED '):].strip() for l in o.splitlines() if l.startswith('SELECTED ')]
        common = [l[len('COMMON '):].strip() for l in o.splitlines() if l.startswith('COMMON ')]
        c.check(rc == 0, 'cmake reader: exit %d for the real list (%s)' % (rc, node), o)
        c.check(selected == sl.node_sources(entries, node), 'cmake reader: %s selection differs from Python' % node,
                '\n'.join(sorted(set(selected) ^ set(sl.node_sources(entries, node)))))
        c.check(common == untagged, 'cmake reader: untagged .c differ from Python (%s)' % node)

    # Malformed lists: every reader stops (tools exit 2, CMake FATAL_ERROR).
    fake = c.root / 'ws' / 'fake'
    (fake / 'src').mkdir(parents=True)
    (fake / '.cproject').write_text('<cproject/>\n', encoding='utf-8')
    nope = c.root / 'nope'
    original = list_file.read_text(encoding='utf-8')
    bad = {
        'unknown tag': replace_line(REGISTRY, REGISTRY + '   @WINDOW'),
        'lower-case tag': replace_line(REGISTRY, REGISTRY + '   @vss'),
        'tag without @': replace_line(REGISTRY, REGISTRY + '   VSS'),
        'repeated tag': replace_line(REGISTRY, REGISTRY + '   @VSS @VSS'),
        'bare @': replace_line(REGISTRY, REGISTRY + '   @'),
        'header line without a tag': append_line('include/network/net_types.h'),
        'tag without a path': append_line('@CIS'),
        'path listed twice': append_line(REGISTRY),
        'parent path': append_line('../network/src/l04_frame_codec/net_wire_registry.c'),
        'backslash path': replace_line(REGISTRY, REGISTRY.replace('/', '\\')),
        'absolute path': append_line('/src/l04_frame_codec/net_wire_common2.c'),
        'not a .c/.h file': append_line('src/l04_frame_codec/notes.txt @VSS'),
        'one node tag outside its node folder': replace_line(REGISTRY, REGISTRY + '   @VSS'),
        'other tag in a node folder': replace_line(NODE_PORT, NODE_PORT + '   @CIS'),
        'unknown node folder': append_line('nodes/window/' + REGISTRY),
        'same logical path': append_line(NODE_DIRS['VSS'] + REGISTRY + '   @VSS'),
        'semicolon': append_line('# a; b'),
        'square bracket': append_line('# see [x'),
    }
    # The node folder rules are checked with files that exist, so that no reader stops at the
    # missing file first; each reader must name the rule.
    rule_text = {'one node tag outside its node folder': 'belongs to one node',
                 'other tag in a node folder': 'must carry exactly the tag',
                 'unknown node folder': 'not in a node folder',
                 'same logical path': 'same logical path'}
    probes = [net / 'nodes' / 'window' / REGISTRY, net / NODE_DIRS['VSS'] / REGISTRY]
    for probe in probes:
        probe.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(str(net / REGISTRY), str(probe))
    for name, transform in bad.items():
        list_file.write_text(transform(original), encoding='utf-8', newline='\n')
        out = c.root / ('bad_' + re.sub(r'\W+', '_', name)) / 'network'
        rc, o = c.tool(net, '--node', 'VSS', '--out', out, '--apply', '--allow-dirty')
        c.check(rc == 2 and not out.exists(), '%s: --node --out exit %d (expected 2, nothing written)' % (name, rc), o)
        if name in rule_text:
            c.check(rule_text[name] in o, '%s: the tool does not name the rule (%r)' % (name, rule_text[name]), o)
        rc, o = c.tool(net, '--node', 'DOMAIN', '--check', out)
        c.check(rc == 2, '%s: --node --check exit %d (expected 2)' % (name, rc), o)
        rc, o = c.tool(net, '--project', fake, '--board', 'none')
        c.check(rc == 2, '%s: --project exit %d (expected 2)' % (name, rc), o)
        rc, o = c.run([sys.executable, '-B', net / 'tools' / 'sdk_swap_check.py', '--ds344', nope, '--ds144', nope,
                       '--arm-gcc-bin', nope, '--out', c.root / 'swap_out'])
        c.check(rc == 2, '%s: sdk_swap_check exit %d (expected 2)' % (name, rc), o)
        rc, o = c.run([c.args.cmake, '-DNETWORK_LIST=' + fwd(list_file), '-DNETWORK_ROOT=' + fwd(net),
                       '-DNETWORK_NODE=VSS', '-P', PROBE])
        c.check(rc != 0 and 's32ds_sources.txt' in o, '%s: cmake reader exit %d (expected FATAL_ERROR)' % (name, rc), o)
        if name in rule_text:
            c.check(rule_text[name] in o, '%s: the cmake reader does not name the rule (%r)' % (name, rule_text[name]), o)
    for probe in probes:
        probe.unlink()
    shutil.rmtree(str(net / 'nodes' / 'window'))
    # A listed file that does not exist: refused by the tools, FATAL_ERROR in CMake.
    list_file.write_text(append_line('src/l04_frame_codec/net_wire_missing.c')(original), encoding='utf-8',
                         newline='\n')
    rc, o = c.tool(net, '--node', 'VSS', '--out', c.root / 'missing' / 'network', '--apply', '--allow-dirty')
    c.check(rc == 1 and 'net_wire_missing.c' in o, 'missing listed file: --node exit %d (expected 1)' % rc, o)
    rc, o = c.run([c.args.cmake, '-DNETWORK_LIST=' + fwd(list_file), '-DNETWORK_ROOT=' + fwd(net),
                   '-DNETWORK_NODE=VSS', '-P', PROBE])
    c.check(rc != 0 and 'net_wire_missing.c' in o, 'missing listed file: cmake reader exit %d' % rc, o)
    # The real list again: every reader accepts it.
    list_file.write_text(original, encoding='utf-8', newline='\n')
    rc, o = c.tool(net, '--project', fake, '--board', 'none')
    c.check(rc == 0, 'real list: --project dry run exit %d' % rc, o)
    rc, o = c.run([sys.executable, '-B', net / 'tools' / 'sdk_swap_check.py', '--ds344', nope, '--ds144', nope,
                   '--arm-gcc-bin', nope, '--out', c.root / 'swap_out'])
    c.check(rc == 3 and 'SKIP' in o, 'real list: sdk_swap_check with missing paths exit %d (expected 3, SKIP)' % rc, o)
    # A short --arm-gcc-bin (fewer than four parent folders, e.g. at the drive root)
    # cannot give the S32DS software folder: still a missing path, not a crash.
    short = pathlib.Path(c.root.anchor) / 'nope_arm_gcc_bin'
    rc, o = c.run([sys.executable, '-B', net / 'tools' / 'sdk_swap_check.py', '--ds344', nope, '--ds144', nope,
                   '--arm-gcc-bin', short, '--out', c.root / 'swap_out'])
    c.check(rc == 3 and 'SKIP' in o, 'short --arm-gcc-bin: sdk_swap_check exit %d (expected 3, SKIP)' % rc, o)
    rc, o = c.tool(net, '--node', 'BCM', '--out', c.root / 'ok' / 'network', '--allow-dirty')
    c.check(rc == 0 and 'DRY RUN' in o, 'real list: --node dry run exit %d' % rc, o)


def bundle_basics(c, net, node, out):
    """Checks every bundle shares; returns the set of bundle paths."""
    have = set(files_of(out)) - {RECORD}
    want = expected_bundle(net, node)
    c.check(have == want, '%s: bundle files differ from the rules' % node,
            'extra: %s\nmissing: %s' % (sorted(have - want)[:10], sorted(want - have)[:10]))
    c.check(not forbidden_paths(have), '%s: forbidden files: %s' % (node, forbidden_paths(have)[:6]))
    for h in HOST_HEADERS:
        c.check(h not in have, '%s: host header %s in the bundle' % (node, h))
    for rel in have - {'README.md'}:
        c.check((out / rel).read_bytes() == source_of(net, rel).read_bytes(), '%s: %s is not a byte copy' % (node, rel))
    c.check((out / 'README.md').read_bytes() == integration(net, node).read_bytes(),
            '%s: README.md is not %s_NODE_INTEGRATION.md' % (node, node))
    for rel in ('include/network/net_upper_profile.h', 'src/platform/rtd/net_board_cfg.h', COMMON_PROBE) + RTD:
        c.check(rel in have, '%s: common file %s missing' % (node, rel))
    return have


def case_domain_contents(c):
    net = c.copy()
    out = c.root / 'DOMAIN' / 'network'
    c.make('DOMAIN', net, out)
    have = bundle_basics(c, net, 'DOMAIN', out)
    for rel in ADAPTERS + DOMAIN_HEADERS:
        c.check(rel in have, 'DOMAIN: %s missing' % rel)
    for rel in DOMAIN_ROOT + DOMAIN_BOARD:
        c.check(rel in have, 'DOMAIN: %s missing' % rel)
    for rel in tuple(r for r in ROOT if r not in DOMAIN_ROOT) + sum(OWN.values(), ()) + VSS_BOARD:
        c.check(rel not in have, 'DOMAIN: %s must not be in the bundle' % rel)
    c.check(sorted(r for r in have if r.startswith('src/node/')) == sorted(DOMAIN_NODE_DIR),
            'DOMAIN: src/node/ files other than the DOMAIN root in the bundle',
            str(sorted(r for r in have if r.startswith('src/node/'))))
    c.check(len([r for r in have if r.endswith('.c')]) == len([p for p, t in list_lines(net)
                                                               if p.endswith('.c') and (not t or 'DOMAIN' in t)]),
            'DOMAIN: .c count differs from the list')


def case_node_contents(c):
    net = c.copy()
    # Headers in a node folder that the list does not name belong to that node only.
    unlisted = (NODE_DIRS['CIS'] + M03 + 'net_cis_probe_private.h', NODE_DIRS['CIS'] + 'include/network/net_cis_probe.h')
    for rel in unlisted:
        (net / rel).parent.mkdir(parents=True, exist_ok=True)
        (net / rel).write_text('/* bundle probe */\n', encoding='utf-8', newline='\n')
    for node in ('VSS', 'CIS', 'BCM'):
        out = c.root / node / 'network'
        c.make(node, net, out)
        have = bundle_basics(c, net, node, out)
        for rel in unlisted:
            c.check((logical(rel) in have) == (node == 'CIS'), '%s: unlisted CIS folder header %s %s'
                    % (node, rel, 'missing' if node == 'CIS' else 'in the bundle'))
        for rel in ROOT + OWN[node]:
            c.check(rel in have, '%s: %s missing' % (node, rel))
        for rel in ADAPTERS + DOMAIN_HEADERS:
            c.check(rel not in have, '%s: DOMAIN file %s in the bundle' % (node, rel))
        c.check(not any(re.search(r'(^|/)net_domain_[^/]*\.c$', r) for r in have), '%s: net_domain_*.c in the bundle'
                % node)
        for other, files in OWN.items():
            if other != node:
                for rel in files:
                    c.check(rel not in have, '%s: %s file %s in the bundle' % (node, other, rel))
        for rel in VSS_BOARD:
            c.check((rel in have) == (node == 'VSS'), '%s: board/s32k344_vss file %s %s' % (
                node, rel, 'missing' if node == 'VSS' else 'present'))
        for rel in ('src/node/net_node_domain.c',) + DOMAIN_BOARD:
            c.check(rel not in have, '%s: DOMAIN file %s in the bundle' % (node, rel))


def parse_record(path):
    text = path.read_bytes().decode('utf-8')
    lines = text.split('\n')
    head = {}
    rows = []
    for line in lines[2:]:
        m = re.match(r'^([0-9a-f]{64})  (\S+)$', line)
        if m:
            rows.append((m.group(2), m.group(1)))
        elif re.match(r'^[a-z_]+: ', line):
            k, v = line.split(': ', 1)
            head[k] = v
    return text, lines, head, rows


def case_record(c):
    net = c.copy()
    ids = profile_ids(net)
    src = net / COMMON_PROBE
    lf = src.read_bytes().replace(b'\r\n', b'\n')
    crlf = lf.replace(b'\n', b'\r\n')
    src.write_bytes(lf)
    one, two = c.root / 'one' / 'network', c.root / 'two' / 'network'
    c.make('VSS', net, one)
    src.write_bytes(crlf)
    c.make('VSS', net, two)
    r1, r2 = (one / RECORD).read_bytes(), (two / RECORD).read_bytes()
    c.check(r1 == r2, 'record: an LF and a CRLF source give different records (hash must read CRLF as LF)')
    c.check((one / COMMON_PROBE).read_bytes() == lf and (two / COMMON_PROBE).read_bytes() == crlf,
            'record: files must be copied byte for byte')
    c.check(b'\r' not in r1, 'record: NETWORK_BUNDLE.txt must be LF only')
    text, lines, head, rows = parse_record(one / RECORD)
    c.check(lines[:2] == ['# Network sources for the VSS node, generated by firmware/network/tools/sync_to_s32ds.py.',
                          '# Do not edit files in this folder. Change firmware/network and generate again.'],
            'record: the two comment lines', '\n'.join(lines[:2]))
    want_head = ['format: 1', 'node: VSS', 'profile: NETWORK_UPPER_NODE_PROFILE=%d' % ids['VSS'],
                 'source: firmware/network', 'source_branch: unknown', 'source_commit: unknown',
                 'source_state: dirty', 'files: %d' % len(rows)]
    c.check(lines[2:10] == want_head, 'record: header lines (copy without git)', '\n'.join(lines[2:10]))
    c.check(text.endswith('\n') and lines[-1] == '' and all(re.match(r'^[0-9a-f]{64}  \S+$', l) for l in lines[10:-1]),
            'record: only file lines after the header, final newline')
    paths = [p for p, _ in rows]
    c.check(paths == sorted(paths), 'record: file lines must be sorted by path')
    c.check(set(paths) == set(files_of(one)) - {RECORD}, 'record: file lines differ from the folder')
    c.check('README.md' in paths and RECORD not in paths, 'record: README.md listed, NETWORK_BUNDLE.txt not')
    for p, digest in rows:
        c.check(digest == lf_sha(one / p) == lf_sha(source_of(net, p) if p != 'README.md' else integration(net, 'VSS')),
                'record: %s hash is not the LF-normalized SHA-256' % p)
    c.check(not re.search(r'\d{4}-\d\d-\d\d|\d\d:\d\d:\d\d', text), 'record: no time stamp')
    for node in NODES:
        out = c.root / ('p_' + node) / 'network'
        c.make(node, net, out)
        _, _, h, _ = parse_record(out / RECORD)
        c.check(h.get('node') == node and h.get('profile') == 'NETWORK_UPPER_NODE_PROFILE=%d' % ids[node],
                'record: %s node / profile line' % node, str(h))
    # A committed source: source_state clean, the commit and branch of HEAD.
    repo, gnet = c.git_repo('repo')
    if repo is not None:
        out = c.root / 'clean' / 'network'
        rc, o = c.tool(gnet, '--node', 'DOMAIN', '--out', out, '--apply')
        c.check(rc == 0, 'record: --apply from a committed source (no --allow-dirty) exit %d' % rc, o)
        _, gl, gh, _ = parse_record(out / RECORD) if (out / RECORD).is_file() else ('', [], {}, [])
        _, head_commit = c.git(repo, 'rev-parse', 'HEAD')
        c.check(gh.get('source_state') == 'clean', 'record: source_state %r (expected clean)' % gh.get('source_state'))
        c.check(gh.get('source_commit') == head_commit.strip() and re.match(r'^[0-9a-f]{40}$', head_commit.strip()),
                'record: source_commit %r (expected HEAD %r)' % (gh.get('source_commit'), head_commit.strip()))
        c.check(gh.get('source_branch') == 'bundle-test', 'record: source_branch %r' % gh.get('source_branch'))


def case_check_integrity(c):
    net = c.copy()
    out = c.root / 'b' / 'network'
    c.make('CIS', net, out)

    def check(expect, what, node='CIS'):
        before = snapshot(out)
        rc, o = c.tool(net, '--node', node, '--check', out)
        word = {0: 'OK', 1: 'EDITED', 3: 'STALE'}[expect]
        c.check(rc == expect and last_line(o) == 'CHECK %s %s' % (node, word),
                '%s: --check exit %d, last line %r (expected %d, CHECK %s %s)' % (what, rc, last_line(o), expect,
                                                                                  node, word), o)
        c.check(snapshot(out) == before, '%s: --check wrote into the bundle' % what)
        return o

    check(0, 'fresh bundle')
    victim = out / COMMON_PROBE
    keep = victim.read_bytes()
    victim.write_bytes(keep + b'\n/* edited */\n')
    o = check(1, 'edited file')
    c.check(COMMON_PROBE in o, 'edited file: the path is not reported', o)
    victim.write_bytes(keep.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
    check(0, 'file with CRLF line ends (Windows checkout)')
    victim.write_bytes(keep.replace(b'\r\n', b'\n'))
    check(0, 'file with LF line ends')
    victim.unlink()
    o = check(1, 'deleted file')
    c.check(COMMON_PROBE in o, 'deleted file: the path is not reported', o)
    victim.write_bytes(keep)
    check(0, 'restored file')
    foreign = out / 'src' / 'foreign_probe.c'
    foreign.write_text('/* not generated */\n', encoding='utf-8')
    o = check(1, 'foreign file')
    c.check('foreign_probe.c' in o, 'foreign file: the path is not reported', o)
    foreign.unlink()
    record = out / RECORD
    text = record.read_bytes()
    record.write_bytes(text.replace(b'\n', b'\r\n'))
    check(0, 'record with CRLF line ends (Windows checkout)')
    record.unlink()
    check(1, 'no record')
    record.write_bytes(text.replace(b'files: ', b'files: 1', 1))
    check(1, 'record with a wrong files count')
    record.write_bytes(text.replace(b'format: 1', b'format: 2', 1))
    check(1, 'record with another format')
    first = text.split(b'\n')[10]
    record.write_bytes(text.replace(first, first[:5] + (b'0' if first[5:6] != b'0' else b'1') + first[6:], 1))
    check(1, 'record with a changed hash')
    record.write_bytes(text.replace(b'# Do not edit', b'# Please do not edit', 1))
    check(1, 'record with an edited comment')
    record.write_bytes(text.replace(b'  README.md\n', b'  src/../README.md\n', 1))
    check(1, 'record with a path that leaves its part of the folder')
    record.write_bytes(text)
    check(0, 'restored record')
    check(1, 'record of another node', node='BCM')
    rc, o = c.tool(net, '--node', 'CIS', '--check', out, '--apply')
    c.check(rc == 2, '--check --apply exit %d (expected 2)' % rc, o)
    rc, o = c.tool(net, '--node', 'CIS', '--check', c.root / 'nothing' / 'network')
    c.check(rc == 1 and last_line(o) == 'CHECK CIS EDITED', 'missing folder: --check exit %d' % rc, o)


def case_check_stale(c):
    net = c.copy()
    out = c.root / 's' / 'network'

    def check(expect, what, path=None):
        rc, o = c.tool(net, '--node', 'BCM', '--check', out)
        word = {0: 'OK', 1: 'EDITED', 3: 'STALE'}[expect]
        c.check(rc == expect and last_line(o) == 'CHECK BCM %s' % word,
                '%s: --check exit %d, last line %r (expected %d)' % (what, rc, last_line(o), expect), o)
        if path:
            c.check(path in o, '%s: %s not reported' % (what, path), o)

    c.make('BCM', net, out)
    check(0, 'fresh bundle')
    src = net / COMMON_PROBE
    src.write_bytes(src.read_bytes() + b'\n/* changed in the source */\n')
    check(3, 'changed source', COMMON_PROBE)
    c.make('BCM', net, out)
    check(0, 'generated again after a source change')
    extra = net / 'include' / 'network' / 'net_bundle_probe.h'
    extra.write_text('#ifndef NET_BUNDLE_PROBE_H\n#define NET_BUNDLE_PROBE_H\n#endif\n', encoding='utf-8')
    check(3, 'new common header', 'include/network/net_bundle_probe.h')
    c.make('BCM', net, out)
    check(0, 'generated again after a new header')
    c.check((out / 'include' / 'network' / 'net_bundle_probe.h').is_file(), 'new header not in the bundle')
    extra.unlink()
    check(3, 'removed header', 'include/network/net_bundle_probe.h')
    set_list(net, replace_line(REGISTRY, REGISTRY + '   @VSS @CIS'))
    check(3, 'source tagged for other nodes', REGISTRY)
    c.make('BCM', net, out)
    check(0, 'generated again after the tag change')
    c.check(not (out / REGISTRY).exists() and not (out / 'include/network/net_bundle_probe.h').exists(),
            'files that left the bundle are still there')
    readme = integration(net, 'BCM')
    readme.write_text(readme.read_text(encoding='utf-8') + '\nchanged\n', encoding='utf-8')
    check(3, 'changed integration notes', 'README.md')


def case_apply_safety(c):
    # The copy sits below a folder named network, so that folder contains firmware/network.
    net = c.copy('network/repo/firmware/network')
    every = c.root / 'network'

    def refused(what, out, *extra, node='VSS', text=None):
        before_net, before_out = snapshot(every), snapshot(out)
        rc, o = c.tool(net, '--node', node, '--out', out, '--apply', *extra)
        c.check(rc == 1 and 'REFUSED' in o, '%s: --apply exit %d (expected 1, REFUSED)' % (what, rc), o)
        c.check(snapshot(every) == before_net and snapshot(out) == before_out, '%s: something was written' % what)
        if text:
            c.check(text in o, '%s: %r not reported' % (what, text), o)

    refused('folder not named network', c.root / 'out' / 'bundle', '--allow-dirty')
    refused('firmware/network itself', net, '--allow-dirty')
    refused('folder inside firmware/network', net / 'include' / 'network', '--allow-dirty')
    refused('new folder inside firmware/network', net / 'sub' / 'network', '--allow-dirty')
    refused('folder that contains firmware/network', every, '--allow-dirty')
    refused('uncommitted sources (no git) without --allow-dirty', c.root / 'd' / 'network', text='source_state dirty')
    refused('bring-up reference project', c.root / 'network_344_can_bringup' / 'network', '--allow-dirty')
    c.check(not (c.root / 'd' / 'network').exists(), 'dirty: output folder created')

    # A folder next to firmware/network (firmware/network_nodes/DOMAIN/network) is not inside it.
    sib = net.parent / 'network_nodes' / 'DOMAIN' / 'network'
    before_net = snapshot(net)
    rc, o = c.tool(net, '--node', 'DOMAIN', '--out', sib, '--apply', '--allow-dirty')
    c.check(rc == 0 and 'REFUSED' not in o and (sib / RECORD).is_file(),
            'sibling folder firmware/network_nodes/DOMAIN/network: --apply exit %d (expected 0)' % rc, o)
    c.check(snapshot(net) == before_net, 'sibling folder: firmware/network changed')
    rc, o = c.tool(net, '--node', 'DOMAIN', '--check', sib)
    c.check(rc == 0 and last_line(o) == 'CHECK DOMAIN OK', 'sibling folder: --check exit %d (expected CHECK DOMAIN OK)' % rc, o)

    out = c.root / 'e' / 'network'
    c.make('VSS', net, out)
    src = net / COMMON_PROBE
    src.write_bytes(src.read_bytes() + b'\n/* source change: the next run has work */\n')
    foreign = out / 'src' / 'foreign_probe.c'
    foreign.write_text('/* someone else\'s file */\n', encoding='utf-8')
    refused('folder with a file not in the record', out, '--allow-dirty', text='foreign_probe.c')
    foreign.unlink()
    victim = out / REGISTRY
    keep = victim.read_bytes()
    victim.write_bytes(keep + b'\n/* edited */\n')
    refused('folder with an edited file', out, '--allow-dirty', text=REGISTRY)
    victim.write_bytes(keep)
    record = out / RECORD
    rtext = record.read_bytes()
    record.unlink()
    refused('non-empty folder without a record', out, '--allow-dirty', text=RECORD)
    record.write_bytes(rtext.replace(b'files: ', b'files: 9', 1))
    refused('folder with a malformed record', out, '--allow-dirty')
    record.write_bytes(rtext)
    refused('folder of another node', out, '--allow-dirty', node='CIS')
    other = c.root / 'x' / 'network'
    other.mkdir(parents=True)
    (other / 'notes.txt').write_text('not a bundle\n', encoding='utf-8')
    refused('non-empty folder that is no bundle', other, '--allow-dirty')

    # Dry runs write nothing.
    before = snapshot(out)
    rc, o = c.tool(net, '--node', 'VSS', '--out', out, '--allow-dirty')
    c.check(rc == 0 and 'DRY RUN' in o and COMMON_PROBE in o and snapshot(out) == before,
            'dry run over a bundle: exit %d, must list the change and write nothing' % rc, o)
    rc, o = c.tool(net, '--node', 'VSS', '--out', c.root / 'f' / 'network', '--allow-dirty')
    c.check(rc == 0 and not (c.root / 'f').exists(), 'dry run into a new folder: exit %d, must write nothing' % rc, o)
    rc, o = c.tool(net, '--node', 'VSS', '--out', out, '--apply', '--allow-dirty')
    c.check(rc == 0, 'valid bundle with a source change: --apply exit %d' % rc, o)

    # Usage errors.
    for args in (('--node', 'VSS', '--project', c.root, '--out', out), ('--node', 'VSS', '--board', 'vss', '--out', out),
                 ('--node', 'VSS', '--out', out, '--check', out), ('--node', 'VSS'),
                 ('--node', 'VSS', '--check', out, '--allow-dirty'), ('--out', out), ('--check', out),
                 ('--node', 'WINDOW', '--out', out), ()):
        before = snapshot(out)
        rc, o = c.tool(net, *args)
        c.check(rc == 2 and snapshot(out) == before, 'usage %s: exit %d (expected 2)' % (' '.join(map(str, args)), rc), o)

    # Committed and uncommitted sources in a real (temporary) git repository.
    repo, gnet = c.git_repo('g')
    if repo is not None:
        gout = c.root / 'gout' / 'network'
        rc, o = c.tool(gnet, '--node', 'DOMAIN', '--out', gout, '--apply')
        c.check(rc == 0, 'git: committed sources, --apply exit %d' % rc, o)
        gsrc = gnet / COMMON_PROBE
        gkeep = gsrc.read_bytes()
        gsrc.write_bytes(gkeep + b'\n/* uncommitted */\n')
        before = snapshot(gout)
        rc, o = c.tool(gnet, '--node', 'DOMAIN', '--out', gout, '--apply')
        c.check(rc == 1 and 'source_state dirty' in o and snapshot(gout) == before,
                'git: modified bundle source, --apply exit %d (expected 1, nothing written)' % rc, o)
        rc, o = c.tool(gnet, '--node', 'DOMAIN', '--out', gout, '--apply', '--allow-dirty')
        _, _, gh, _ = parse_record(gout / RECORD)
        c.check(rc == 0 and gh.get('source_state') == 'dirty', 'git: --allow-dirty must write source_state dirty', o)
        gsrc.write_bytes(gkeep)
        rc, o = c.tool(gnet, '--node', 'DOMAIN', '--out', gout, '--apply')
        c.check(rc == 0, 'git: sources committed again, --apply exit %d' % rc, o)
        new_header = gnet / 'include' / 'network' / 'net_bundle_untracked.h'
        new_header.write_text('/* untracked */\n', encoding='utf-8')
        rc, o = c.tool(gnet, '--node', 'DOMAIN', '--out', gout, '--apply')
        c.check(rc == 1 and 'source_state dirty' in o, 'git: untracked bundle header, --apply exit %d' % rc, o)
        new_header.unlink()
        cm = gnet / 'CMakeLists.txt'
        cm.write_bytes(cm.read_bytes() + b'\n# not part of a bundle\n')
        rc, o = c.tool(gnet, '--node', 'DOMAIN', '--out', gout, '--apply')
        c.check(rc == 0, 'git: change outside the bundle, --apply exit %d (expected 0)' % rc, o)

        # The same repository after the merge: the bundle sits next to firmware/network.
        gsib = gnet.parent / 'network_nodes' / 'DOMAIN' / 'network'
        rc, o = c.tool(gnet, '--node', 'DOMAIN', '--out', gsib, '--apply')
        state = parse_record(gsib / RECORD)[2].get('source_state') if (gsib / RECORD).is_file() else None
        c.check(rc == 0 and state == 'clean',
                'git: sibling folder in the same repository, --apply exit %d, source_state %s (expected 0, clean)'
                % (rc, state), o)
        rc, o = c.tool(gnet, '--node', 'DOMAIN', '--out', gsib, '--apply')
        c.check(rc == 0 and re.search(r'\bnew 0, changed 0\b', o) is not None,
                'git: sibling folder, second --apply must change nothing (exit %d)' % rc, o)


def case_idempotent(c):
    net = c.copy()
    out = c.root / 'i' / 'network'
    # A listed source alone in its folder, with a header of that folder (copied by
    # the folder rule): both leave the bundle with the list line, and the folder too.
    port_c = 'src/bundle_probe/net_bundle_probe.c'
    port_h = 'src/bundle_probe/net_bundle_probe.h'
    (net / 'src' / 'bundle_probe').mkdir()
    (net / port_h).write_text('/* bundle probe */\nextern int net_bundle_probe;\n', encoding='utf-8', newline='\n')
    (net / port_c).write_text('#include "net_bundle_probe.h"\nint net_bundle_probe;\n', encoding='utf-8',
                              newline='\n')
    set_list(net, append_line(port_c))
    c.make('DOMAIN', net, out)
    before = snapshot(out)
    rc, o = c.tool(net, '--node', 'DOMAIN', '--out', out, '--apply', '--allow-dirty')
    c.check(rc == 0 and 'new 0, changed 0' in o and 'delete 0' in o, 'second --apply: exit %d, must change nothing'
            % rc, o)
    c.check(snapshot(out) == before, 'second --apply rewrote files (contents or modification times changed)')
    # Sources that leave the bundle are deleted, and folders that become empty go too.
    c.check((out / port_c).is_file() and (out / port_h).is_file(), 'precondition: probe files in the bundle')
    set_list(net, drop_line(port_c))
    gone_h = 'include/network/net_wire_can_bcm.h'
    (net / gone_h).unlink()
    rc, o = c.tool(net, '--node', 'DOMAIN', '--out', out, '--apply', '--allow-dirty')
    c.check(rc == 0, 'apply after removing sources: exit %d' % rc, o)
    for rel in (port_c, port_h, gone_h):
        c.check(not (out / rel).exists(), 'left the bundle but still there: %s' % rel)
        c.check(rel in o, 'deletion of %s not reported' % rel, o)
    c.check(not (out / 'src' / 'bundle_probe').exists(), 'empty folder src/bundle_probe not removed')
    text = (out / RECORD).read_text(encoding='utf-8')
    c.check(port_c not in text and gone_h not in text, 'record still lists deleted files')
    rc, o = c.tool(net, '--node', 'DOMAIN', '--check', out)
    c.check(rc == 0, 'check after the update: exit %d' % rc, o)
    before = snapshot(out)
    rc, o = c.tool(net, '--node', 'DOMAIN', '--out', out, '--apply', '--allow-dirty')
    c.check(rc == 0 and snapshot(out) == before, 'third --apply changed the bundle', o)


def manifest_paths(c, output):
    m = re.search(r'^manifest: (.+)$', output, re.M)
    if not c.check(m is not None, 'no manifest line', output):
        return set()
    with open(m.group(1).strip(), encoding='utf-8') as fh:
        return {f['path'] for f in json.load(fh)['files']}


def case_board_set(c):
    net = c.copy()
    with open(FIXTURE, encoding='utf-8') as fh:
        ref = json.load(fh)['boards']
    ws = c.root / 'ws'
    bench, vss = ws / 'fake_bench', ws / 'fake_vss'
    for p in (bench, vss):
        (p / 'src').mkdir(parents=True)
        (p / '.cproject').write_text('<cproject/>\n', encoding='utf-8')
    (vss / 'src' / 'app').mkdir()
    (vss / 'src' / 'app' / 'vss_main.c').write_text('/* fake */\n', encoding='utf-8')
    projects = {'bench': bench, 'vss': vss, 'none': bench}
    got = {}
    for board in ('bench', 'vss', 'none'):
        rc, o = c.tool(net, '--project', projects[board], '--board', board)
        c.check(rc == 0, '--board %s dry run exit %d' % (board, rc), o)
        got[board] = manifest_paths(c, o)
        missing = sorted(set(ref[board]) - got[board] - set(MCU_WRAPPER))
        c.check(not missing, '--board %s no longer copies %d file(s) of the set before the node tags: %s'
                % (board, len(missing), missing[:8]))
        for rel in MCU_WRAPPER:
            c.check(rel not in got[board], '--board %s still copies the fixed-MCU wrapper %s' % (board, rel))
        added = sorted(got[board] - set(ref[board]))
        if added:
            print('note: --board %s copies %d new file(s): %s' % (board, len(added), added[:8]))
        for rel in ADAPTERS + ('include/network/net_domain_c2.h',) + HOST_HEADERS:
            c.check('network/' + rel not in got[board], '--board %s copies %s' % (board, rel))
    rc, o = c.tool(net, '--project', vss, '--board', 'vss', '--apply')
    c.check(rc == 0, '--board vss --apply exit %d' % rc, o)
    on_disk = {'network/' + r for r in files_of(vss / 'src' / 'network')} | {
        'net_board/' + r for r in files_of(vss / 'src' / 'net_board')}
    c.check(on_disk == got['vss'], '--board vss --apply wrote other files than its manifest')
    rc, o = c.run([sys.executable, '-B', '-c', 'import sys; sys.path.insert(0, sys.argv[1]); import sdk_swap_check as s; '
                   'print("\\n".join(s.product_units()))', net / 'tools'])
    units = [l.strip() for l in o.splitlines() if l.strip()]
    c.check(rc == 0 and {logical(u) for u in units} == {p[len('network/'):] for p in got['none'] if p.endswith('.c')},
            'sdk_swap_check compiles other .c files than the board project copy', o)


def compile_probe(c, source, incs, define):
    obj = c.root / 'probe.o'
    cc = c.args.cc
    if os.path.basename(cc).lower().startswith('cl'):
        cmd = [cc, '/nologo', '/c', '/Fo' + str(obj)] + (['/D' + define] if define else []) + ['/I' + str(i) for i in incs]
    else:
        cmd = [cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-c', '-o', obj] + (
            ['-D' + define] if define else []) + ['-I' + str(i) for i in incs]
    return c.run(cmd + [source])


def case_profile_required(c):
    if not c.check(bool(c.args.cc), 'no C compiler given (--cc)'):
        return
    ids = profile_ids(NETWORK)
    c.check(set(ids) == set(NODES) | {'HOST'}, 'net_upper_profile.h: NET_UPPER_PROFILE_* defines %s' % ids)
    message = 'NETWORK_UPPER_NODE_PROFILE is not defined (set it in the build: %s)' % ', '.join(
        '%s=%d' % (n, ids.get(n, -1)) for n in ('VSS', 'CIS', 'BCM', 'DOMAIN', 'HOST'))
    probes = {
        'net_upper_profile.h': ('#include "network/net_upper_profile.h"\n', [NETWORK / 'include']),
        'net_board_cfg.h': ('#include "net_board_cfg.h"\n', [NETWORK / 'include', NETWORK / 'src' / 'platform' / 'rtd']),
    }
    for name, (head, incs) in probes.items():
        tu = c.root / ('probe_%s.c' % name.replace('.', '_'))
        tu.write_text(head + 'int net_profile_probe;\n', encoding='utf-8')
        rc, o = compile_probe(c, tu, incs, '')
        c.check(rc != 0 and message in o, '%s without NETWORK_UPPER_NODE_PROFILE: exit %d, expected the error %r'
                % (name, rc, message), o)
        for node, pid in sorted(ids.items()):
            rc, o = compile_probe(c, tu, incs, 'NETWORK_UPPER_NODE_PROFILE=%d' % pid)
            c.check(rc == 0, '%s with NETWORK_UPPER_NODE_PROFILE=%d (%s): exit %d' % (name, pid, node, rc), o)


CASES = {
    'tags': case_tags, 'domain_contents': case_domain_contents, 'node_contents': case_node_contents,
    'record': case_record, 'check_integrity': case_check_integrity, 'check_stale': case_check_stale,
    'apply_safety': case_apply_safety, 'idempotent': case_idempotent, 'board_set': case_board_set,
    'profile_required': case_profile_required,
}


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    p.add_argument('case', choices=sorted(CASES))
    p.add_argument('--cc', default='', help='C compiler of the build (profile_required)')
    p.add_argument('--cmake', default='cmake', help='cmake executable (tags: the CMake list reader)')
    args = p.parse_args()
    c = Case(args)
    try:
        CASES[args.case](c)
    except Exception as e:  # report, never leave the temporary folder behind
        c.check(False, 'exception: %r' % (e,))
        raise
    finally:
        c.close()
    if c.failures:
        print('FAIL tools.node_bundle_%s (%d)' % (args.case, c.failures))
        return 1
    print('PASS tools.node_bundle_%s' % args.case)
    return 0


if __name__ == '__main__':
    sys.exit(main())
