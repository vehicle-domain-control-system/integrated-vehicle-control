#!/usr/bin/env python3
"""Builds and runs the host-side tests of firmware/vss.

  1. Every src/**/*.c that does not touch the vendor drivers is compiled
     (-std=c99 -Wall -Wextra -Werror) and archived into libvss_host.a.
     Excluded: hal/s32k344/*.c (vendor drivers) and app/vss_main.c (entry point).
  2. Every tests/host/test_*.c is linked with the archive plus the support
     sources in tests/host (any .c whose name does not start with "test_")
     and run. A test program prints "RESULT passed=N failed=M".
     Exception: tests/host/test_hal_*.c link one board adapter source from
     hal/s32k344 with the recording vendor-driver stand-in tests/rtd_mock/*.c
     instead (no archive, no HAL stand-in).
     Steps 1-2 run once per build configuration (CONFIGS): the plain build,
     then the same sources and tests with the undefined-behaviour and bounds
     checks in trap mode (no runtime library needed). A trap ends the test
     program with an illegal-instruction exit and fails it. A probe program
     with a deliberate out-of-bounds read must trap, proving the checks are on.
  3. Configuration guards: out-of-range build switches must fail to compile.
  4. tools/check_layers.py --selftest.

Usage: host_test.py [--cc PATH] [--filter SUBSTR] [--build-dir DIR] [-v]
Exit status 0 = everything passed and at least one check ran.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
VSS = os.path.normpath(os.path.join(HERE, '..'))
SRC = os.path.join(VSS, 'src')
TESTS = os.path.join(VSS, 'tests', 'host')
RTD_MOCK = os.path.join(VSS, 'tests', 'rtd_mock')
REPO = os.path.normpath(os.path.join(VSS, '..', '..'))
WORK = os.path.join(REPO, '.vss_work')

# -funsigned-char matches the target, where plain char is unsigned.
# VSS_CFG_HOST_TEST=1 adds the drivers' reset functions used between cases.
CFLAGS = ['-std=c99', '-funsigned-char', '-Wall', '-Wextra', '-Werror', '-Wstrict-prototypes',
          '-Wundef', '-g', '-O0', '-DVSS_CFG_HOST_TEST=1']
# Build configurations: (row prefix, flags added to CFLAGS).
UBSAN_FLAGS = ['-fsanitize=undefined,bounds', '-fsanitize-undefined-trap-on-error']
CONFIGS = (
    ('', []),
    ('[ubsan] ', UBSAN_FLAGS),
)
UBSAN_PROBE = ('int main(int argc, char **argv)\n{\n    int a[4] = {0, 1, 2, 3};\n'
               '    (void)argv;\n    return a[argc + 3];\n}\n')
EXCLUDE_FROM_LIB = ('hal/s32k344/', 'app/vss_main.c')
# Board adapter tests: file name prefix -> adapter source(s) under src/.
HAL_TESTS = (
    ('test_hal_audio_tx', ('hal/s32k344/hal_audio_tx_s32k344.c',)),
    ('test_hal_board', ('hal/s32k344/hal_board_s32k344.c',)),
)
RESULT_RE = re.compile(r'^RESULT passed=(\d+) failed=(\d+)\s*$', re.M)
EXE = '.exe' if os.name == 'nt' else ''


def find_tools(cc_arg):
    cc, ar = cc_arg, None
    env_path = os.path.join(WORK, 'env.json')
    if os.path.isfile(env_path):
        try:
            host = json.load(open(env_path, encoding='utf-8')).get('host', {})
            cc = cc or host.get('gcc')
            ar = host.get('ar')
        except (OSError, ValueError):
            pass
    if not cc or not os.path.isfile(cc):
        cc = shutil.which('gcc') or shutil.which('clang')
    if not ar or not os.path.isfile(ar):
        ar = shutil.which('ar') or (os.path.join(os.path.dirname(cc), 'ar' + EXE) if cc else None)
    return cc, ar


def run(cmd, cwd=None, inp=None, timeout=300):
    return subprocess.run(cmd, cwd=cwd, input=inp, capture_output=True, text=True, timeout=timeout)


class Report:
    def __init__(self, verbose):
        self.rows = []          # (name, ok, passed, failed, note)
        self.verbose = verbose
        self.log = []

    def add(self, name, ok, passed=0, failed=0, note='', detail=''):
        self.rows.append((name, ok, passed, failed, note))
        line = '%-40s %-4s  passed=%-4d failed=%-3d %s' % (name, 'PASS' if ok else 'FAIL', passed, failed, note)
        print(line)
        self.log.append(line)
        if detail and (not ok or self.verbose):
            for d in detail.rstrip().splitlines():
                print('    ' + d)
                self.log.append('    ' + d)

    @property
    def ok(self):
        return bool(self.rows) and all(r[1] for r in self.rows)


def build_library(cc, ar, build, rep, flags, tag):
    objs = []
    srcs = []
    for d, _, fs in os.walk(SRC):
        for f in sorted(fs):
            if f.endswith('.c'):
                rel = os.path.relpath(os.path.join(d, f), SRC).replace('\\', '/')
                if not any(rel.startswith(x) for x in EXCLUDE_FROM_LIB):
                    srcs.append((rel, os.path.join(d, f)))
    srcs.sort()
    if not srcs:
        rep.add(tag + 'library (src/**/*.c)', True, note='no host-buildable sources yet')
        return None
    failed = 0
    for rel, path in srcs:
        obj = os.path.join(build, 'lib_' + rel.replace('/', '__') + '.o')
        r = run([cc] + flags + ['-c', path, '-o', obj])
        if r.returncode != 0:
            failed += 1
            rep.add(tag + 'compile ' + rel, False, 0, 1, detail=r.stdout + r.stderr)
        else:
            objs.append(obj)
    if failed:
        return None
    lib = os.path.join(build, 'libvss_host.a')
    if os.path.exists(lib):
        os.remove(lib)
    r = run([ar, 'rcs', lib] + objs)
    if r.returncode != 0:
        rep.add(tag + 'archive libvss_host.a', False, 0, 1, detail=r.stdout + r.stderr)
        return None
    rep.add(tag + 'library (src/**/*.c)', True, len(objs), 0, note='%d object(s) compiled' % len(objs))
    return lib


def hal_test_sources(name):
    """Adapter sources a test_hal_* program links, or None for a normal test."""
    for prefix, srcs in HAL_TESTS:
        if name.startswith(prefix):
            return [os.path.join(SRC, s) for s in srcs]
    if name.startswith('test_hal_'):
        return []
    return None


def build_and_run_tests(cc, build, lib, flt, rep, flags, tag):
    support = []
    tests = []
    for f in sorted(os.listdir(TESTS)):
        if not f.endswith('.c'):
            continue
        (tests if f.startswith('test_') else support).append(f)
    rtd_mock_srcs = [os.path.join(RTD_MOCK, f) for f in sorted(os.listdir(RTD_MOCK)) if f.endswith('.c')]
    if flt:
        tests = [t for t in tests if flt in t]
    support_objs = []
    for f in support:
        obj = os.path.join(build, 'sup_' + f + '.o')
        r = run([cc] + flags + ['-c', os.path.join(TESTS, f), '-o', obj, '-I', TESTS])
        if r.returncode != 0:
            rep.add(tag + 'compile tests/host/' + f, False, 0, 1, detail=r.stdout + r.stderr)
            return
        support_objs.append(obj)
    if not tests:
        rep.add(tag + 'tests/host/test_*.c', False, 0, 1, note='no test program found')
        return
    for f in tests:
        name = f[:-2]
        row = tag + name
        exe = os.path.join(build, name + EXE)
        adapter = hal_test_sources(name)
        if adapter is None:
            cmd = [cc] + flags + ['-I', TESTS, os.path.join(TESTS, f)] + support_objs
            if lib:
                cmd.append(lib)
        elif not adapter:
            rep.add(row, False, 0, 1, note='no adapter source listed in HAL_TESTS')
            continue
        else:
            cmd = [cc] + flags + ['-I', TESTS, '-I', RTD_MOCK, os.path.join(TESTS, f)] + \
                adapter + rtd_mock_srcs
        cmd += ['-o', exe]
        r = run(cmd)
        if r.returncode != 0:
            rep.add(row, False, 0, 1, note='build failed', detail=r.stdout + r.stderr)
            continue
        t0 = time.time()
        try:
            r = run([exe], cwd=build, timeout=120)
        except subprocess.TimeoutExpired:
            rep.add(row, False, 0, 1, note='timeout')
            continue
        m = RESULT_RE.search(r.stdout)
        if not m:
            rep.add(row, False, 0, 1, note='no RESULT line (exit %d)' % r.returncode,
                    detail=r.stdout + r.stderr)
            continue
        passed, failed = int(m.group(1)), int(m.group(2))
        ok = r.returncode == 0 and failed == 0 and passed > 0
        rep.add(row, ok, passed, failed, note='%.2fs' % (time.time() - t0),
                detail=r.stdout if not ok else '')


def ubsan_probe(cc, build, rep, tag):
    """The trap-mode checks must really be on: a deliberate out-of-bounds read traps."""
    src = os.path.join(build, 'ubsan_probe.c')
    exe = os.path.join(build, 'ubsan_probe' + EXE)
    with open(src, 'w', encoding='utf-8') as fh:
        fh.write(UBSAN_PROBE)
    r = run([cc, '-std=c99', '-O0'] + UBSAN_FLAGS + [src, '-o', exe])
    if r.returncode != 0:
        rep.add(tag + 'trap probe', False, 0, 1, note='build failed', detail=r.stdout + r.stderr)
        return
    r = run([exe], cwd=build, timeout=30)
    trapped = r.returncode != 0
    rep.add(tag + 'trap probe', trapped, 1 if trapped else 0, 0 if trapped else 1,
            note=('out-of-bounds read trapped (exit 0x%X)' % (r.returncode & 0xFFFFFFFF))
            if trapped else 'did not trap')


def config_guards(cc, rep):
    """Out-of-range build switches must be rejected by vss_config.h."""
    source = '#include "vss_config.h"\nint main(void) { return 0; }\n'
    inc = os.path.join(SRC, 'infra')
    base = [cc, '-std=c99', '-funsigned-char', '-Wall', '-Wextra', '-Werror', '-fsyntax-only', '-I', inc, '-x', 'c', '-']
    good = run(base, inp=source)
    rep.add('config: defaults accepted', good.returncode == 0, 1 if good.returncode == 0 else 0,
            0 if good.returncode == 0 else 1, detail=good.stderr)
    bad = [
        ('VSS_CFG_TEST_INPUT=2', 'must be 0 or 1'),
        ('VSS_CFG_NET_STUB=7', 'must be 0 or 1'),
        ('VSS_CFG_HOST_TEST=2', 'must be 0 or 1'),
        ('VSS_CFG_VOLUME_PERCENT=5', 'outside the allowed range'),
        ('VSS_CFG_VOLUME_PERCENT=101', 'outside the allowed range'),
        ('VSS_CFG_FLOW_PERIOD_MS=25', 'must be 1..20'),
        ('VSS_CFG_STORE_RECLAIM_MS=2000', 'must exceed'),
        ('VSS_CFG_RX_GAP_BUDGET_MS=500', 'must be 0..100'),
        ('VSS_CFG_STORE_RECLAIM_PERIOD_MS=5', 'must be 10..1000'),
    ]
    passed = failed = 0
    notes = []
    for define, expect in bad:
        r = run(base[:1] + ['-D' + define] + base[1:], inp=source)
        if r.returncode != 0 and expect in r.stderr:
            passed += 1
        else:
            failed += 1
            notes.append('%s was not rejected (%s)' % (define, expect))
    rep.add('config: invalid values rejected', failed == 0, passed, failed, detail='\n'.join(notes))


def tools_selftest(rep):
    r = run([sys.executable, '-I', os.path.join(HERE, 'check_layers.py'), '--selftest'])
    m = re.search(r'\((\d+) cases, (\d+) wrong\)', r.stdout)
    passed = int(m.group(1)) - int(m.group(2)) if m else 0
    failed = int(m.group(2)) if m else 1
    rep.add('check_layers --selftest', r.returncode == 0, passed, failed, detail=r.stdout)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--cc', help='host C compiler (default: from the local environment file, then PATH)')
    ap.add_argument('--filter', help='only run tests whose file name contains this text')
    ap.add_argument('--build-dir', help='where objects and executables go')
    ap.add_argument('-v', '--verbose', action='store_true')
    args = ap.parse_args()

    cc, ar = find_tools(args.cc)
    if not cc or not ar:
        print('host_test: no C compiler / ar found (gcc or clang needed)')
        return 2
    ver = run([cc, '--version']).stdout.splitlines()[0]
    print('host_test: %s' % ver)

    cleanup = None
    if args.build_dir:
        build = os.path.abspath(args.build_dir)
        os.makedirs(build, exist_ok=True)
    elif os.path.isdir(WORK):
        build = os.path.join(WORK, 'build', 'host')
        shutil.rmtree(build, ignore_errors=True)
        os.makedirs(build)
    else:
        build = tempfile.mkdtemp(prefix='vss_host_')
        cleanup = build

    rep = Report(args.verbose)
    for tag, extra in CONFIGS:
        sub = os.path.join(build, tag.strip(' []') or 'base')
        os.makedirs(sub, exist_ok=True)
        flags = CFLAGS + extra
        if extra == UBSAN_FLAGS:
            ubsan_probe(cc, sub, rep, tag)
        failures = sum(1 for r in rep.rows if not r[1])
        lib = build_library(cc, ar, sub, rep, flags, tag)
        if lib is not None or sum(1 for r in rep.rows if not r[1]) == failures:
            build_and_run_tests(cc, sub, lib, args.filter, rep, flags, tag)
    if not args.filter:
        config_guards(cc, rep)
        tools_selftest(rep)

    total_p = sum(r[2] for r in rep.rows)
    total_f = sum(r[3] for r in rep.rows)
    verdict = 'PASS' if rep.ok else 'FAIL'
    summary = 'host_test: %s  (%d checks passed, %d failed, %d groups)' % (verdict, total_p, total_f, len(rep.rows))
    print(summary)

    reports = os.path.join(WORK, 'reports')
    if os.path.isdir(reports):
        with open(os.path.join(reports, 'host_test.log'), 'w', encoding='utf-8') as fh:
            fh.write(time.strftime('%Y-%m-%d %H:%M:%S') + '\n' + ver + '\n')
            fh.write('\n'.join(rep.log) + '\n' + summary + '\n')
    if cleanup:
        shutil.rmtree(cleanup, ignore_errors=True)
    return 0 if rep.ok else 1


if __name__ == '__main__':
    sys.exit(main())
