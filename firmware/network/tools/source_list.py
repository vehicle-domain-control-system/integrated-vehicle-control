"""Reader of tools/s32ds_sources.txt: the product source list with node tags.

Line format ('#' starts a comment, blank lines are ignored):

    <path relative to firmware/network>   [@NODE ...]      NODE: VSS CIS BCM DOMAIN

  - a .c line without a tag belongs to every node; a tagged .c line belongs
    only to the nodes it names
  - a .h line must carry tags: that header then belongs only to the named
    nodes. Headers that are not listed are common.
  - anything else is an error: an unknown or repeated tag, a header line
    without a tag, a path that is not a relative .c/.h path (no '..', no '\\',
    no drive or leading '/'), a path listed twice, and the characters ; [ ]
    anywhere in the file, comments included (CMake would split such a line
    differently)

Readers that use this module: tools/sync_to_s32ds.py (--project and --node)
and tools/sdk_swap_check.py. CMakeLists.txt (NETWORK_BUILD_RTD_MOCK) applies
the same rules with tools/s32ds_sources.cmake.
"""
from __future__ import annotations
import collections, pathlib, re

NETWORK = pathlib.Path(__file__).resolve().parents[1]
LIST_PATH = NETWORK / 'tools' / 's32ds_sources.txt'
PROFILE_HEADER = NETWORK / 'include' / 'network' / 'net_upper_profile.h'
NODES = ('VSS', 'CIS', 'BCM', 'DOMAIN')
PATH_RE = re.compile(r'^[A-Za-z0-9_][A-Za-z0-9_./-]*\.[ch]$')

Entry = collections.namedtuple('Entry', 'path tags line')


class ListError(ValueError):
    """A malformed list; .problems holds one message per bad line."""

    def __init__(self, problems):
        super().__init__('; '.join(problems))
        self.problems = list(problems)


def parse(text):
    """[Entry(path, frozenset of node names, line number)] in list order, or ListError."""
    entries, problems, seen = [], [], {}
    for number, raw in enumerate(text.splitlines(), 1):
        where = 'line %d' % number
        if any(c in raw for c in ';[]'):
            problems.append('%s: the characters ; [ ] are not allowed (also not in comments)' % where)
            continue
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        tokens = line.split()
        path, tags = tokens[0], tokens[1:]
        if path.startswith('@'):
            problems.append('%s: node tag without a path: %s' % (where, line))
            continue
        parts = path.split('/')
        if not PATH_RE.match(path) or '' in parts or '.' in parts or '..' in parts:
            problems.append('%s: %r is not a .c/.h path relative to firmware/network' % (where, path))
            continue
        names, bad = [], False
        for tag in tags:
            name = tag[1:] if tag.startswith('@') else None
            if name not in NODES:
                problems.append('%s: unknown node tag %r on %s (tags: @VSS @CIS @BCM @DOMAIN)' % (where, tag, path))
                bad = True
            elif name in names:
                problems.append('%s: node tag %s repeated on %s' % (where, tag, path))
                bad = True
            else:
                names.append(name)
        if bad:
            continue
        if path.endswith('.h') and not names:
            problems.append('%s: header line %s has no node tag (list only node-only headers)' % (where, path))
            continue
        if path in seen:
            problems.append('%s: %s is already listed on line %d' % (where, path, seen[path]))
            continue
        seen[path] = number
        entries.append(Entry(path, frozenset(names), number))
    if problems:
        raise ListError(problems)
    return entries


def read(path=LIST_PATH):
    return parse(pathlib.Path(path).read_text(encoding='utf-8'))


def node_sources(entries, node):
    """C sources of one node: untagged ones and the ones tagged with this node (list order)."""
    return [e.path for e in entries if e.path.endswith('.c') and (not e.tags or node in e.tags)]


def board_sources(entries):
    """C sources of an S32DS board project copy (--project): every one except the DOMAIN-only ones."""
    return [e.path for e in entries if e.path.endswith('.c') and e.tags != frozenset(('DOMAIN',))]


def node_headers(entries):
    """{listed header: frozenset of the nodes it belongs to}."""
    return {e.path: e.tags for e in entries if e.path.endswith('.h')}


def profile_ids(header=PROFILE_HEADER):
    """{node: NETWORK_UPPER_NODE_PROFILE value} from the NET_UPPER_PROFILE_<NODE> defines."""
    text = pathlib.Path(header).read_text(encoding='utf-8', errors='replace')
    out = {}
    for node in NODES:
        m = re.search(r'^[ \t]*#[ \t]*define[ \t]+NET_UPPER_PROFILE_%s[ \t]+(\d+)[ \t]*$' % node, text, re.M)
        if m:
            out[node] = int(m.group(1))
    return out
