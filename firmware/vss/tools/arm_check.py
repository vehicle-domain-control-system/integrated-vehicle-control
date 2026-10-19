#!/usr/bin/env python3
"""Cross-compiles firmware/vss/src for the target and reports stack and size.

Toolchain and vendor header locations come from the local environment file
.vss_work/env.json (S32 Design Studio compiler, driver headers, generated
configuration). When that file or any listed path is missing, the minimal
stand-in headers in tests/rtd_mock are used instead and the report says so.

Steps
  1. Compile every src/**/*.c with the project's compiler options plus
     -Wall -Wextra -Werror (vendor headers are treated as system headers).
  2. Compile tools/arm_rtd_probe.c: it uses every vendor entry point in the
     form the HAL calls it, so a changed vendor signature fails here. With real
     headers the probe is also compiled against tests/rtd_mock, which keeps the
     stand-in headers honest.
  3. Stack: with real headers the vendor driver sources are compiled as well
     (not part of the check, only for their stack figures). The worst call
     chain from main() and from each interrupt entry is computed from the
     compiler's call-graph output. Calls through function pointers cannot be
     followed and are listed.
  4. Size per object (arm-none-eabi-size).

Limits: main path <= 3072 bytes (the board's stack region is 4096 bytes);
main path + deepest interrupt chain + exception frame must fit in the region.

Usage: arm_check.py [--mock] [--src DIR] [--no-probe] [--no-vendor-stack] [-v]
Exit status 0 = every compile succeeded and the stack limits hold.
"""

import argparse
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
VSS = os.path.normpath(os.path.join(HERE, '..'))
DEFAULT_SRC = os.path.join(VSS, 'src')
MOCK_DIR = os.path.join(VSS, 'tests', 'rtd_mock')
PROBE = os.path.join(HERE, 'arm_rtd_probe.c')
REPO = os.path.normpath(os.path.join(VSS, '..', '..'))
WORK = os.path.join(REPO, '.vss_work')
ENV_FILE = os.path.join(WORK, 'env.json')

MAIN_STACK_LIMIT = 3072
STACK_REGION_BYTES = 4096
EXCEPTION_FRAME_BYTES = 104          # 8 core words + 18 FPU words, with lazy stacking
FRAME_WARN_BYTES = 256               # a single function frame this large hints at a big local
# Interrupt entry points and the callbacks the vendor handlers reach through
# function pointers (bound by name in the generated configuration).
ISR_ROOTS = {
    'Dma0_Ch0_IRQHandler': ('AudioPlayer_DmaTxIrqCallback',),
    'PIT_0_ISR': ('Cs2100Ref_Callback',),
    'AudioPlayer_Sai0IrqHandler': ('AudioPlayer_SaiTxCallback',),
}

WARN_FLAGS = ['-pedantic', '-Wall', '-Wextra', '-Werror', '-Wstrict-prototypes',
              '-Wsign-compare', '-Wundef', '-Wdouble-promotion',
              '-Werror=implicit-function-declaration']
STACK_FLAGS = ['-fstack-usage', '-fcallgraph-info=su']


def load_env():
    """Returns (env, why_not). env is None unless every needed path exists."""
    if not os.path.isfile(ENV_FILE):
        return None, 'local environment file not found'
    try:
        env = json.load(open(ENV_FILE, encoding='utf-8'))
        tc = env['toolchain']
        rtd = env['rtd']
        arm = env['arm_compile']
    except (OSError, ValueError, KeyError) as exc:
        return None, 'local environment file unreadable (%s)' % exc
    needed = [tc['gcc'], tc['size'], rtd['project_headers'], rtd['generate_include']] + list(arm['include_dirs'])
    missing = [p for p in needed if not os.path.exists(p)]
    if missing:
        return None, 'path(s) missing: ' + ', '.join(missing[:3]) + (' ...' if len(missing) > 3 else '')
    return env, ''


class Toolchain:
    def __init__(self, gcc, size, base_flags, include_dirs, mode):
        self.gcc, self.size = gcc, size
        self.base_flags = base_flags
        self.include_dirs = include_dirs
        self.mode = mode           # 'REAL' or 'MOCK'

    def cmd(self, source, obj, extra=()):
        c = [self.gcc] + self.base_flags
        for d in self.include_dirs:
            c += ['-isystem', d]
        return c + list(extra) + ['-c', source, '-o', obj]


def make_toolchain(env, mock):
    if env is None or mock:
        return None
    arm = env['arm_compile']
    flags = [arm['std'], arm['optimization']] + arm['cflags'] + arm['specs'] + \
            ['--sysroot=' + env['toolchain']['sysroot']] + \
            ['-D' + d for d in arm['defines']]
    return Toolchain(env['toolchain']['gcc'], env['toolchain']['size'], flags,
                     list(arm['include_dirs']), 'REAL')


