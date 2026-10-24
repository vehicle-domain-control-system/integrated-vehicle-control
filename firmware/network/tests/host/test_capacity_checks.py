"""Link conditions of the node buffer capacities (include/network/net_capacity.h).

ctest tools.capacity_checks:
  python -B test_capacity_checks.py --cc <C compiler>

Compiles single files of this tree for the CIS, BCM and VSS node profiles
into a new temporary folder (nothing is written here), with the warning flags
of the node build:
  poll   NET_UPPER_POLL_BUDGET <= NET_UPPER_RX_CAPACITY
         -DNET_UPPER_POLL_BUDGET=<RX + 1> does not compile (net_upper_service.c);
         the default and -DNET_UPPER_POLL_BUDGET=<RX> compile.
  tx     NET_UPPER_TX_CAPACITY >= entries of the node publish table
         CIS and BCM with -DNET_UPPER_TX_CAPACITY=<entries - 1> do not compile
         (src/node/net_node_profile_<node>.c); the default and
         -DNET_UPPER_TX_CAPACITY=<entries> compile, the VSS default too.
  publish NET_UPPER_PUBLISH_CAPACITY >= entries of the node publish table
         (one publisher slot per entry): CIS and BCM with
         -DNET_UPPER_PUBLISH_CAPACITY=<entries - 1> do not compile, the default
         and -DNET_UPPER_PUBLISH_CAPACITY=<entries> compile.
  drain  NET_BOARD_ISR_DRAIN_BUDGET <= NET_HAL_RX_CAPACITY
         -DNET_BOARD_ISR_DRAIN_BUDGET=<HAL RX + 1> does not compile
         (src/platform/rtd/net_hal_rtd.c on sdk_mock/rtd); the default and
         -DNET_BOARD_ISR_DRAIN_BUDGET=<HAL RX> compile.
A refused build must be refused by the check of that condition (its typedef
name in the compiler output), not by some other error.

The publish entries are the messages the node sends, from the message lists
of docs/network/interface/03_COMMUNICATION/10_CAN_FD.md and 30_UART_C.md:
CIS 0x0A0, 0x300, 0x310, 0x320 (CAN) and P_PERMISSION 0x62 (UART-C), BCM
0x140, 0x141, 0x200, 0x210, 0x220, 0x230, VSS 0x340. The publish table of each
profile must list exactly these. The capacity values themselves are read from
the profile with a probe program (their table is checked elsewhere).
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
# node profile -> MCU of its node build (NETWORK_NODE_PLATFORM)
NODES = {'CIS': 144, 'BCM': 144, 'VSS': 344}
PUBLISH = {
    'CIS': {('CAN', 0x0A0), ('CAN', 0x300), ('CAN', 0x310), ('CAN', 0x320), ('UART_C', 0x62)},
    'BCM': {('CAN', 0x140), ('CAN', 0x141), ('CAN', 0x200), ('CAN', 0x210), ('CAN', 0x220), ('CAN', 0x230)},
    'VSS': {('CAN', 0x340)},
}
SERVICE = 'src/l03_message_service/m04_message_service/net_upper_service.c'
RTD_COMMON = 'src/platform/rtd/net_hal_rtd.c'
CHECK_NAME = {
    'poll': 'net_capacity_NET_UPPER_POLL_BUDGET_le_NET_UPPER_RX_CAPACITY',
    'tx': 'net_node_publish_entries_le_NET_UPPER_TX_CAPACITY',
    'publish': 'net_node_publish_entries_le_NET_UPPER_PUBLISH_CAPACITY',
    'drain': 'net_board_drain_budget_in_range',
}
PROBE = r'''#include <stdio.h>
#include "network/net_upper.h"
#include "network/net_upper_publish.h"
#include "network/net_hal_adapter.h"
#include "net_board_cfg.h"
#define P(name) printf("%s=%lu\n", #name, (unsigned long)(name))
int main(void)
{
    P(NET_UPPER_TX_CAPACITY);
    P(NET_UPPER_PUBLISH_CAPACITY);
    P(NET_UPPER_RX_CAPACITY);
    P(NET_UPPER_POLL_BUDGET);
    P(NET_HAL_RX_CAPACITY);
    P(NET_BOARD_ISR_DRAIN_BUDGET);
    return 0;
}
'''


class Checks:
    def __init__(self, cc):
        self.cc = cc
        self.failures = 0
        self.tmp = tempfile.TemporaryDirectory(prefix='capacity_checks_')
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

    def defines(self, node, extra=()):
        return ['-DNETWORK_UPPER_NODE_PROFILE=%d' % self.ids[node], '-DNETWORK_NODE_PLATFORM=%d' % NODES[node]] + [
            '-D' + d for d in extra]

    def compile(self, node, rel, extra=(), mock=False):
        self.count += 1
        obj = self.root / ('unit_%d.o' % self.count)
        incs = ['-I' + str(NETWORK / i) for i in INCLUDES] + (['-I' + str(NETWORK / MOCK_INCLUDE)] if mock else [])
        return self.run([self.cc] + FLAGS + self.defines(node, extra) + incs + ['-c', NETWORK / rel, '-o', obj])

    def values(self, node):
        src = self.root / 'values.c'
        src.write_text(PROBE, encoding='utf-8')
        exe = self.root / ('values_%s%s' % (node, '.exe' if os.name == 'nt' else ''))
        incs = ['-I' + str(NETWORK / i) for i in INCLUDES]
        rc, out = self.run([self.cc] + FLAGS + self.defines(node) + incs + [src, '-o', exe])
        if not self.check(rc == 0, '%s: value probe does not compile' % node, out):
            return None
        rc, out = self.run([exe])
        if not self.check(rc == 0, '%s: value probe does not run' % node, out):
            return None
        return {m.group(1): int(m.group(2)) for m in re.finditer(r'^(NET_[A-Z_]+)=(\d+)\s*$', out, re.M)}

    def refused(self, cid, node, rel, define, mock=False):
        rc, out = self.compile(node, rel, (define,), mock)
        self.check(rc != 0 and CHECK_NAME[cid] in out,
                   '%s %s: -D%s must stop the build at the %s check (%s); exit %d'
                   % (cid, node, define, cid, CHECK_NAME[cid], rc), out)
        if rc != 0 and CHECK_NAME[cid] in out:
            print('ok   %s %s: -D%s does not compile' % (cid, node, define))

    def accepted(self, cid, node, rel, define=None, mock=False):
        rc, out = self.compile(node, rel, (define,) if define else (), mock)
        what = '-D' + define if define else 'the default'
        if self.check(rc == 0, '%s %s: %s does not compile' % (cid, node, what), out):
            print('ok   %s %s: %s compiles' % (cid, node, what))


def profile_ids():
    text = (NETWORK / 'include' / 'network' / 'net_upper_profile.h').read_text(encoding='utf-8')
    return {m.group(1): int(m.group(2)) for m in re.finditer(r'^#define NET_UPPER_PROFILE_(\w+) (\d+)\s*$', text, re.M)}


def publish_table(node):
    """{(link, id)} of the publish table of the node profile file."""
    text = (NETWORK / 'src' / 'node' / ('net_node_profile_%s.c' % node.lower())).read_text(encoding='utf-8')
    m = re.search(r'publish_table\[\]\s*=\s*\{(.*?)\n\};', text, re.S)
    if not m:
        return None
    return {(link, int(ident, 16)) for link, ident in
            re.findall(r'\{\s*NET_WIRE_(CAN|UART_C|UART_M)\s*,\s*0x([0-9A-Fa-f]+)u', m.group(1))}


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    p.add_argument('--cc', default='', help='C compiler of the build (GCC or Clang)')
    args = p.parse_args()
    if not args.cc or os.path.basename(args.cc).lower().startswith('cl'):
        print('FAIL tools.capacity_checks: needs a GCC-compatible C compiler (--cc %r)' % args.cc)
        return 1
    c = Checks(args.cc)
    try:
        if not c.check(all(n in c.ids for n in NODES), 'net_upper_profile.h: profile ids %s' % c.ids):
            raise RuntimeError('node profile ids missing')
        for node in NODES:
            table = publish_table(node)
            c.check(table == PUBLISH[node], '%s: publish table %s, documented sends %s' % (
                node, sorted(table or ()), sorted(PUBLISH[node])))
            v = c.values(node)
            if v is None:
                continue
            print('%s: %s' % (node, ' '.join('%s=%d' % (k[4:], x) for k, x in sorted(v.items()))))
            rx, hal_rx, entries = v['NET_UPPER_RX_CAPACITY'], v['NET_HAL_RX_CAPACITY'], len(PUBLISH[node])
            c.accepted('poll', node, SERVICE)
            c.accepted('poll', node, SERVICE, 'NET_UPPER_POLL_BUDGET=%du' % rx)
            c.refused('poll', node, SERVICE, 'NET_UPPER_POLL_BUDGET=%du' % (rx + 1))
            profile = 'src/node/net_node_profile_%s.c' % node.lower()
            c.accepted('tx', node, profile)
            if node in ('CIS', 'BCM'):
                c.accepted('tx', node, profile, 'NET_UPPER_TX_CAPACITY=%du' % entries)
                c.refused('tx', node, profile, 'NET_UPPER_TX_CAPACITY=%du' % (entries - 1))
                c.accepted('publish', node, profile, 'NET_UPPER_PUBLISH_CAPACITY=%du' % entries)
                c.refused('publish', node, profile, 'NET_UPPER_PUBLISH_CAPACITY=%du' % (entries - 1))
            c.accepted('drain', node, RTD_COMMON, mock=True)
            c.accepted('drain', node, RTD_COMMON, 'NET_BOARD_ISR_DRAIN_BUDGET=%du' % hal_rx, mock=True)
            c.refused('drain', node, RTD_COMMON, 'NET_BOARD_ISR_DRAIN_BUDGET=%du' % (hal_rx + 1), mock=True)
    except Exception as e:  # report, never leave the temporary folder behind
        c.check(False, 'exception: %r' % (e,))
    finally:
        c.close()
    if c.failures:
        print('FAIL tools.capacity_checks (%d)' % c.failures)
        return 1
    print('PASS tools.capacity_checks')
    return 0


if __name__ == '__main__':
    sys.exit(main())
