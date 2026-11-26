"""FlexCAN message buffer layout of the board binding (src/platform/rtd/net_board_cfg.h).

ctest tools.board_mb_layout:
  python -B test_board_mb_layout.py --cc <C compiler>

Compiles single files of this tree into a new temporary folder (nothing is
written here), with the warning flags of the node build:
  defaults  the MB defaults follow NETWORK_NODE_PLATFORM:
              144 (or CPU_S32K144HFT0VLLT): max 7, RX MBs 0..1, TX from MB 2
              344 (or CPU_S32K344) and no MCU choice (HOST profile, no
              node composition root): max 16, RX MBs 0..3, TX from MB 8
            and a -D value still overrides a default.
  range     src/platform/rtd/net_hal_rtd_can.c (on sdk_mock/rtd) compiles with
            the default layout of each node build (CIS, BCM on 144, VSS on 344)
            and with the last TX first MB that fits; one MB further
            (CIS, BCM on 144: -DNET_BOARD_CAN_TX_MB_FIRST=4, TX MBs 4..7 of 7)
            does not compile.
  overlap   a TX range that overlaps the RX MBs does not compile.
A refused build must be refused by the layout check (its typedef name in the
compiler output), not by some other error. TX uses NET_HAL_TX_CAPACITY MBs
(node profile, include/network/net_capacity.h), read with a probe program.
"""
import argparse
import os
import pathlib
import re
import subprocess
import sys
import tempfile

NETWORK = pathlib.Path(__file__).resolve().parents[2]
INCLUDES = ('include', 'src/l04_frame_codec', 'src/l02_typed_handoff/m03_typed_handoff',
            'src/l05_transport_port/m07_buffer_outcome', 'src/l05_transport_port/m09_observation',
            'src/l03_message_service/m04_message_service', 'src/platform/rtd')
MOCK_INCLUDE = 'sdk_mock/rtd/include'
FLAGS = ['-std=c99', '-Wall', '-Wextra', '-Wpedantic', '-Werror']
CAN_UNIT = 'src/platform/rtd/net_hal_rtd_can.c'
# node profile -> MCU of its node build (NETWORK_NODE_PLATFORM)
NODES = {'CIS': 144, 'BCM': 144, 'VSS': 344}
LAYOUT = {
    144: {'NET_BOARD_CAN_MAX_MB': 7, 'NET_BOARD_CAN_RX_MB_FIRST': 0, 'NET_BOARD_CAN_RX_MB_COUNT': 2,
          'NET_BOARD_CAN_TX_MB_FIRST': 2},
    344: {'NET_BOARD_CAN_MAX_MB': 16, 'NET_BOARD_CAN_RX_MB_FIRST': 0, 'NET_BOARD_CAN_RX_MB_COUNT': 4,
          'NET_BOARD_CAN_TX_MB_FIRST': 8},
}
CHECK_RANGE = 'net_board_mb_in_range'
CHECK_OVERLAP = 'net_board_rx_tx_disjoint'
PROBE = r'''#include <stdio.h>
#include "network/net_hal_adapter.h"
#include "net_board_cfg.h"
#define P(name) printf("%s=%lu\n", #name, (unsigned long)(name))
int main(void)
{
    P(NET_BOARD_CAN_MAX_MB);
    P(NET_BOARD_CAN_RX_MB_FIRST);
    P(NET_BOARD_CAN_RX_MB_COUNT);
    P(NET_BOARD_CAN_TX_MB_FIRST);
    P(NET_HAL_TX_CAPACITY);
    return 0;
}
'''


