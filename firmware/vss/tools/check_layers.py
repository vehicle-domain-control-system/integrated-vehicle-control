#!/usr/bin/env python3
"""Checks the include direction between the source layers of firmware/vss/src.

  app  ->  service  ->  driver  ->  hal/include
   |          |           |
   +----------+-----------+------>  infra
  service/input/vss_input_net.{c,h}, app  ->  net_if
  hal/s32k344  ->  vendor driver headers (the only place they may appear)

hal/include headers include nothing but <stdint.h> and <stdbool.h>.

Besides #include edges it checks:
  * configuration-tool generated symbols appear only in hal_s32k344_cfg.h,
  * vendor driver API calls appear only under hal/s32k344,
  * the interrupt callbacks that the generated configuration binds by name are
    defined only under hal/s32k344,
  * no extern data declarations anywhere (modules share state only through
    functions; extern is allowed on function declarations and extern "C").

Unmodified third-party files (THIRD_PARTY) skip the standard and vendor
header rules: their extra includes sit behind feature switches the build
turns off. Instead they must not call an allocator.

Usage: check_layers.py [--src DIR] [--selftest]
Exit status 0 = no violation.
"""

import argparse
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SRC = os.path.normpath(os.path.join(HERE, '..', 'src'))

LAYERS = ('app', 'service', 'driver', 'hal_include', 'hal_s32k344', 'infra', 'net_if')

# layers a file of the key layer may include (project-internal headers)
ALLOWED = {
    'app':         {'app', 'service', 'driver', 'hal_include', 'infra', 'net_if'},
    'service':     {'service', 'driver', 'infra'},
    'driver':      {'driver', 'hal_include', 'infra'},
    'hal_include': {'hal_include'},
    'hal_s32k344': {'hal_s32k344', 'hal_include', 'infra'},
    'infra':       {'infra'},
    'net_if':      {'net_if'},
}

# service files that may reach the Network API headers (app may as well)
NET_IF_USERS = {'service/input/vss_input_net.c', 'service/input/vss_input_net.h'}

STD_HEADERS = {
    'assert.h', 'ctype.h', 'errno.h', 'float.h', 'inttypes.h', 'limits.h',
    'math.h', 'stdarg.h', 'stdbool.h', 'stddef.h', 'stdint.h', 'stdio.h',
    'string.h',
}
FORBIDDEN_STD = {'stdlib.h', 'malloc.h'}  # no dynamic allocation
HAL_INCLUDE_STD = {'stdint.h', 'stdbool.h'}

# minimp3.h: <stdlib.h>, and SIMD intrinsics headers in branches that
# MINIMP3_NO_SIMD turns off
THIRD_PARTY = {'service/audio_stream/decoder/minimp3.h'}
ALLOC_CALL = re.compile(r'\b(?:malloc|calloc|realloc|free)\s*\(')

GENERATED_SYMBOLS = re.compile(
    r'\b(?:Clock_Ip_aClockConfig|g_pin_mux_InitConfigArr\w*|NUM_OF_CONFIGURED_PINS\w*'
    r'|intCtrlConfig|I2c_Lpi2cMaster\w*|PIT_0_(?:InitConfig|ChannelConfig|CH)\w*'
    r'|Rm_Config\w*|Dma_Ip_Sa_\w+|SA_DMA_LOGIC\w*|Sai_Ip_Sa_\w+|SAICONFIGURATION\w*'
    r'|CS2100_REF_(?:PIN|PORT)|PT[A-K]_[HL]_HALF)\b')
VENDOR_API = re.compile(
    r'\b(?:Clock_Ip|Siul2_Port_Ip|Siul2_Dio_Ip|IntCtrl_Ip|Lpi2c_Ip|Pit_Ip|Dma_Ip|Dma_Mux_Ip|Sai_Ip|Rm)_\w+'
    r'|\bRm_Init\b')
CALLBACK_NAMES = re.compile(
    r'\b(?:AudioPlayer_DmaTxIrqCallback|AudioPlayer_SaiTxCallback'
    r'|AudioPlayer_Sai0IrqHandler|Cs2100Ref_Callback)\b')

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)
EXTERN_RE = re.compile(r'\bextern\b(?!\s*"C")([^;{]*)[;{]')
COMMENT_RE = re.compile(r'/\*.*?\*/|//[^\n]*', re.S)


