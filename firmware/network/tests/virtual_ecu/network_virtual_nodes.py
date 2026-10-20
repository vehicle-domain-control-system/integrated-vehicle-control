"""HAL-less node workers connected by a byte-only virtual CAN bus / UART-C line.

Each node worker (tests/virtual_ecu/virtual_node.c) runs its own build's node
profile table and App Port facade; the peer worker (virtual_peer.c, HOST
profile) plays DOMAIN on CAN and the Pi on UART-C. This runner only moves the
CAN envelopes and UART bytes the workers put on the wire (UART bytes in
uneven fragments) and checks what the receiving side delivered. No codec,
routing or owner decision is implemented here.

Every check prints one line: CHECK <name> PASS|FAIL <facts as JSON>.
"""
from __future__ import annotations
import argparse, collections, json, os, pathlib, queue, subprocess, sys, threading

CAN_NODES = ('DOMAIN', 'VSS', 'CIS', 'BCM')
ALL_NODES = CAN_NODES + ('PI',)
UART_C_PEER = {'CIS': 'PI', 'PI': 'CIS'}
PROFILES = {'VSS': 6, 'CIS': 1, 'BCM': 3, 'DOMAIN': 0, 'PI': 0}
BOOT = {'DOMAIN': 301, 'VSS': 601, 'CIS': 401, 'BCM': 201, 'PI': 6401}
EPOCH = 3


class Worker:
    def __init__(self, name, executable, log):
        self.name, self.log, self.now = name, log, 1000
        command = [str(executable)] + ([name] if name in ('DOMAIN', 'PI') else [])
        self.p = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True, bufsize=1)
        self.lines = queue.Queue()

        def reader():
            for line in self.p.stdout:
                self.lines.put(line)
            self.lines.put(None)
        threading.Thread(target=reader, daemon=True).start()
        ready = self.read()
        if ready.get('event') != 'ready' or ready.get('profile') != PROFILES[name]:
            raise RuntimeError(f'{name}: unexpected start {ready}')
        self.ready = ready

    def read(self):
        try:
            line = self.lines.get(timeout=15)
        except queue.Empty:
            raise RuntimeError(f'{self.name}: worker did not answer')
        if line is None:
            raise RuntimeError(f'{self.name} exited {self.p.poll()}: {self.p.stderr.read()}')
        item = json.loads(line)
        self.log(item)
        return item

    def command(self, text):
        self.log({'event': 'driver_command', 'node': self.name, 'command': text})
        self.p.stdin.write(text + '\n')
        self.p.stdin.flush()
        out = []
        while True:
            item = self.read()
            out.append(item)
            if item['event'] == 'done':
                return out

    def close(self):
        if self.p.poll() is None:
            self.p.stdin.write('quit\n')
            self.p.stdin.flush()
            self.p.wait(timeout=10)
        if self.p.returncode:
            raise RuntimeError(f'{self.name} exited {self.p.returncode}')


def find_executable(root, role):
    name = 'network_virtual_node_' + role + ('.exe' if os.name == 'nt' else '')
    found = sorted(root.rglob(name))
    if len(found) != 1:
        raise FileNotFoundError(f'{name}: expected one build below {root}, found {len(found)}')
    return found[0]


