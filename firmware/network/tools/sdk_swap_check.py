"""SDK swap compile: src/platform/rtd against the real RTD headers of two S32DS projects.

The RTD backend (src/platform/rtd/*.c) and the public entry that shares its
board hooks (src/node/net_entry.c) are compiled with ARM GCC 10.2,
-fsyntax-only, with the include paths of a real S32DS project instead of the
mock SDK:
  DS344: <project>/RTD/include + <project>/generate/include + PlatformSDK_S32K3
         BaseNXP (header, include) + Platform (include, startup/include)
  DS144: the same folders of the S32K1 RTD 3.0.0 PlatformSDK
No source file changes between the mock build and these builds: only include
paths and net_board_cfg.h -D overrides differ.

Board names come from the project's generated configuration, read here:
FlexCAN config / state symbols, the controller and error callback names typed
into ConfigTools, max_num_mb (message buffer layout), the LPUART config symbol
of the UART-C instance and its callback (shared with another instance ->
routing mode). Only values that differ from the net_board_cfg.h defaults are
passed as -D. Two probes also compile each generated config source together
with the HAL file that defines the callbacks, so a callback signature that
disagrees with the generated prototype is a compile error.

Variants per project: profile VSS and CIS x time base A (NET_BOARD_ISR_TICK_HOOK
+ NET_BOARD_TICK_TO_MS_HOOK) and B (NET_BOARD_NOW_MS_HOOK). Informational,
not counted: time base C (OsIf system counter), which needs
OSIF_USE_SYSTEM_TIMER ON in the project.

The S32DS projects are only read; every output goes to --out (summary.md,
logs/, probes/). A project file changing during the run is reported as an
error. An include path containing sdk_mock is an error.

Exit: 0 every counted compile has 0 errors, 1 errors, 3 a path is missing (skipped).
Example:
  python tools/sdk_swap_check.py --ds344 <ws>/network_344_can_bringup \\
      --ds144 <ws>/network_144_can_debuger --arm-gcc-bin <S32DS>/build_tools/gcc_v10.2/gcc-10.2-arm32-eabi/bin
"""
from __future__ import annotations
import argparse, datetime, hashlib, os, pathlib, re, subprocess, sys

NETWORK = pathlib.Path(__file__).resolve().parents[1]
UNITS = ('src/platform/rtd/net_hal_rtd.c', 'src/platform/rtd/net_hal_rtd_can.c',
         'src/platform/rtd/net_hal_rtd_uart.c', 'src/node/net_entry.c')
# Board project include folders of the Network sources (same as the host node build).
NETWORK_INCLUDES = ('include', 'src/platform/rtd', 'src/l04_frame_codec', 'src/l02_typed_handoff/m03_typed_handoff',
                    'src/l05_transport_port/m07_buffer_outcome', 'src/l05_transport_port/m09_observation',
                    'src/l03_message_service/m04_message_service')
PROJECT_WARNINGS = ('-std=c99', '-Wall', '-Wextra', '-Wstrict-prototypes', '-Wsign-compare',
                    '-Werror=implicit-function-declaration', '-Wundef', '-Wdouble-promotion', '-fno-short-enums')
PROFILES = {'VSS': 6, 'CIS': 1}
TIME_BASES = {
    'A': ('-DNET_BOARD_ISR_TICK_HOOK=Board_IsrTick', '-DNET_BOARD_TICK_TO_MS_HOOK=Board_TickToMs'),
    'B': ('-DNET_BOARD_NOW_MS_HOOK=Board_NowMs',),
}
# net_board_cfg.h defaults (the DS344 bring-up values).
DEFAULTS = {'NET_BOARD_CAN_CONFIG': 'FlexCAN_Config0', 'NET_BOARD_CAN_STATE': 'FlexCAN_State0',
            'NET_BOARD_CAN_CALLBACK': 'Can0_BringupCallback',
            'NET_BOARD_CAN_ERROR_CALLBACK': 'Can0_BringupErrorCallback',
            'NET_BOARD_CAN_MAX_MB': '16U', 'NET_BOARD_CAN_RX_MB_COUNT': '4U', 'NET_BOARD_CAN_TX_MB_FIRST': '8U',
            'NET_BOARD_UART_C_INSTANCE': '11U', 'NET_BOARD_UART_C_CONFIG': 'Lpuart_Uart_Ip_xHwConfigPB_11'}