def make_mock_toolchain(env):
    """Same compiler and options as the real build, stand-in headers only."""
    if env is None:
        gcc = shutil.which('arm-none-eabi-gcc')
        if not gcc:
            return None
        size = shutil.which('arm-none-eabi-size')
        flags = ['-std=c99', '-Os', '-mcpu=cortex-m7', '-mthumb', '-mfloat-abi=hard',
                 '-mfpu=fpv5-sp-d16', '-funsigned-char', '-fno-short-enums',
                 '-funsigned-bitfields', '-fno-common']
    else:
        gcc, size = env['toolchain']['gcc'], env['toolchain']['size']
        arm = env['arm_compile']
        flags = [arm['std'], arm['optimization']] + arm['cflags'] + arm['specs'] + \
                ['--sysroot=' + env['toolchain']['sysroot']] + \
                ['-D' + d for d in arm['defines']]
    return Toolchain(gcc, size, flags, [MOCK_DIR], 'MOCK')


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def list_c(root):
    out = []
    for d, _, fs in os.walk(root):
        for f in fs:
            if f.endswith('.c'):
                out.append(os.path.join(d, f))
    return sorted(out)


def slug(root, path):
    return os.path.relpath(path, root).replace('\\', '/').replace('/', '__')


def compile_all(tc, sources, root, outdir, extra, strict, jobs=1):
    """Returns list of (source, obj or None, stderr)."""
    os.makedirs(outdir, exist_ok=True)

    def one(src):
        obj = os.path.join(outdir, slug(root, src) + '.o')
        cmd = tc.cmd(src, obj, (WARN_FLAGS if strict else ['-w']) + list(extra))
        r = run(cmd)
        return src, (obj if r.returncode == 0 else None), (r.stdout + r.stderr)

    if jobs <= 1:
        return [one(s) for s in sources]
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        return list(ex.map(one, sources))


NODE_RE = re.compile(r'node:\s*\{\s*title:\s*"([^"]+)"\s*label:\s*"([^"]*)"')
EDGE_RE = re.compile(r'edge:\s*\{\s*sourcename:\s*"([^"]+)"\s*targetname:\s*"([^"]+)"')
STACK_RE = re.compile(r'(\d+) bytes \(([^)]*)\)')


class CallGraph:
    def __init__(self):
        self.frame = {}      # title -> bytes
        self.kind = {}       # title -> 'static' | 'dynamic' | ...
        self.name = {}       # title -> display name
        self.where = {}      # title -> 'file:line:col'
        self.edges = {}      # title -> set(titles)
        self.indirect = set()
        self.origin = {}     # title -> 'src' | 'vendor'

    def add_file(self, ci_path, origin):
        try:
            text = open(ci_path, encoding='utf-8', errors='replace').read()
        except OSError:
            return
        for m in NODE_RE.finditer(text):
            title, label = m.group(1), m.group(2)
            parts = label.split('\\n')
            self.name[title] = parts[0]
            if len(parts) > 1:
                self.where.setdefault(title, parts[1])
            sm = STACK_RE.search(label)
            if sm:
                self.frame[title] = int(sm.group(1))
                self.kind[title] = sm.group(2)
                self.origin[title] = origin
        for m in EDGE_RE.finditer(text):
            s, t = m.group(1), m.group(2)
            if t == '__indirect_call':
                self.indirect.add(s)
            else:
                self.edges.setdefault(s, set()).add(t)

    def find(self, name):
        for t, n in self.name.items():
            if n == name and t in self.frame:
                return t
        return None

    def depth(self, root):
        """Worst stack from root: (bytes, path, unknown callees, cycle or None)."""
        memo, unknown, cycle = {}, set(), []

        def walk(t, stack):
            if t in memo:
                return memo[t]
            if t in stack:
                if not cycle:
                    cycle.append(' -> '.join(self.name.get(x, x) for x in stack[stack.index(t):] + [t]))
                return (0, [])
            if t not in self.frame:
                unknown.add(self.name.get(t, t))
                return (0, [])
            best = (0, [])
            for c in sorted(self.edges.get(t, ())):
                d = walk(c, stack + [t])
                if d[0] > best[0]:
                    best = d
            res = (self.frame[t] + best[0], [(self.name.get(t, t), self.frame[t])] + best[1])
            memo[t] = res
            return res

        d, path = walk(root, [])
        return d, path, sorted(unknown), (cycle[0] if cycle else None)

    def reachable(self, root):
        seen, todo = set(), [root]
        while todo:
            t = todo.pop()
            if t in seen:
                continue
            seen.add(t)
            todo.extend(self.edges.get(t, ()))
        return seen