def strip_comments(text):
    """Blank out comments but keep line numbering."""
    return COMMENT_RE.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text)


def is_function_declaration(decl):
    """True for 'T name(args)', false for objects such as 'T x', 'T a[4]' or
    the function pointer 'T (*p)(args)'."""
    paren = decl.find('(')
    if paren < 0:
        return False
    return not decl[paren + 1:].lstrip().startswith('*')


def layer_of(rel):
    parts = rel.replace('\\', '/').split('/')
    if len(parts) < 2:
        return None
    top = parts[0]
    if top == 'hal':
        if len(parts) >= 3 and parts[1] == 'include':
            return 'hal_include'
        if len(parts) >= 3 and parts[1] == 's32k344':
            return 'hal_s32k344'
        return None
    if top in ('app', 'service', 'driver', 'infra', 'net_if'):
        return top
    return None


def collect(src):
    files = []
    for d, _, fs in os.walk(src):
        for f in fs:
            if f.endswith(('.c', '.h')):
                files.append(os.path.relpath(os.path.join(d, f), src).replace('\\', '/'))
    return sorted(files)


def check(src):
    violations = []
    edges = 0
    files = collect(src)
    counts = dict((l, 0) for l in LAYERS)

    def bad(rel, line, msg):
        violations.append('%s:%d: %s' % (rel, line, msg))

    for rel in files:
        layer = layer_of(rel)
        if layer is None:
            bad(rel, 1, 'file is not inside a known layer directory')
            continue
        counts[layer] += 1
        text = open(os.path.join(src, rel), encoding='utf-8', errors='replace').read()
        clean = strip_comments(text)

        for m in INCLUDE_RE.finditer(clean):
            line = clean.count('\n', 0, m.start()) + 1
            kind, name = m.group(1), m.group(2)
            base = os.path.basename(name)
            if kind == '<':
                if rel in THIRD_PARTY:
                    continue
                if name in FORBIDDEN_STD:
                    bad(rel, line, '<%s> is not allowed (no dynamic allocation)' % name)
                elif name in STD_HEADERS:
                    if layer == 'hal_include' and name not in HAL_INCLUDE_STD:
                        bad(rel, line, 'hal/include may only use stdint.h and stdbool.h, not <%s>' % name)
                elif layer != 'hal_s32k344':
                    bad(rel, line, 'vendor header <%s> outside hal/s32k344' % name)
                continue
            target = os.path.normpath(os.path.join(src, os.path.dirname(rel), name))
            inside = os.path.commonpath([target, src]) == os.path.normpath(src)
            if inside and os.path.isfile(target):
                trel = os.path.relpath(target, src).replace('\\', '/')
                tlayer = layer_of(trel)
                edges += 1
                if tlayer is None:
                    bad(rel, line, 'include "%s" resolves outside the known layers' % name)
                    continue
                allowed = ALLOWED[layer]
                if rel in NET_IF_USERS:
                    allowed = allowed | {'net_if'}
                if tlayer not in allowed:
                    bad(rel, line, '%s -> %s is not allowed ("%s")' % (layer, tlayer, name))
                    continue
                if layer == 'app' and tlayer == 'hal_include' and base != 'hal_board.h':
                    bad(rel, line, 'app may include only hal_board.h from hal/include, not "%s"' % name)
                if layer == 'service' and tlayer == 'driver' and not (
                        trel.startswith('driver/') and
                        (rel.startswith('service/audio_stream/') or rel.startswith('service/health/'))):
                    bad(rel, line, 'only service/audio_stream and service/health may include driver headers')
            else:
                # not found next to the including file: a vendor header or a
                # path that does not follow the file-relative rule
                if layer != 'hal_s32k344':
                    bad(rel, line, 'include "%s" does not resolve relative to the file '
                                   '(vendor headers belong in hal/s32k344)' % name)

        for m in EXTERN_RE.finditer(clean):
            if not is_function_declaration(m.group(1)):
                line = clean.count('\n', 0, m.start()) + 1
                bad(rel, line, 'extern data declaration "%s" (share state through functions)'
                    % ' '.join(m.group(0).split()))

        if rel in THIRD_PARTY:
            for m in ALLOC_CALL.finditer(clean):
                bad(rel, clean.count('\n', 0, m.start()) + 1,
                    'allocation call "%s" in third-party source' % m.group(0).rstrip('( '))

        is_cfg = rel == 'hal/s32k344/hal_s32k344_cfg.h'
        for lineno, ln in enumerate(clean.split('\n'), 1):
            if GENERATED_SYMBOLS.search(ln) and not is_cfg:
                bad(rel, lineno, 'generated configuration symbol outside hal_s32k344_cfg.h: %s'
                    % GENERATED_SYMBOLS.search(ln).group(0))
            if layer != 'hal_s32k344':
                v = VENDOR_API.search(ln)
                if v:
                    bad(rel, lineno, 'vendor driver symbol outside hal/s32k344: %s' % v.group(0))
                c = CALLBACK_NAMES.search(ln)
                if c:
                    bad(rel, lineno, 'interrupt callback "%s" belongs to hal/s32k344' % c.group(0))
    return files, counts, edges, violations