TX_MB_USED = 4  # NET_HAL_TX_CAPACITY

TARGETS = {
    'DS344': dict(cpu=('-mcpu=cortex-m7', '-mthumb', '-mfloat-abi=hard', '-mfpu=fpv5-sp-d16'),
                  defines=('-DCPU_S32K344', '-DCPU_CORTEX_M7', '-DGCC'),
                  sdk='PlatformSDK_S32K3/RTD', base='BaseNXP_TS_T40D34M70I1R0', platform='Platform_TS_T40D34M70I1R0',
                  uart_c_instance=11, rtd='S32K3 RTD 7.0.x'),
    'DS144': dict(cpu=('-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=hard', '-mfpu=fpv4-sp-d16'),
                  defines=('-DCPU_S32K144HFT0VLLT', '-DCPU_CORTEX_M4F', '-DGCC'),
                  sdk='PlatformSDK_S32K1_S32M24/RTD', base='BaseNXP_TS_T40D2M30I0R0',
                  platform='Platform_TS_T40D2M30I0R0', uart_c_instance=0, rtd='S32K1 RTD 3.0.x'),
}


def fwd(p):
    return str(p).replace('\\', '/')


def read(path):
    return pathlib.Path(path).read_text(encoding='utf8', errors='replace')


def block(text, name):
    """Body of 'const <type> <name> = { ... };' in a generated source."""
    m = re.search(r'\b' + re.escape(name) + r'\s*=\s*\{(.*?)\n[ \t]*\};', text, re.S)
    return m.group(1) if m else None


def derive(project, uart_c_instance):
    """Board names from the generated configuration. Returns (names, problems)."""
    gen_inc, gen_src = project / 'generate' / 'include', project / 'generate' / 'src'
    names, problems = {}, []
    try:
        can_h, can_c = read(gen_inc / 'FlexCAN_Ip_Sa_PBcfg.h'), read(gen_src / 'FlexCAN_Ip_Sa_PBcfg.c')
        uart_h, uart_c = read(gen_inc / 'Lpuart_Uart_Ip_Sa_PBcfg.h'), read(gen_src / 'Lpuart_Uart_Ip_Sa_PBcfg.c')
    except OSError as e:
        return names, [f'generated config not readable: {e}']
    configs = re.findall(r'extern\s+const\s+Flexcan_Ip_ConfigType\s+(\w+)\s*;', can_h)
    states = re.findall(r'extern\s+Flexcan_Ip_StateType\s+(\w+)\s*;', can_h)
    if not configs or not states:
        problems.append('FlexCAN_Ip_Sa_PBcfg.h: no config / state symbol')
        return names, problems
    names['NET_BOARD_CAN_CONFIG'], names['NET_BOARD_CAN_STATE'] = configs[0], states[0]
    body = block(can_c, configs[0])
    if body is None:
        problems.append(f'FlexCAN_Ip_Sa_PBcfg.c: {configs[0]} not found')
        return names, problems
    mb = re.search(r'max_num_mb\s*\*/\s*\(uint8\)\s*(\d+)', body)
    cb = re.search(r'/\*\s*Controller Callback\s*\*/\s*&?\s*(\w+)', body)
    ecb = re.search(r'/\*\s*Error Callback\s*\*/\s*&?\s*(\w+)', body)
    if not (mb and cb and ecb):
        problems.append(f'FlexCAN_Ip_Sa_PBcfg.c: max_num_mb / callbacks of {configs[0]} not found')
        return names, problems
    max_mb = int(mb.group(1))
    names['NET_BOARD_CAN_CALLBACK'], names['NET_BOARD_CAN_ERROR_CALLBACK'] = cb.group(1), ecb.group(1)
    names['NET_BOARD_CAN_MAX_MB'] = f'{max_mb}U'
    # Layout of net_board_cfg.h: S32K344 (16 MBs) RX 0..3 / TX 8..11; S32K144 (7 MBs) RX 0..1 / TX 2..5.
    rx_count, tx_first = (4, 8) if max_mb >= 8 + TX_MB_USED else (2, 2)
    if tx_first + TX_MB_USED > max_mb:
        problems.append(f'max_num_mb {max_mb}: RX {rx_count} + TX {TX_MB_USED} MBs do not fit')
    names['NET_BOARD_CAN_RX_MB_COUNT'], names['NET_BOARD_CAN_TX_MB_FIRST'] = f'{rx_count}U', f'{tx_first}U'
    uart_cfgs = re.findall(r'extern\s+const\s+Lpuart_Uart_Ip_UserConfigType\s+(\w+)\s*;', uart_h)
    wanted = f'Lpuart_Uart_Ip_xHwConfigPB_{uart_c_instance}'
    if wanted not in uart_cfgs:
        problems.append(f'Lpuart_Uart_Ip_Sa_PBcfg.h: {wanted} not declared (have {uart_cfgs})')
        return names, problems
    names['NET_BOARD_UART_C_INSTANCE'], names['NET_BOARD_UART_C_CONFIG'] = f'{uart_c_instance}U', wanted
    callbacks = {}
    for cfg in uart_cfgs:
        b = block(uart_c, cfg)
        m = re.search(r'Callback to invoke for Uart Event\.\s*\*/\s*&?\s*(\w+)', b or '')
        if m:
            callbacks[cfg] = m.group(1)
    if wanted not in callbacks:
        problems.append(f'Lpuart_Uart_Ip_Sa_PBcfg.c: callback of {wanted} not found')
        return names, problems
    names['uart_callback'] = callbacks[wanted]
    names['uart_callback_shared'] = sorted(c for c, f in callbacks.items() if f == callbacks[wanted] and c != wanted)
    osif = re.search(r'#define\s+OSIF_USE_SYSTEM_TIMER\s+\((STD_\w+)\)', read(gen_inc / 'OsIf_Cfg.h'))
    names['OSIF_USE_SYSTEM_TIMER'] = osif.group(1) if osif else '?'
    names['_max_mb'] = max_mb
    return names, problems