class World:
    def __init__(self, root, emit):
        self.trace, self.pending, self.at = [], collections.deque(), 1000

        def log(item):
            self.trace.append(item)
            emit(item)
        self.nodes = {}
        try:
            for name in ALL_NODES:
                role = 'PEER' if name in ('DOMAIN', 'PI') else name
                self.nodes[name] = Worker(name, find_executable(root, role), log)
        except BaseException:
            self.close()
            raise

    def command(self, name, text, drain=True):
        items = self.nodes[name].command(text)
        self.pending.extend(i for i in items if i['event'] == 'wire')
        if text.startswith('tick ') and items[-1]['status'] != 'HOLD':
            self.nodes[name].now = int(text.split()[1])
        if drain:
            self.drain()
        return items

    def sync(self, name, at):
        if self.nodes[name].now < at:
            self.command(name, f'tick {at}', False)

    def drain(self):
        while self.pending:
            f = self.pending.popleft()
            sender, link, at = f['node'], f['link'], f['at']
            if link == 0:
                for name in CAN_NODES:
                    if name == sender:
                        continue
                    self.sync(name, at)
                    self.command(name, f"canrx {f['id']} {f['dlc']} {f['flags']} {EPOCH} {at} {f['hex']}", False)
            elif link == 2:
                peer = UART_C_PEER[sender]
                self.sync(peer, at)
                raw, pos, part, sizes = bytes.fromhex(f['hex']), 0, 0, (1, 3, 7, 2, 8)
                while pos < len(raw):
                    piece = raw[pos:pos + sizes[part % len(sizes)]]
                    pos += len(piece)
                    part += 1
                    self.command(peer, f'uartrx 2 {EPOCH} {at} {piece.hex()}', False)
            else:
                raise RuntimeError(f'unexpected link in {f}')

    def run(self, n=12):
        for _ in range(n):
            self.at += 1
            for name in ALL_NODES:
                self.command(name, f'tick {self.at}', False)
            self.drain()

    def events(self, event, node=None, **fields):
        return [i for i in self.trace if i['event'] == event and (node is None or i.get('node') == node)
                and all(i.get(k) == v for k, v in fields.items())]

    def observe(self, node):
        return next(i for i in self.command(node, 'observe') if i['event'] == 'observation')

    def close(self):
        errors = []
        for w in self.nodes.values():
            try:
                w.close()
            except Exception as e:  # report every worker
                errors.append(str(e))
        if errors:
            raise RuntimeError('; '.join(errors))


def need(cond, what):
    if not cond:
        raise AssertionError(what)


def one(items, what):
    need(len(items) == 1, f'{what}: expected 1, got {len(items)}')
    return items[0]


def vss_event_to_status(w):
    """DOMAIN VSS_EVENT -> VssApp_OnDomainEvent -> VssNet_PublishStatus -> DOMAIN VSS_STATUS."""
    w.command('DOMAIN', 'vss_event 77 1')
    w.run()
    rx = one(w.events('app_rx', 'VSS', call='VssApp_OnDomainEvent', occurrence=77), 'VSS event delivery')
    need(rx['kind'] == 1 and rx['boot'] == BOOT['DOMAIN'] and rx['use_limit'] == 500, rx)
    one(w.events('app_submit', 'VSS', call='VssNet_PublishStatus', result='OK'), 'VSS status submit')
    back = one(w.events('peer_rx', 'DOMAIN', id=0x340, update=77), 'VSS_STATUS at DOMAIN')
    need(back['source_boot'] == BOOT['VSS'] and back['source_tx'] == 1 and back['accepting'] == 1, back)
    return {'event_occurrence': 77, 'status_update': back['update'], 'status_boot': back['source_boot'],
            'status_tx': back['source_tx']}


def cis_permission_relay(w):
    """DOMAIN DOMAIN_PERMISSION -> CisApp_OnDomainPermission + P_PERMISSION relay bytes -> Pi."""
    w.command('DOMAIN', 'permission 1 5')
    w.run()
    rx = one(w.events('app_rx', 'CIS', call='CisApp_OnDomainPermission', update=5), 'CIS permission delivery')
    need(rx['power'] == 1 and rx['boot'] == BOOT['DOMAIN'], rx)
    # UPDATE_SEQUENCE is the CIS relay revision (04_COMMON/10_DATA_AND_ID_RULES.md
    # number spaces): 1 for the first relay of this CIS run, not the DOMAIN 5.
    relay = w.events('peer_rx', 'PI', type=0x62, update=1)
    need(relay, 'P_PERMISSION at the Pi')
    need(relay[0]['cis_boot'] == BOOT['CIS'] and relay[0]['power'] == 1 and relay[0]['link_sequence'] == 1, relay[0])
    need(w.events('wire', 'CIS', link=2, type=0x62), 'P_PERMISSION bytes on UART-C')
    return {'permission_update': 5, 'relay_cis_boot': relay[0]['cis_boot'], 'relay_use_limit': relay[0]['use_limit'],
            'relay_link_sequence': relay[0]['link_sequence']}