class Checks:
    def __init__(self, cc):
        self.cc = cc
        self.failures = 0
        self.tmp = tempfile.TemporaryDirectory(prefix='board_mb_layout_')
        self.root = pathlib.Path(self.tmp.name)
        self.env = dict(os.environ)
        if os.path.dirname(cc):
            self.env['PATH'] = os.path.dirname(cc) + os.pathsep + self.env.get('PATH', '')
        self.count = 0
        self.ids = profile_ids()

    def close(self):
        self.tmp.cleanup()

    def check(self, cond, msg, detail=''):
        if not cond:
            self.failures += 1
            print('FAIL: %s' % msg)
            if detail:
                print('      ' + detail.strip().replace('\n', '\n      ')[-2500:])
        return cond

    def run(self, cmd):
        r = subprocess.run([str(x) for x in cmd], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                           encoding='utf-8', errors='replace', env=self.env, cwd=str(self.root), timeout=600)
        return r.returncode, r.stdout

    def defines(self, node, platform, extra=()):
        out = ['-DNETWORK_UPPER_NODE_PROFILE=%d' % self.ids[node]] if node else []
        if platform:
            out.append('-DNETWORK_NODE_PLATFORM=%d' % platform)
        return out + ['-D' + d for d in extra]

    def includes(self, mock):
        return ['-I' + str(NETWORK / i) for i in INCLUDES] + (['-I' + str(NETWORK / MOCK_INCLUDE)] if mock else [])

    def values(self, label, node, platform, extra=()):
        self.count += 1
        src = self.root / 'values.c'
        src.write_text(PROBE, encoding='utf-8')
        exe = self.root / ('values_%d%s' % (self.count, '.exe' if os.name == 'nt' else ''))
        rc, out = self.run([self.cc] + FLAGS + self.defines(node, platform, extra) + self.includes(False)
                           + [src, '-o', exe])
        if not self.check(rc == 0, '%s: value probe does not compile' % label, out):
            return None
        rc, out = self.run([exe])
        if not self.check(rc == 0, '%s: value probe does not run' % label, out):
            return None
        return {m.group(1): int(m.group(2)) for m in re.finditer(r'^(NET_[A-Z_]+)=(\d+)\s*$', out, re.M)}

    def defaults(self, label, node, platform, want, extra=()):
        v = self.values(label, node, platform, extra)
        if v is None:
            return None
        bad = ['%s=%s (want %d)' % (k, v.get(k), x) for k, x in sorted(want.items()) if v.get(k) != x]
        if self.check(not bad, '%s: %s' % (label, ', '.join(bad))):
            print('ok   %s: max %d, RX %d..%d, TX from %d' % (
                label, v['NET_BOARD_CAN_MAX_MB'], v['NET_BOARD_CAN_RX_MB_FIRST'],
                v['NET_BOARD_CAN_RX_MB_FIRST'] + v['NET_BOARD_CAN_RX_MB_COUNT'] - 1,
                v['NET_BOARD_CAN_TX_MB_FIRST']))
        return v

    def compile_can(self, node, extra=()):
        self.count += 1
        obj = self.root / ('can_%d.o' % self.count)
        return self.run([self.cc] + FLAGS + self.defines(node, NODES[node], extra) + self.includes(True)
                        + ['-c', NETWORK / CAN_UNIT, '-o', obj])

    def accepted(self, node, what, extra=()):
        rc, out = self.compile_can(node, extra)
        if self.check(rc == 0, '%s: %s does not compile' % (node, what), out):
            print('ok   %s: %s compiles' % (node, what))

    def refused(self, node, what, check_name, extra):
        rc, out = self.compile_can(node, extra)
        if self.check(rc != 0 and check_name in out,
                      '%s: %s must stop the build at the %s check; exit %d' % (node, what, check_name, rc), out):
            print('ok   %s: %s does not compile' % (node, what))


