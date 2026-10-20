"""HAL-less multi-node run: build and connect the node workers, no SDK.

Builds, each in its own CMake tree below --build-dir, the HAL-less node worker
(NETWORK_BUILD_VIRTUAL_NODE) for the node profiles VSS, CIS, BCM and the
HOST peer (DOMAIN on CAN, the Pi on UART-C), then runs
tests/virtual_ecu/network_virtual_nodes.py: CAN as envelopes, UART-C as bytes.
Every node's App Port facade has to carry at least one message across.

SDK independence: every compile command of these trees is checked; no include
path may point at the mock SDK, at src/platform/rtd or at an NXP SDK folder.

The DOMAIN profile needs the pinned C2 target header
(Domain_Interface_v0.5_C2_target.h, --domain-c2-include). Without it the
DOMAIN profile is excluded and reported; the DOMAIN side of the bus is then
the HOST peer (product service, node DOMAIN, test owner, no C2 adapter).

Example:
  python tools/virtual_nodes_run.py --build-dir .build-virtual --generator Ninja
Last line: RESULT passed=N failed=M (exit 1 when M > 0).
No ARM, SDK, Flash, hardware operation or Git mutation is performed.
"""
from __future__ import annotations
import argparse, json, pathlib, re, shlex, subprocess, sys

PROFILES = ('HOST', 'VSS', 'CIS', 'BCM')
C2_HEADER = 'Domain_Interface_v0.5_C2_target.h'
# Path fragments that must not appear on an include path of the HAL-less run.
FORBIDDEN = ('sdk_mock', 'platform/rtd', 'platformsdk', 'workspaces32ds', '/rtd/include', 'generate/include')


def run(command, log):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            encoding='utf8', errors='replace')
    log.write_text(result.stdout, encoding='utf8')
    return result


def include_paths(entry):
    if 'arguments' in entry:
        args = list(entry['arguments'])
    else:
        args = shlex.split(entry['command'], posix=False)
    paths = []
    for i, a in enumerate(args):
        a = a.strip('"')
        for flag in ('-I', '-isystem', '-iquote', '-idirafter'):
            if a == flag and i + 1 < len(args):
                paths.append(args[i + 1].strip('"'))
            elif a.startswith(flag) and len(a) > len(flag):
                paths.append(a[len(flag):])
    return paths


def sdk_check(trees):
    """(commands checked, offending entries) over the trees' compile_commands.json."""
    count, bad = 0, []
    for tree in trees:
        db = tree / 'compile_commands.json'
        if not db.is_file():
            bad.append(f'{tree.name}: compile_commands.json missing')
            continue
        for entry in json.loads(db.read_text(encoding='utf8')):
            count += 1
            source = entry.get('file', '').replace('\\', '/').lower()
            if any(f in source for f in FORBIDDEN):
                bad.append(f'{tree.name}: source {entry.get("file")}')
            for path in include_paths(entry):
                norm = path.replace('\\', '/').lower()
                if any(f in norm for f in FORBIDDEN):
                    bad.append(f'{tree.name}: include {path} ({entry.get("file")})')
    return count, bad