def fmt_path(path):
    return ' -> '.join('%s(%d)' % p for p in path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--mock', action='store_true', help='use tests/rtd_mock instead of the vendor headers')
    ap.add_argument('--src', default=DEFAULT_SRC, help='source root (default: firmware/vss/src)')
    ap.add_argument('--no-probe', action='store_true', help='skip tools/arm_rtd_probe.c')
    ap.add_argument('--no-vendor-stack', action='store_true',
                    help='do not compile the vendor driver sources for their stack figures')
    ap.add_argument('--build-dir', help='where objects and stack files go')
    ap.add_argument('-v', '--verbose', action='store_true')
    args = ap.parse_args()

    src_root = os.path.normpath(os.path.abspath(args.src))
    env, why = load_env()
    real = make_toolchain(env, args.mock)
    mock = make_mock_toolchain(env)
    if real is None:
        reason = 'forced with --mock' if args.mock else why
        tc = mock
        if tc is None:
            print('arm_check: FAIL - no target compiler (%s)' % reason)
            return 2
    else:
        tc = real
        reason = ''
    ver = run([tc.gcc, '--version']).stdout.splitlines()[0]

    build = os.path.abspath(args.build_dir) if args.build_dir else (
        os.path.join(WORK, 'build', 'arm') if os.path.isdir(WORK) else os.path.join(os.environ.get('TEMP', '.'), 'vss_arm'))
    shutil.rmtree(build, ignore_errors=True)
    os.makedirs(build)

    lines = []          # report lines
    failures = []

    def out(s=''):
        print(s)
        lines.append(s)

    out('arm_check: compiler : %s' % ver)
    if tc.mode == 'REAL':
        out('arm_check: headers  : REAL vendor headers (RTD %s) and generated configuration'
            % env['rtd'].get('version', '?'))
    else:
        out('arm_check: headers  : MOCK (tests/rtd_mock) - %s' % reason)
        out('arm_check:            the vendor headers were NOT used; signatures are only as good as the stand-ins')

    # 1. sources ------------------------------------------------------------------
    sources = list_c(src_root)
    results = compile_all(tc, sources, src_root, os.path.join(build, 'src'), STACK_FLAGS, True)
    objs = []
    for src, obj, msg in results:
        rel = os.path.relpath(src, src_root).replace('\\', '/')
        if obj is None:
            failures.append('compile failed: ' + rel)
            out('  FAIL %s' % rel)
            for ln in msg.rstrip().splitlines()[:25]:
                out('       ' + ln)
        else:
            objs.append((rel, obj))
            if msg.strip() and args.verbose:
                out('  note %s: %s' % (rel, msg.strip()))
    out('arm_check: sources  : %d file(s), %d compiled cleanly' % (len(sources), len(objs)))

    # 2. probe --------------------------------------------------------------------
    if not args.no_probe:
        probes = [(tc, 'headers in use')]
        if tc.mode == 'REAL' and mock is not None:
            probes.append((mock, 'tests/rtd_mock'))
        for t, label in probes:
            res = compile_all(t, [PROBE], HERE, os.path.join(build, 'probe_' + t.mode), [], True)
            ok = res[0][1] is not None
            out('arm_check: probe    : %s (%s)' % ('ok' if ok else 'FAIL', label))
            if not ok:
                failures.append('probe does not compile against ' + label)
                for ln in res[0][2].rstrip().splitlines()[:25]:
                    out('       ' + ln)
        if tc.mode == 'REAL' and mock is not None:
            res = compile_all(mock, sources, src_root, os.path.join(build, 'src_mock'), [], True)
            bad = [os.path.relpath(s, src_root) for s, o, _ in res if o is None]
            out('arm_check: sources against tests/rtd_mock: %s' % ('ok' if not bad else 'FAIL ' + ', '.join(bad)))
            if bad:
                failures.append('sources do not compile against tests/rtd_mock')
                for s, o, m in res:
                    if o is None:
                        for ln in m.rstrip().splitlines()[:15]:
                            out('       ' + ln)

    # 3. stack --------------------------------------------------------------------
    cg = CallGraph()
    for rel, obj in objs:
        cg.add_file(obj[:-2] + '.ci', 'src')
    vendor_note = 'vendor driver stack not included (mock headers)'
    if tc.mode == 'REAL' and not args.no_vendor_stack:
        proj = env['project']
        vdirs = [os.path.join(proj['rtd_copy_dir'], 'src'), os.path.join(proj['generate_dir'], 'src'), proj['board_dir']]
        vsrc = []
        for d in vdirs:
            if os.path.isdir(d):
                vsrc += [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.endswith('.c')]
        t0 = time.time()
        vres = compile_all(tc, vsrc, os.path.dirname(proj['dir']), os.path.join(build, 'vendor'),
                           STACK_FLAGS, False, jobs=max(2, (os.cpu_count() or 2)))
        for s, o, m in vres:
            if o:
                cg.add_file(o[:-2] + '.ci', 'vendor')
        vendor_note = 'vendor driver sources compiled for stack figures: %d/%d (%.0fs)' % (
            sum(1 for r in vres if r[1]), len(vsrc), time.time() - t0)
    elif tc.mode == 'REAL':
        vendor_note = 'vendor driver stack not included (--no-vendor-stack)'

    out('arm_check: stack    : %s' % vendor_note)
    main_t = cg.find('main')
    stack_rows = []
    if main_t is None:
        out('arm_check: stack    : no main() in the sources yet - nothing to measure')
        main_depth = 0
    else:
        main_depth, path, unknown, cycle = cg.depth(main_t)
        out('arm_check: main path: %d bytes (limit %d)' % (main_depth, MAIN_STACK_LIMIT))
        out('             chain: ' + fmt_path(path))
        if cycle:
            failures.append('recursion in main path: ' + cycle)
            out('             RECURSION: ' + cycle)
        if unknown:
            out('             frames unknown for: ' + ', '.join(unknown))
        ind = sorted(cg.name.get(t, t) for t in cg.reachable(main_t) if t in cg.indirect)
        if ind:
            out('             calls through function pointers (not followed) in: ' + ', '.join(ind))
        if main_depth > MAIN_STACK_LIMIT:
            failures.append('main path stack %d > %d' % (main_depth, MAIN_STACK_LIMIT))
        isr_max, isr_name = 0, '-'
        for root in sorted(ISR_ROOTS):
            t = cg.find(root)
            if t is None:
                continue
            d, p, u, c = cg.depth(t)
            cbd = 0
            for cb in ISR_ROOTS[root]:
                ct = cg.find(cb)
                if ct is not None:
                    cbd = max(cbd, cg.depth(ct)[0])
            d += cbd           # upper bound: callback frames stacked on the handler's deepest chain
            out('arm_check: interrupt: %-26s %4d bytes (incl. callback %d)' % (root, d, cbd))
            if d > isr_max:
                isr_max, isr_name = d, root
        worst = main_depth + isr_max + EXCEPTION_FRAME_BYTES
        out('arm_check: worst total (main + %s + %d exception frame): %d of %d bytes'
            % (isr_name, EXCEPTION_FRAME_BYTES, worst, STACK_REGION_BYTES))
        out('             all three interrupts share one priority level, so they do not nest')
        if worst > STACK_REGION_BYTES:
            failures.append('worst-case stack %d > %d' % (worst, STACK_REGION_BYTES))

    big = sorted(((cg.frame[t], cg.name[t], cg.where.get(t, '')) for t in cg.frame
                  if cg.origin.get(t) == 'src' and cg.frame[t] >= FRAME_WARN_BYTES), reverse=True)
    dyn = sorted(cg.name[t] for t in cg.frame if cg.origin.get(t) == 'src' and 'dynamic' in cg.kind.get(t, ''))
    for b, n, w in big:
        out('arm_check: WARN frame of %s is %d bytes (%s): large local?' % (n, b, w))
    if dyn:
        failures.append('dynamic stack use in: ' + ', '.join(dyn))
        out('arm_check: FAIL dynamic stack allocation in: ' + ', '.join(dyn))
    top = sorted(((cg.frame[t], cg.name[t]) for t in cg.frame if cg.origin.get(t) == 'src'), reverse=True)[:8]
    if top:
        out('arm_check: largest frames in src: ' + ', '.join('%s=%d' % (n, b) for b, n in top))

    # 4. size ---------------------------------------------------------------------
    if objs and tc.size:
        r = run([tc.size, '--format=berkeley', '-t'] + [o for _, o in objs])
        out('arm_check: size (bytes, per object file; -Os):')
        for ln in r.stdout.rstrip().splitlines():
            out('  ' + ln)

    verdict = 'FAIL' if failures else 'PASS'
    out('arm_check: %s%s' % (verdict, ' - ' + '; '.join(failures) if failures else
                              ('' if tc.mode == 'REAL' else ' (MOCK headers: not a substitute for the real-header run)')))

    reports = os.path.join(WORK, 'reports')
    if os.path.isdir(reports):
        with open(os.path.join(reports, 'arm_check_report.txt'), 'w', encoding='utf-8') as fh:
            fh.write(time.strftime('%Y-%m-%d %H:%M:%S') + '\n' + '\n'.join(lines) + '\n')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
