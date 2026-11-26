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

DOMAIN node (--domain-owner-src <DOMAIN src folder>, with --domain-c2-include):
the DOMAIN node composition root (nodes/domain/src/node/net_node_domain.c), the DOMAIN board
file and the actual DOMAIN runtime, compiled by path from the read-only DOMAIN
sources, run as one more worker on the Host virtual backend (its own tree,
SDK-checked as well). A second node run then has this worker as the DOMAIN
side of CAN and runs the DOMAIN checks (network_virtual_nodes.py
--domain-node). Without --domain-owner-src this run is left out.

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


def configure_and_build(args, source, build, profile, extra, target, logs, label=None):
    config = [args.cmake, '-S', str(source), '-B', str(build), '-DNETWORK_TARGET=host',
              '-DCMAKE_BUILD_TYPE=Debug', '-DBUILD_TESTING=OFF', '-DNETWORK_NODE_PROFILE=' + profile,
              '-DNETWORK_BUILD_WIRE_CANDIDATE=ON', '-DNETWORK_BUILD_UPPER_STACK=ON',
              '-DNETWORK_BUILD_HAL_PORT_ADAPTER=ON', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'] + extra
    if args.generator:
        config += ['-G', args.generator]
    if args.cc:
        config.append('-DCMAKE_C_COMPILER=' + args.cc)
    name = label or profile
    for stage, command in (('configure', config),
                           ('build', [args.cmake, '--build', str(build), '--target', target, '-j', str(args.jobs)])):
        result = run(command, logs / f'{name}_{stage}.log')
        if result.returncode:
            print(f'{name} {stage} failed (exit {result.returncode}):')
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


def node_run(source, root, extra, trace, log):
    """One run of network_virtual_nodes.py: (passed, failed) of its CHECK lines."""
    passed = failed = 0
    result = subprocess.run([sys.executable, '-B', str(source / 'tests' / 'virtual_ecu' / 'network_virtual_nodes.py'),
                             '--bin-dir', str(root), '--trace', str(root / trace)] + extra,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf8',
                            errors='replace')
    log.write_text(result.stdout, encoding='utf8')
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
    return passed, failed


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build-dir', required=True, type=pathlib.Path)
    parser.add_argument('--generator')
    parser.add_argument('--cc')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--domain-c2-include', type=pathlib.Path,
                        help='folder with the pinned ' + C2_HEADER + ' (optional)')
    parser.add_argument('--domain-owner-src', type=pathlib.Path,
                        help='DOMAIN src folder (application/, interface/), read only: the DOMAIN node worker '
                             'and its checks (optional, needs --domain-c2-include)')
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

    c2 = args.domain_c2_include
    owner = args.domain_owner_src
    domain_node = False
    if owner is not None:
        if c2 is None or not (c2 / C2_HEADER).is_file():
            failed += 1
            print(f'DOMAIN node FAIL: --domain-owner-src needs --domain-c2-include with {C2_HEADER}', flush=True)
        elif not (owner / 'application' / 'domain_runtime.c').is_file():
            failed += 1
            print(f'DOMAIN node FAIL: no application/domain_runtime.c in {owner}', flush=True)
        else:
            build = root / 'VIRTUAL_NODE_DOMAIN'
            extra = ['-DNETWORK_BUILD_VIRTUAL_NODE=ON', '-DNETWORK_BUILD_DOMAIN_C2=ON',
                     '-DNETWORK_DOMAIN_C2_INCLUDE_DIR=' + str(c2.resolve()),
                     '-DNETWORK_DOMAIN_C2_OWNER_SOURCE_DIR=' + str(owner.resolve())]
            if not configure_and_build(args, source, build, 'DOMAIN', extra, 'network_virtual_node_DOMAIN', logs,
                                       'DOMAIN_NODE'):
                print(f'RESULT passed=0 failed={failed + 1}')
                return 1
            print('DOMAIN: node worker built (network_virtual_node_DOMAIN: DOMAIN node root, C2 adapter, '
                  'DOMAIN runtime, no SDK)', flush=True)
            trees.append(build)
            domain_node = True

    count, bad = sdk_check(trees)
    if bad or count == 0:
        failed += 1
        print(f'SDK check FAIL: {len(bad)} offending compile commands of {count}')
        for line in bad[:20]:
            print('  ' + line)
    else:
        print(f'SDK check: {count} compile commands, no include path to the mock SDK, src/platform/rtd '
              f'or an NXP SDK folder', flush=True)

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

    runs = [([], 'TRACE.jsonl', 'nodes_run.log')]
    if domain_node:
        runs.append((['--domain-node'], 'TRACE_DOMAIN.jsonl', 'nodes_run_domain.log'))
    for extra, trace, log in runs:
        p, f = node_run(source, root, extra, trace, logs / log)
        passed, failed = passed + p, failed + f
    print(f'RESULT passed={passed} failed={failed}')
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