def configure_and_build(args, source, build, profile, extra, target, logs):
    config = [args.cmake, '-S', str(source), '-B', str(build), '-DNETWORK_TARGET=host',
              '-DCMAKE_BUILD_TYPE=Debug', '-DBUILD_TESTING=OFF', '-DNETWORK_NODE_PROFILE=' + profile,
              '-DNETWORK_BUILD_WIRE_CANDIDATE=ON', '-DNETWORK_BUILD_UPPER_STACK=ON',
              '-DNETWORK_BUILD_HAL_PORT_ADAPTER=ON', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'] + extra
    if args.generator:
        config += ['-G', args.generator]
    if args.cc:
        config.append('-DCMAKE_C_COMPILER=' + args.cc)
    for label, command in (('configure', config),
                           ('build', [args.cmake, '--build', str(build), '--target', target, '-j', str(args.jobs)])):
        result = run(command, logs / f'{profile}_{label}.log')
        if result.returncode:
            print(f'{profile} {label} failed (exit {result.returncode}):')
            print(result.stdout[-4000:])
            return False
    return True


def domain_c2_ready(build):
    exe = sorted(build.rglob('network_virtual_DOMAIN*'))
    exe = [p for p in exe if p.is_file() and p.suffix in ('', '.exe')]
    if len(exe) != 1:
        return False, f'network_virtual_DOMAIN: found {len(exe)}'
    p = subprocess.Popen([str(exe[0])], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         text=True)
    try:
        first = p.stdout.readline()
        p.stdin.write('quit\n')
        p.stdin.flush()
        p.wait(timeout=10)
        item = json.loads(first) if first.strip() else {}
    except Exception as e:
        p.kill()
        return False, str(e)
    ok = item.get('event') == 'ready' and item.get('profile') == 5 and p.returncode == 0
    return ok, json.dumps(item)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build-dir', required=True, type=pathlib.Path)
    parser.add_argument('--generator')
    parser.add_argument('--cc')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--domain-c2-include', type=pathlib.Path,
                        help='folder with the pinned ' + C2_HEADER + ' (optional)')
    args = parser.parse_args()
    source = pathlib.Path(__file__).resolve().parents[1]
    root = args.build_dir.resolve()
    logs = root / 'logs'
    logs.mkdir(parents=True, exist_ok=True)
    passed = failed = 0

    trees = []
    for profile in PROFILES:
        build = root / ('VIRTUAL_NODE_' + profile)
        role = 'PEER' if profile == 'HOST' else profile
        if not configure_and_build(args, source, build, profile, ['-DNETWORK_BUILD_VIRTUAL_NODE=ON'],
                                   'network_virtual_node_' + role, logs):
            print('RESULT passed=0 failed=1')
            return 1
        print(f'{profile}: node worker built (network_virtual_node_{role}, no SDK)', flush=True)
        trees.append(build)

    count, bad = sdk_check(trees)
    if bad or count == 0:
        failed += 1
        print(f'SDK check FAIL: {len(bad)} offending compile commands of {count}')
        for line in bad[:20]:
            print('  ' + line)
    else:
        print(f'SDK check: {count} compile commands, no include path to the mock SDK, src/platform/rtd '
              f'or an NXP SDK folder', flush=True)

    c2 = args.domain_c2_include
    if c2 is not None and (c2 / C2_HEADER).is_file():
        build = root / 'VIRTUAL_DOMAIN'
        extra = ['-DNETWORK_BUILD_VIRTUAL_ECU=ON', '-DNETWORK_BUILD_DOMAIN_C2=ON',
                 '-DNETWORK_DOMAIN_C2_INCLUDE_DIR=' + str(c2.resolve())]
        ok = configure_and_build(args, source, build, 'DOMAIN', extra, 'network_virtual_DOMAIN', logs)
        detail = 'build failed'
        if ok:
            ok, detail = domain_c2_ready(build)
        print(f'CHECK domain_c2_worker_ready {"PASS" if ok else "FAIL"} {detail}', flush=True)
        passed, failed = passed + (1 if ok else 0), failed + (0 if ok else 1)
    else:
        why = 'no --domain-c2-include given' if c2 is None else f'{C2_HEADER} not found in {c2}'
        print(f'DOMAIN profile excluded: {why}. The DOMAIN side of the bus is the HOST peer '
              f'(product service, node DOMAIN, test owner, no C2 adapter).', flush=True)

    result = subprocess.run([sys.executable, '-B', str(source / 'tests' / 'virtual_ecu' / 'network_virtual_nodes.py'),
                             '--bin-dir', str(root), '--trace', str(root / 'TRACE.jsonl')],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf8',
                            errors='replace')
    (logs / 'nodes_run.log').write_text(result.stdout, encoding='utf8')
    seen = 0
    for line in result.stdout.splitlines():
        m = re.match(r'CHECK (\S+) (PASS|FAIL)', line)
        if m:
            seen += 1
            print(line, flush=True)
            if m.group(2) == 'PASS':
                passed += 1
            else:
                failed += 1
    if result.returncode != 0 and seen == 0:
        failed += 1
        print(result.stdout[-4000:])
    elif result.returncode != 0 and failed == 0:
        failed += 1
        print(f'node run exit {result.returncode} without a failed check')
    print(f'RESULT passed={passed} failed={failed}')
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
