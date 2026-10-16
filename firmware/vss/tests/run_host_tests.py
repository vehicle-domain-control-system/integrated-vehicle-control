"""Reproducible P1-A/C1/P1-B + P2/P3 C99 Host tests; no hardware access."""
import argparse
import json
import os
import pathlib
import shutil
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--out', type=pathlib.Path, default=HERE.parent / 'build' / 'host')
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
results = []
flags = ['-std=c99', '-Wall', '-Wextra', '-Werror', '-Wpedantic', '-Wconversion',
         '-Wshadow', '-Wstrict-prototypes', '-Wmissing-prototypes', '-I', str(HERE.parent / 'src')]
source_names = ['vss_input.c', 'vss_store.c', 'vss_policy.c', 'vss_store_playback.c',
                'vss_playback.c', 'vss_flow.c', 'vss_audio_backend.c']
sources = [str(HERE.parent / 'src' / name) for name in source_names]
tests = {'': str(HERE / 'test_input_store.c'), 'warning_': str(HERE / 'test_warning_hold.c'),
         'policy_': str(HERE / 'test_policy_store.c'),
         'audio_': str(HERE / 'test_audio_backend.c'),
         'playback_': str(HERE / 'test_playback_flow.c')}

def run(label, command):
    proc = subprocess.run(command, capture_output=True, text=True, errors='replace')
    item = {'label': label, 'command': command, 'returncode': proc.returncode,
            'stdout': proc.stdout, 'stderr': proc.stderr}
    results.append(item)
    (args.out / (label + '.txt')).write_text(json.dumps(item, indent=2), encoding='utf-8')
    print(label, 'PASS' if proc.returncode == 0 else 'FAIL', proc.returncode)
    if proc.stdout:
        print(proc.stdout.rstrip())
    if proc.stderr:
        print(proc.stderr.rstrip())
    return proc.returncode == 0

gcc = shutil.which('gcc')
if not gcc:
    raise SystemExit('GCC required; no compiler found')
run('gcc_version', [gcc, '--version'])
mandatory_ok = True
for name, source in zip([pathlib.Path(n).stem for n in source_names], sources):
    mandatory_ok &= run('production_' + name, [gcc, *flags, '-O2', '-fstack-usage', '-c', source,
                                               '-o', str(args.out / (name + '.o'))])

def build_and_run(compiler, suffix, options):
    ok = True
    for prefix, test in tests.items():
        label = prefix + suffix
        exe = args.out.resolve() / ('test_' + label + ('.exe' if os.name == 'nt' else ''))
        built = run('build_' + label, [compiler, *flags, *options, '-DVSS_HOST_TEST',
                                       *sources, test, '-o', str(exe)])
        ok &= built
        if built:
            ok &= run('run_' + label, [str(exe)])
    return ok

for optimization in ['O0', 'O2']:
    mandatory_ok &= build_and_run(gcc, optimization, ['-' + optimization, '-g'])

# Uses actual archived public headers and explicit HOST readiness stubs only.
# This is deliberately compiled WITHOUT VSS_HOST_TEST; not a target build.
for optimization in ['O0', 'O2']:
    exe = args.out.resolve() / ('target_gate_' + optimization + '.exe')
    built = run('build_target_gate_' + optimization,
        [gcc, *flags, '-' + optimization, '-DVSS_S32K344_REFERENCE_BINDING',
         '-DVSS_AUDIO_REFERENCE_HOST', '-I', str(HERE.parent / 'integration' / 's32k344'),
         '-I', str(HERE.parent / 'audio_reference' / 'host_shim'),
         str(HERE.parent / 'src' / 'vss_audio_backend.c'),
         str(HERE / 'test_audio_target_gate.c'), '-o', str(exe)])
    mandatory_ok &= built
    if built:
        mandatory_ok &= run('run_target_gate_' + optimization, [str(exe)])

# Probe actual instrumentation support; failures are kept as NOT_RUN evidence.
probe = args.out / 'sanitizer_probe.c'
probe.write_text('int main(void) { return 0; }\n', encoding='ascii')
for sanitizer in ['address', 'undefined']:
    probe_exe = args.out.resolve() / ('probe_' + sanitizer + '.exe')
    available = run('probe_' + sanitizer, [gcc, '-std=c99', '-fsanitize=' + sanitizer,
                                          str(probe), '-o', str(probe_exe)])
    if available:
        mandatory_ok &= build_and_run(gcc, sanitizer, ['-O1', '-g',
                      '-fsanitize=' + sanitizer, '-fno-omit-frame-pointer'])
    else:
        print(sanitizer, 'NOT_RUN: compiler/runtime probe unavailable')

clang = shutil.which('clang')
if clang:
    run('clang_version', [clang, '--version'])
    mandatory_ok &= build_and_run(clang, 'clang', ['-O2'])
else:
    print('Clang NOT_RUN: not found on PATH')
(args.out / 'results.json').write_text(json.dumps({'mandatory_pass': mandatory_ok, 'results': results}, indent=2), encoding='utf-8')
raise SystemExit(0 if mandatory_ok else 1)