def rtd_version(project):
    try:
        text = read(project / 'RTD' / 'include' / 'FlexCAN_Ip.h')
    except OSError:
        return '?'
    parts = [re.search(r'#define\s+FLEXCAN_IP_SW_%s_VERSION_H\s+(\d+)' % k, text) for k in ('MAJOR', 'MINOR', 'PATCH')]
    return '.'.join(m.group(1) for m in parts) if all(parts) else '?'


def snapshot(project):
    """SHA-256 of every file in the folders the compile reads (read-only proof)."""
    out = {}
    for sub in ('RTD/include', 'generate/include', 'generate/src'):
        for p in sorted((project / sub).rglob('*')):
            if p.is_file():
                out[fwd(p.relative_to(project))] = hashlib.sha256(p.read_bytes()).hexdigest()
    return out


def overrides(names):
    return [f'-D{k}={v}' for k, v in names.items() if k in DEFAULTS and DEFAULTS[k] != v]


def compile_one(gcc, flags, unit, log):
    command = [fwd(gcc), '-fsyntax-only'] + list(flags) + [fwd(unit)]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            encoding='utf8', errors='replace')
    log.write_text(' '.join(command) + '\n\n' + result.stdout, encoding='utf8')
    errors = len(re.findall(r':\d+:\d+: (?:fatal )?error:', result.stdout))
    if result.returncode != 0 and errors == 0:
        errors = 1
    net_root = fwd(NETWORK).lower()
    warn_lines = [l for l in result.stdout.splitlines() if ': warning:' in l]
    net_warn = sum(1 for l in warn_lines if fwd(l).lower().startswith(net_root) or 'probes/' in fwd(l).lower())
    return dict(command=command, errors=errors, warnings=len(warn_lines), network_warnings=net_warn,
                first_error=next((l for l in result.stdout.splitlines() if 'error' in l), ''))