def cis_vision_to_occupant(w):
    """Pi P_VISION_RESULT bytes -> CisApp_OnPiVisionResult -> CisNet_PublishOccupant -> DOMAIN CIS_OCCUPANT."""
    w.command('PI', 'vision 42 2')
    w.run()
    rx = one(w.events('app_rx', 'CIS', call='CisApp_OnPiVisionResult', vision=42), 'CIS vision delivery')
    need(rx['count'] == 2 and rx['pi_boot'] == BOOT['PI'] and rx['link_sequence'] == 1, rx)
    one(w.events('app_submit', 'CIS', call='CisNet_PublishOccupant', result='OK'), 'CIS occupant submit')
    back = one(w.events('peer_rx', 'DOMAIN', id=0x310, vision=42), 'CIS_OCCUPANT at DOMAIN')
    need(back['count'] == 2 and back['pi_boot'] == BOOT['PI'] and back['source_boot'] == BOOT['CIS'], back)
    return {'vision_sequence': 42, 'occupant_count': back['count'], 'occupant_boot': back['source_boot']}


def cis_rear_publish(w):
    """CisNet_PublishRear -> DOMAIN CIS_REAR."""
    w.command('CIS', 'cis_rear 9 1234')
    one(w.events('app_submit', 'CIS', call='CisNet_PublishRear', result='OK'), 'CIS rear submit')
    w.run()
    back = one(w.events('peer_rx', 'DOMAIN', id=0x0a0, update=9), 'CIS_REAR at DOMAIN')
    need(back['distance'] == 1234 and back['source_boot'] == BOOT['CIS'], back)
    return {'rear_update': 9, 'distance_raw': back['distance']}


def bcm_door_command_to_result(w):
    """DOMAIN BCM_DOOR_COMMAND -> BcmApp_OnDomainDoorCommand -> BcmNet_SendResult -> DOMAIN BCM_RESULT."""
    w.command('DOMAIN', f"door_command 901 {BOOT['BCM']} 0")
    w.run()
    rx = one(w.events('app_rx', 'BCM', call='BcmApp_OnDomainDoorCommand', command=901), 'BCM command delivery')
    need(rx['target_boot'] == BOOT['BCM'] and rx['boot'] == BOOT['DOMAIN'] and rx['target'] == 0, rx)
    one(w.events('app_submit', 'BCM', call='BcmNet_SendResult', result='OK'), 'BCM result submit')
    back = one(w.events('peer_rx', 'DOMAIN', id=0x140, command=901), 'BCM_RESULT at DOMAIN')
    need(back['command_boot'] == BOOT['DOMAIN'] and back['result'] == 3 and back['source_boot'] == BOOT['BCM'], back)
    return {'command_id': 901, 'result_command_boot': back['command_boot'], 'result': back['result']}


def bcm_door_state_publish(w):
    """BcmNet_PublishDoorState -> DOMAIN BCM_DOOR_STATE."""
    w.command('BCM', 'bcm_door_state 3')
    one(w.events('app_submit', 'BCM', call='BcmNet_PublishDoorState', result='OK'), 'BCM door state submit')
    w.run()
    back = one(w.events('peer_rx', 'DOMAIN', id=0x200, update=3), 'BCM_DOOR_STATE at DOMAIN')
    need(back['source_boot'] == BOOT['BCM'], back)
    return {'door_state_update': 3}


CHECKS = (vss_event_to_status, cis_permission_relay, cis_vision_to_occupant, cis_rear_publish,
          bcm_door_command_to_result, bcm_door_state_publish)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin-dir', required=True, type=pathlib.Path)
    p.add_argument('--trace', type=pathlib.Path)
    args = p.parse_args()
    out = args.trace.open('w', encoding='utf8') if args.trace else None

    def emit(item):
        if out:
            out.write(json.dumps(item, separators=(',', ':')) + '\n')
    failed = 0
    try:
        world = World(args.bin_dir, emit)
        try:
            for check in CHECKS:
                try:
                    facts = check(world)
                    print(f'CHECK {check.__name__} PASS {json.dumps(facts)}', flush=True)
                except Exception as e:  # one line per failed check, keep going
                    failed += 1
                    print(f'CHECK {check.__name__} FAIL {json.dumps(str(e))}', flush=True)
        finally:
            world.close()
    finally:
        if out:
            out.close()
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