def run(src):
    files, counts, edges, violations = check(src)
    print('check_layers: %d files, %d project-internal includes' % (len(files), edges))
    print('  ' + ', '.join('%s=%d' % (l, counts[l]) for l in LAYERS))
    for v in violations:
        print('VIOLATION ' + v)
    print('check_layers: %s (%d violation%s)' % ('FAIL' if violations else 'PASS',
                                                 len(violations), '' if len(violations) == 1 else 's'))
    return 1 if violations else 0


def selftest():
    """Builds throw-away trees and asserts that each rule fires (and only then)."""
    cases = [
        # (name, {path: content}, expected substring or None)
        ('clean', {
            'app/a.c': '#include "../service/s.h"\n#include "../hal/include/hal_board.h"\n#include "../net_if/n.h"\n',
            'service/s.h': '', 'service/audio_stream/t.h': '#include "../../driver/x/d.h"\n',
            'driver/x/d.h': '#include "../../hal/include/h.h"\n', 'net_if/n.h': '',
            'hal/include/hal_board.h': '#include <stdint.h>\n',
            'hal/include/h.h': '#include <stdint.h>\n#include <stdbool.h>\n'}, None),
        ('driver->service', {'driver/d.c': '#include "../service/s.h"\n', 'service/s.h': ''}, 'driver -> service'),
        ('hal->driver', {'hal/s32k344/x.c': '#include "../../driver/d.h"\n', 'driver/d.h': ''}, 'hal_s32k344 -> driver'),
        ('service->app', {'service/s.c': '#include "../app/a.h"\n', 'app/a.h': ''}, 'service -> app'),
        ('service->hal', {'service/s.c': '#include "../hal/include/h.h"\n', 'hal/include/h.h': ''}, 'service -> hal_include'),
        ('app->hal other', {'app/a.c': '#include "../hal/include/hal_i2c.h"\n', 'hal/include/hal_i2c.h': ''}, 'only hal_board.h'),
        ('service/input->driver', {'service/input/i.c': '#include "../../driver/d.h"\n', 'driver/d.h': ''}, 'only service/audio_stream'),
        ('net_if from store', {'service/store/s.c': '#include "../../net_if/n.h"\n', 'net_if/n.h': ''}, 'service -> net_if'),
        ('net_if->service', {'net_if/n.h': '#include "../service/s.h"\n', 'service/s.h': ''}, 'net_if -> service'),
        ('net_if from input_net', {'service/input/vss_input_net.c': '#include "../../net_if/n.h"\n', 'net_if/n.h': ''}, None),
        ('hal/include std', {'hal/include/h.h': '#include <string.h>\n'}, 'only use stdint.h'),
        ('hal/include->infra', {'hal/include/h.h': '#include "../../infra/vss_config.h"\n',
                                'infra/vss_config.h': ''}, 'hal_include -> infra'),
        ('extern data', {'driver/d.h': 'extern int g_shared;\n'}, 'extern data'),
        ('extern array', {'service/s.c': 'extern const unsigned char g_tab[4];\n'}, 'extern data'),
        ('extern fn pointer', {'driver/d.h': 'extern void (*g_hook)(void);\n'}, 'extern data'),
        ('extern multi-line', {'app/a.c': 'extern unsigned long\n    g_counter;\n'}, 'extern data'),
        ('extern in hal', {'hal/s32k344/x.c': 'extern volatile int g_flag;\n'}, 'extern data'),
        ('extern function ok', {'driver/d.h': 'extern void Driver_Run(void);\n'
                                              'extern int Driver_Get(int a, int (*cb)(int));\n'}, None),
        ('extern in comment ok', {'driver/d.c': '/* extern int g_x; */\n// extern int g_y;\n'}, None),
        ('extern "C" ok', {'driver/d.h': '#ifdef __cplusplus\nextern "C" {\n#endif\n'}, None),
        ('third-party includes ok', {'service/audio_stream/decoder/minimp3.h':
                                     '#include <stdlib.h>\n#include <immintrin.h>\n'}, None),
        ('third-party alloc', {'service/audio_stream/decoder/minimp3.h': 'void *p = malloc(4);\n'},
         'allocation call'),
        ('stdlib next to third-party', {'service/audio_stream/decoder/port.c': '#include <stdlib.h>\n'},
         'no dynamic allocation'),
        ('rtd in service', {'service/s.c': '#include "Sai_Ip.h"\n'}, 'does not resolve relative'),
        ('rtd angle in driver', {'driver/d.c': '#include <Sai_Ip.h>\n'}, 'vendor header'),
        ('rtd in hal ok', {'hal/s32k344/x.c': '#include "Sai_Ip.h"\nvoid f(void){ Sai_Ip_Send(0,0,0); }\n'}, None),
        ('vendor call in driver', {'driver/d.c': 'void f(void){ Sai_Ip_AbortTransfer(0); }\n'}, 'vendor driver symbol'),
        ('vendor call in comment', {'driver/d.c': '/* Sai_Ip_AbortTransfer */\n// Dma_Ip_Init\n'}, None),
        ('generated symbol', {'hal/s32k344/x.c': 'int a = SA_DMA_LOGIC_CH_0;\n'}, 'generated configuration symbol'),
        ('generated in cfg ok', {'hal/s32k344/hal_s32k344_cfg.h': '#define X SA_DMA_LOGIC_CH_0\n'}, None),
        ('callback in driver', {'driver/d.c': 'void AudioPlayer_DmaTxIrqCallback(void){}\n'}, 'interrupt callback'),
        ('callback in hal ok', {'hal/s32k344/x.c': 'void AudioPlayer_DmaTxIrqCallback(void){}\n'}, None),
        ('stdlib', {'service/s.c': '#include <stdlib.h>\n'}, 'no dynamic allocation'),
        ('unknown dir', {'misc/m.c': ''}, 'not inside a known layer'),
        ('escape src', {'app/a.c': '#include "../../outside.h"\n'}, 'does not resolve relative'),
    ]
    failed = 0
    for name, tree, expect in cases:
        with tempfile.TemporaryDirectory() as tmp:
            for p, c in tree.items():
                full = os.path.join(tmp, p)
                os.makedirs(os.path.dirname(full), exist_ok=True)
                with open(full, 'w', encoding='utf-8') as fh:
                    fh.write(c)
            _, _, _, v = check(tmp)
        ok = (not v) if expect is None else any(expect in x for x in v)
        print('  %-24s %s' % (name, 'ok' if ok else 'WRONG: %r' % v))
        failed += 0 if ok else 1
    print('check_layers selftest: %s (%d cases, %d wrong)' % ('FAIL' if failed else 'PASS', len(cases), failed))
    return 1 if failed else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--src', default=DEFAULT_SRC, help='source root (default: firmware/vss/src)')
    ap.add_argument('--selftest', action='store_true', help='verify the checker itself on synthetic trees')
    args = ap.parse_args()
    if args.selftest:
        return selftest()
    return run(os.path.normpath(os.path.abspath(args.src)))


if __name__ == '__main__':
    sys.exit(main())