def profile_ids():
    text = (NETWORK / 'include' / 'network' / 'net_upper_profile.h').read_text(encoding='utf-8')
    return {m.group(1): int(m.group(2)) for m in re.finditer(r'^#define NET_UPPER_PROFILE_(\w+) (\d+)\s*$', text, re.M)}


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    p.add_argument('--cc', default='', help='C compiler of the build (GCC or Clang)')
    args = p.parse_args()
    if not args.cc or os.path.basename(args.cc).lower().startswith('cl'):
        print('FAIL tools.board_mb_layout: needs a GCC-compatible C compiler (--cc %r)' % args.cc)
        return 1
    c = Checks(args.cc)
    try:
        if not c.check(all(n in c.ids for n in list(NODES) + ['HOST']), 'net_upper_profile.h: profile ids %s' % c.ids):
            raise RuntimeError('node profile ids missing')
        # defaults per MCU choice
        c.defaults('NETWORK_NODE_PLATFORM=144', 'CIS', 144, LAYOUT[144])
        c.defaults('NETWORK_NODE_PLATFORM=344', 'CIS', 344, LAYOUT[344])
        c.defaults('CPU_S32K144HFT0VLLT', 'CIS', None, LAYOUT[144], ('CPU_S32K144HFT0VLLT',))
        c.defaults('CPU_S32K344', 'VSS', None, LAYOUT[344], ('CPU_S32K344',))
        c.defaults('no MCU choice (HOST profile)', 'HOST', None, LAYOUT[344])
        over = dict(LAYOUT[144], NET_BOARD_CAN_MAX_MB=8, NET_BOARD_CAN_TX_MB_FIRST=3)
        c.defaults('144 with -D overrides', 'BCM', 144, over,
                   ('NET_BOARD_CAN_MAX_MB=8U', 'NET_BOARD_CAN_TX_MB_FIRST=3U'))
        # range and overlap checks of net_hal_rtd_can.c per node build
        for node, platform in NODES.items():
            v = c.defaults('%s node build (%d)' % (node, platform), node, platform, LAYOUT[platform])
            if v is None:
                continue
            tx, max_mb = v['NET_HAL_TX_CAPACITY'], v['NET_BOARD_CAN_MAX_MB']
            rx_end = v['NET_BOARD_CAN_RX_MB_FIRST'] + v['NET_BOARD_CAN_RX_MB_COUNT']
            c.check(v['NET_BOARD_CAN_TX_MB_FIRST'] + tx <= max_mb,
                    '%s: default TX MBs %d..%d beyond %d MBs' % (node, v['NET_BOARD_CAN_TX_MB_FIRST'],
                                                                 v['NET_BOARD_CAN_TX_MB_FIRST'] + tx - 1, max_mb))
            c.accepted(node, 'the default layout (TX MBs %d..%d of %d)'
                       % (v['NET_BOARD_CAN_TX_MB_FIRST'], v['NET_BOARD_CAN_TX_MB_FIRST'] + tx - 1, max_mb))
            last = max_mb - tx
            c.accepted(node, '-DNET_BOARD_CAN_TX_MB_FIRST=%d (TX MBs %d..%d of %d)' % (last, last, max_mb - 1, max_mb),
                       ('NET_BOARD_CAN_TX_MB_FIRST=%du' % last,))
            c.refused(node, '-DNET_BOARD_CAN_TX_MB_FIRST=%d (TX MBs %d..%d of %d)'
                      % (last + 1, last + 1, max_mb, max_mb), CHECK_RANGE,
                      ('NET_BOARD_CAN_TX_MB_FIRST=%du' % (last + 1),))
            c.refused(node, '-DNET_BOARD_CAN_TX_MB_FIRST=%d (overlaps RX MBs below %d)' % (rx_end - 1, rx_end),
                      CHECK_OVERLAP, ('NET_BOARD_CAN_TX_MB_FIRST=%du' % (rx_end - 1),))
        # the case of the S32K144 named in the board binding: TX MBs 4..7 with 7 MBs
        for node in ('CIS', 'BCM'):
            c.refused(node, '-DNET_BOARD_CAN_TX_MB_FIRST=4 on 144', CHECK_RANGE, ('NET_BOARD_CAN_TX_MB_FIRST=4u',))
    except Exception as e:  # report, never leave the temporary folder behind
        c.check(False, 'exception: %r' % (e,))
    finally:
        c.close()
    if c.failures:
        print('FAIL tools.board_mb_layout (%d)' % c.failures)
        return 1
    print('PASS tools.board_mb_layout')
    return 0


if __name__ == '__main__':
    sys.exit(main())