def main():
    stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    default_out = pathlib.Path(os.environ.get('LOCALAPPDATA', os.path.expanduser('~'))) / 'Temp' / f'VSSNET_sdkswap_{stamp}'
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--ds344', type=pathlib.Path, required=True)
    p.add_argument('--ds144', type=pathlib.Path, required=True)
    p.add_argument('--arm-gcc-bin', type=pathlib.Path, required=True)
    p.add_argument('--platform-sdk', type=pathlib.Path,
                   help='S32DS software folder with PlatformSDK_* (default: derived from --arm-gcc-bin)')
    p.add_argument('--out', type=pathlib.Path, default=default_out)
    args = p.parse_args()

    gcc = args.arm_gcc_bin / ('arm-none-eabi-gcc.exe' if os.name == 'nt' else 'arm-none-eabi-gcc')
    sdk_root = args.platform_sdk or args.arm_gcc_bin.resolve().parents[3] / 'software'
    projects = {'DS344': args.ds344, 'DS144': args.ds144}
    missing = [str(x) for x in [gcc] if not x.is_file()]
    for key, proj in projects.items():
        t = TARGETS[key]
        for d in (proj / 'RTD' / 'include', proj / 'generate' / 'include',
                  sdk_root / t['sdk'] / t['base'], sdk_root / t['sdk'] / t['platform']):
            if not d.is_dir():
                missing.append(str(d))
    if missing:
        print('SKIP: path not found: ' + '; '.join(missing))
        return 3
    out = args.out.resolve()
    for d in (NETWORK, *projects.values()):
        if out == d.resolve() or d.resolve() in out.parents:
            print(f'--out must not be inside {d}')
            return 1
    (out / 'logs').mkdir(parents=True, exist_ok=True)
    (out / 'probes').mkdir(parents=True, exist_ok=True)

    rows, problems, sections = [], [], []
    for key, proj in projects.items():
        proj = proj.resolve()
        t = TARGETS[key]
        before = snapshot(proj)
        names, why = derive(proj, t['uart_c_instance'])
        problems += [f'{key}: {w}' for w in why]
        if why:
            continue
        base, plat = sdk_root / t['sdk'] / t['base'], sdk_root / t['sdk'] / t['platform']
        includes = [NETWORK / i for i in NETWORK_INCLUDES] + [
            proj / 'RTD' / 'include', proj / 'generate' / 'include', base / 'header', base / 'include',
            plat / 'include', plat / 'startup' / 'include']
        mock = [fwd(i) for i in includes if 'sdk_mock' in fwd(i).lower()]
        if mock:
            problems.append(f'{key}: mock SDK on the include path: {mock}')
            continue
        inc_flags = ['-I' + fwd(i) for i in includes]
        board = overrides(names)
        common = list(t['cpu']) + list(t['defines']) + list(PROJECT_WARNINGS) + board + inc_flags
        sections.append((key, proj, names, board, includes, rtd_version(proj)))

        def add(label, profile, time_base, mode, unit, extra, counted=True):
            flags = common + ['-DNETWORK_UPPER_NODE_PROFILE=%d' % PROFILES[profile]] + list(extra)
            log = out / 'logs' / f'{key}_{label}_{pathlib.Path(unit).stem}.log'
            r = compile_one(gcc, flags, unit, log)
            r.update(target=key, label=label, profile=profile, time=time_base, mode=mode,
                     unit=fwd(pathlib.Path(unit).relative_to(NETWORK) if NETWORK in pathlib.Path(unit).parents
                              else pathlib.Path(unit).name), counted=counted, log=log.name)
            rows.append(r)

        for profile in PROFILES:
            for time_base, hooks in TIME_BASES.items():
                # CIS: UART-C is routed by board code (callback shared or not, the
                # HAL then only provides NetHalRtdUart_OnEvent).
                mode = 'routing' if profile == 'CIS' else '-'
                for unit in UNITS:
                    add(f'{profile}_{time_base}', profile, time_base, mode, NETWORK / unit, hooks)
        # Probes: generated config source + the HAL file that defines its callbacks.
        probe_can = out / 'probes' / f'{key}_probe_can.c'
        probe_can.write_text(f'#include "{fwd(proj / "generate" / "src" / "FlexCAN_Ip_Sa_PBcfg.c")}"\n'
                             f'#include "{fwd(NETWORK / "src/platform/rtd/net_hal_rtd_can.c")}"\n', encoding='utf8')
        add('probe_can', 'CIS', 'B', 'callbacks vs generated prototypes', probe_can, TIME_BASES['B'])
        probe_uart = out / 'probes' / f'{key}_probe_uart.c'
        probe_uart.write_text(f'#include "{fwd(proj / "generate" / "src" / "Lpuart_Uart_Ip_Sa_PBcfg.c")}"\n'
                              f'#include "{fwd(NETWORK / "src/platform/rtd/net_hal_rtd_uart.c")}"\n', encoding='utf8')
        add('probe_uart', 'CIS', 'B', 'HAL-defined %s' % names['uart_callback'], probe_uart,
            TIME_BASES['B'] + ('-DNET_BOARD_UART_CALLBACK=' + names['uart_callback'],))
        # Informational: time base C (OsIf system counter).
        add('VSS_C', 'VSS', 'C', 'informational', NETWORK / 'src/platform/rtd/net_hal_rtd.c', (), counted=False)
        after = snapshot(proj)
        if before != after:
            changed = sorted(k for k in set(before) | set(after) if before.get(k) != after.get(k))
            problems.append(f'{key}: project files changed during the run: {changed[:10]}')

    counted = [r for r in rows if r['counted']]
    total_errors = sum(r['errors'] for r in counted) + len(problems)
    lines = [f'# SDK swap compile ({stamp})', '',
             f'- Network sources: `{fwd(NETWORK)}` (unchanged; only include paths and -D differ from the mock build)',
             f'- Compiler: `{fwd(gcc)}`', f'- Units: {", ".join("`%s`" % u for u in UNITS)}',
             f'- Result: **{"PASS" if total_errors == 0 else "FAIL"}**, counted compiles {len(counted)}, '
             f'errors {sum(r["errors"] for r in counted)}, problems {len(problems)}',
             '- Not counted: `VSS_C` (time base C, OsIf system counter). With OSIF_USE_SYSTEM_TIMER STD_OFF '
             'its expected result is the OSIF_COUNTER_SYSTEM error; a board build uses time base A or B.', '']
    for key, proj, names, board, includes, ver in sections:
        lines += [f'## {key} `{fwd(proj)}`', '',
                  f'- RTD version (FlexCAN_Ip.h): {ver} (expected {TARGETS[key]["rtd"]})',
                  f'- OSIF_USE_SYSTEM_TIMER: {names["OSIF_USE_SYSTEM_TIMER"]}',
                  f'- Generated names: CAN config `{names["NET_BOARD_CAN_CONFIG"]}`, state `{names["NET_BOARD_CAN_STATE"]}`, '
                  f'callbacks `{names["NET_BOARD_CAN_CALLBACK"]}` / `{names["NET_BOARD_CAN_ERROR_CALLBACK"]}`, '
                  f'max_num_mb {names["_max_mb"]}, UART-C `{names["NET_BOARD_UART_C_CONFIG"]}` callback '
                  f'`{names["uart_callback"]}` (shared with {", ".join(names["uart_callback_shared"]) or "none"})',
                  f'- net_board_cfg.h overrides: {" ".join("`%s`" % b for b in board) or "none (defaults match)"}',
                  '- Include paths (no sdk_mock):'] + [f'  - `{fwd(i)}`' for i in includes] + ['']
    lines += ['## Compiles', '', '| target | variant | time | callback mode | unit | errors | warnings (Network) | counted |',
              '|---|---|---|---|---|---|---|---|']
    for r in rows:
        lines.append(f'| {r["target"]} | {r["label"]} | {r["time"]} | {r["mode"]} | `{r["unit"]}` | {r["errors"]} | '
                     f'{r["warnings"]} ({r["network_warnings"]}) | {"yes" if r["counted"] else "no"} |')
    if problems:
        lines += ['', '## Problems', ''] + [f'- {x}' for x in problems]
    lines += ['', '## Command lines', '']
    for r in rows:
        lines += [f'### {r["target"]} {r["label"]} {r["unit"]}', '', '```', ' '.join(r['command']), '```', '']
    (out / 'summary.md').write_text('\n'.join(lines) + '\n', encoding='utf8')

    for r in rows:
        tag = 'info' if not r['counted'] else ('OK' if r['errors'] == 0 else 'ERR')
        print(f'{tag:4} {r["target"]} {r["label"]:10} {r["unit"]:36} errors={r["errors"]} warnings={r["warnings"]}'
              + (f'  {r["first_error"][:120]}' if r['errors'] and r['counted'] else ''))
    for x in problems:
        print('PROBLEM ' + x)
    print(f'summary: {fwd(out / "summary.md")}')
    print(f'SDK swap: {"PASS" if total_errors == 0 else "FAIL"} counted={len(counted)} errors={total_errors}')
    return 0 if total_errors == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
