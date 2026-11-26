"""HAL-less node workers connected by a byte-only virtual CAN bus / UART-C line.

Each node worker (tests/virtual_ecu/virtual_node.c) runs its own build's node
profile table and App Port facade; the peer worker (virtual_peer.c, HOST
profile) plays DOMAIN on CAN and the Pi on UART-C. This runner only moves the
CAN envelopes and UART bytes the workers put on the wire (UART bytes in
uneven fragments) and checks what the receiving side delivered. No codec,
routing or owner decision is implemented here.

With --domain-node the DOMAIN side of CAN is the DOMAIN node worker
(virtual_domain_node.c: the DOMAIN node root, its C2 adapter and the actual
DOMAIN runtime) instead of the HOST peer, and the DOMAIN checks run. The
DOMAIN application does not call its TxPort yet; the driver asks the worker
to call DomainIf_Tx*. UART-M frames of the DOMAIN node have no ESP32 worker
here: they stay in the trace.

Every check prints one line: CHECK <name> PASS|FAIL <facts as JSON>.
"""
from __future__ import annotations
import argparse, collections, json, os, pathlib, queue, subprocess, sys, threading

CAN_NODES = ('DOMAIN', 'VSS', 'CIS', 'BCM')
ALL_NODES = CAN_NODES + ('PI',)
UART_C_PEER = {'CIS': 'PI', 'PI': 'CIS'}
PROFILES = {'VSS': 6, 'CIS': 1, 'BCM': 3, 'DOMAIN': 0, 'PI': 0}
DOMAIN_NODE_PROFILE = 5
BOOT = {'DOMAIN': 301, 'VSS': 601, 'CIS': 401, 'BCM': 201, 'PI': 6401}
EPOCH = 3


class Worker:
    def __init__(self, name, executable, log, profile=None, extra=()):
        self.name, self.log, self.now = name, log, 1000
        command = [str(executable)] + ([name] if name in ('DOMAIN', 'PI') else []) + [str(a) for a in extra]
        self.p = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True, bufsize=1)
        self.lines = queue.Queue()

        def reader():
            for line in self.p.stdout:
                self.lines.put(line)
            self.lines.put(None)
        threading.Thread(target=reader, daemon=True).start()
        ready = self.read()
        if ready.get('event') != 'ready' or ready.get('profile') != (PROFILES[name] if profile is None else profile):
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
    def __init__(self, root, emit, domain_node=False):
        self.trace, self.pending, self.at = [], collections.deque(), 1000
        self.domain_node = domain_node

        def log(item):
            self.trace.append(item)
            emit(item)
        self.log, self.exe, self.nodes = log, {}, {}
        try:
            for name in ALL_NODES:
                role = 'PEER' if name == 'PI' or (name == 'DOMAIN' and not domain_node) else name
                self.exe[name] = find_executable(root, role)
                self.nodes[name] = Worker(name, self.exe[name], log, self.profile(name))
        except BaseException:
            self.close()
            raise

    def profile(self, name):
        return DOMAIN_NODE_PROFILE if name == 'DOMAIN' and self.domain_node else PROFILES[name]

    def restart(self, name, boot):
        """A new run of the node: the worker process ends and starts again with
        another BOOT_ID (all its state, sequences included, starts anew)."""
        self.nodes[name].close()
        self.log({'event': 'driver_restart', 'node': name, 'boot': boot})
        self.nodes[name] = Worker(name, self.exe[name], self.log, self.profile(name), (boot,))
        need(self.nodes[name].ready.get('boot', boot) == boot, self.nodes[name].ready)
        self.sync(name, self.at)

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
            elif link == 1 and self.domain_node and sender == 'DOMAIN':
                continue  # UART-M to the ESP32: no worker in this run, kept in the trace
            else:
                raise RuntimeError(f'unexpected link in {f}')

    def run(self, n=12):
        for _ in range(n):
            self.at += 1
            for name in ALL_NODES:
                self.command(name, f'tick {self.at}', False)
            self.drain()

    def run_for(self, ms, step=2):
        """Every worker ticks to the same time in steps of step ms, ms in all."""
        end = self.at + ms
        while self.at < end:
            self.at = min(self.at + step, end)
            for name in ALL_NODES:
                self.command(name, f'tick {self.at}', False)
            self.drain()

    def events(self, event, node=None, since=0, **fields):
        return [i for i in self.trace[since:] if i['event'] == event and (node is None or i.get('node') == node)
                and all(i.get(k) == v for k, v in fields.items())]

    def query(self, what):
        return next(i for i in self.command('DOMAIN', 'query ' + what) if i['event'] == 'c2_query')

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

# ------------------------------------------------------------- DOMAIN node ---
# C2 and wire values the checks compare (03_COMMUNICATION/10_CAN_FD.md).
DOMAIN_IF_OK, DATA_QUALITY_OK, NO_DATA = 0, 0, 3
NODE_READY, BARRIER_OPEN = 8, 2
CAN_PERIOD = 50


def domain_node_ready(w):
    """DOMAIN node READY (NetNode_Init with the C2 adapter as Owner), TxPort registered."""
    r = w.nodes['DOMAIN'].ready
    need(r['profile'] == DOMAIN_NODE_PROFILE and r['boot'] == BOOT['DOMAIN'] and r['stage'] == NODE_READY, r)
    need(r['registered'] and r['barriers'] and r['transition_bound'] and r['occupant_enabled'], r)
    return {k: r[k] for k in ('profile', 'boot', 'stage', 'registered', 'barriers', 'transition_bound',
                              'occupant_enabled')}


def domain_cis_rear_rx(w):
    """CisNet_PublishRear -> CIS_REAR 0x0A0 -> DomainIf_RxCisRear stores it -> DomainIf_QueryGetCisRear."""
    mark = len(w.trace)
    w.command('CIS', 'cis_rear 9 1234')
    one(w.events('app_submit', 'CIS', mark, call='CisNet_PublishRear', result='OK'), 'CIS rear submit')
    w.run_for(10)
    rx = one(w.events('c2_rx', 'DOMAIN', mark, call='RxCisRear'), 'RxCisRear')
    need(rx['status'] == DOMAIN_IF_OK and rx['mm'] == 1234 and rx['source'] == BOOT['CIS'], rx)
    need(rx['distance']['sequence'] == 9 and rx['distance']['quality'] == DATA_QUALITY_OK, rx)
    q = w.query('rear')
    need(q['status'] == DOMAIN_IF_OK and q['mm'] == rx['mm'] and q['source'] == rx['source'], q)
    need(q['distance']['sequence'] == 9 and q['distance']['quality'] == DATA_QUALITY_OK, q)
    return {'rx_mm': rx['mm'], 'query_mm': q['mm'], 'source': q['source'], 'sequence': q['distance']['sequence']}


def permissions(w, since):
    return w.events('app_rx', 'CIS', since, call='CisApp_OnDomainPermission')


def accepted(items, what):
    return one([i for i in items if i['event'] == 'c2_tx' and i['status'] == DOMAIN_IF_OK], what)


def withdraw_once(w, bundle, evaluation):
    """ALLOW (OK) of a new DOMAIN evaluation, then its withdrawal false + NO_DATA
    under the same evaluation; the test bundle gives the power value bundle
    (0 or 255) for the non-OK quality."""
    w.command('DOMAIN', f'bundle_unknown {bundle}')
    mark = len(w.trace)
    accepted(w.command('DOMAIN', f'permission 1 ok {evaluation} 0'), 'ALLOW accepted by the TxPort')
    allowed_at = w.at
    w.run_for(60)
    allow = [i for i in permissions(w, mark) if i['power'] == 1]
    need(allow and allow[0]['quality'] == DATA_QUALITY_OK and allow[0]['boot'] == BOOT['DOMAIN'], allow[:1])
    mark = len(w.trace)
    tx = accepted(w.command('DOMAIN', f'permission 0 nodata {evaluation} {w.at - allowed_at}'),
                  'withdrawal accepted by the TxPort')
    w.run_for(2 * CAN_PERIOD)
    after = permissions(w, mark)
    unknown = [i for i in after if i['power'] == 255]
    need(unknown, f'no UNKNOWN at CIS within two CAN periods: {after}')
    first = unknown[0]
    need(first['quality'] == NO_DATA and first['received_at'] - tx['at'] <= 2 * CAN_PERIOD, first)
    need(first['update'] != allow[0]['update'], (allow[0], first))
    need(not [i for i in after if i['power'] != 255 and i['received_at'] >= first['received_at']], after)
    return {'bundle': bundle, 'allow_update': allow[0]['update'], 'withdraw_update': first['update'],
            'power': first['power'], 'quality': first['quality'], 'delay_ms': first['received_at'] - tx['at']}


def domain_permission_withdraw_immediate(w):
    """ALLOW, then DOMAIN withdraws it (false + NO_DATA): CIS gets not-allowed
    (255 + NO_DATA) within two CAN periods, whether the bundle gave 0 or 255."""
    return {'bundle_0': withdraw_once(w, 0, 1), 'bundle_255': withdraw_once(w, 255, 2)}


def domain_permission_repeat_kept(w):
    """M04 repeats DOMAIN_PERMISSION every 50 ms: CIS gets the same
    UPDATE_SEQUENCE again with a new TX_SEQUENCE and a growing AGE."""
    mark = len(w.trace)
    accepted(w.command('DOMAIN', 'permission 1 ok 3 0'), 'ALLOW accepted by the TxPort')
    w.run_for(3 * CAN_PERIOD + 20)
    rx = [i for i in permissions(w, mark) if i['power'] == 1]
    need(len(rx) >= 3, f'{len(rx)} deliveries')
    need(len({i['update'] for i in rx}) == 1, rx)
    need(all(b['age'] > a['age'] and b['tx'] > a['tx'] for a, b in zip(rx, rx[1:])), rx)
    gaps = [b['received_at'] - a['received_at'] for a, b in zip(rx, rx[1:])]
    need(all(abs(g - CAN_PERIOD) <= 2 for g in gaps), gaps)
    return {'update': rx[0]['update'], 'deliveries': len(rx), 'ages': [i['age'] for i in rx], 'gaps_ms': gaps}


def domain_bcm_command_result(w):
    """A BCM frame gives DOMAIN the BCM BOOT; DomainIf_TxBcmDoorCommand -> 0x100
    with TARGET_BOOT_ID = that BOOT -> BcmApp_OnDomainDoorCommand ->
    BcmNet_SendResult -> BCM_RESULT 0x140 -> DomainIf_RxBcmCommandResult."""
    mark = len(w.trace)
    w.command('BCM', 'bcm_door_state 4')
    w.run_for(10)
    one(w.events('c2_rx', 'DOMAIN', mark, call='RxBcmDoorState'), 'RxBcmDoorState')
    items = w.command('DOMAIN', 'door_command 902')
    target = one([i for i in items if i['event'] == 'c2_target'], 'command target')
    need(target['bcm_target_boot'] == BOOT['BCM'], target)
    accepted(items, 'command accepted by the TxPort')
    w.run_for(20)
    rx = one(w.events('app_rx', 'BCM', mark, call='BcmApp_OnDomainDoorCommand', command=902), 'BCM command')
    need(rx['target_boot'] == BOOT['BCM'] and rx['boot'] == BOOT['DOMAIN'], rx)
    one(w.events('app_submit', 'BCM', mark, call='BcmNet_SendResult', result='OK'), 'BCM result submit')
    res = one(w.events('c2_rx', 'DOMAIN', mark, call='RxBcmCommandResult', command=902), 'RxBcmCommandResult')
    return {'target_boot': rx['target_boot'], 'command': res['command'], 'result': res['result'],
            'reason': res['reason'], 'domain_status': res['status']}


def domain_vss_warning_repeat(w):
    """DomainIf_TxVssWarningState once -> M04 repeats 0x0C0 every 50 ms -> VSS
    gets each repetition (same lanes, new TX_SEQUENCE)."""
    mark = len(w.trace)
    accepted(w.command('DOMAIN', 'warning 70 1'), 'warning accepted by the TxPort')
    w.run_for(3 * CAN_PERIOD + 20)
    rx = w.events('app_rx', 'VSS', mark, call='VssApp_OnDomainWarningState', rear_sequence=70)
    need(len(rx) >= 3, f'{len(rx)} deliveries')
    need(len({(i['rear_state'], i['boot']) for i in rx}) == 1 and rx[0]['boot'] == BOOT['DOMAIN'], rx)
    need(all(b['tx'] > a['tx'] for a, b in zip(rx, rx[1:])), rx)
    gaps = [b['received_at'] - a['received_at'] for a, b in zip(rx, rx[1:])]
    need(all(abs(g - CAN_PERIOD) <= 2 for g in gaps), gaps)
    return {'deliveries': len(rx), 'tx': [i['tx'] for i in rx], 'gaps_ms': gaps}


def barrier(w, kind):
    return w.observe('DOMAIN')[kind + '_barrier']


def domain_env_transition(w):
    """CIS_ENVIRONMENT through its barrier: the first frame of a CIS run is held,
    the adapter step applies the DOMAIN private transition (epoch 1), the next
    frame is stored. CIS restarts: epoch 2 invalidates the old values first,
    then the environment of the new run is delivered again."""
    mark = len(w.trace)
    for sequence in (11, 14):
        w.command('CIS', f'cis_environment {sequence} 2150')
        w.run_for(10)
    rx = one(w.events('c2_rx', 'DOMAIN', mark, call='RxCisCabinEnvironment'), 'environment of the first run')
    need(rx['status'] == DOMAIN_IF_OK and rx['temperature'] == 2150 and rx['temperature_q']['sequence'] == 14, rx)
    q1 = w.query('environment')
    need(q1['temperature'] == 2150 and q1['applied'] and q1['admitted'] == BOOT['CIS'] and q1['epoch'] == 1, q1)
    w.restart('CIS', BOOT['CIS'] + 1)
    mark = len(w.trace)
    w.command('CIS', 'cis_environment 1 1980')
    w.run_for(10)
    b = barrier(w, 'environment')
    need(b['state'] == BARRIER_OPEN and b['id'] == BOOT['CIS'] + 1 and b['next_epoch'] == 3 and b['applied'] == 2, b)
    held = w.query('environment')
    need(held['temperature_q']['quality'] != DATA_QUALITY_OK and held['epoch'] == 2, held)
    need(not w.events('c2_rx', 'DOMAIN', mark, call='RxCisCabinEnvironment'), 'first frame of the new run delivered')
    w.command('CIS', 'cis_environment 4 1980')
    w.run_for(10)
    rx = one(w.events('c2_rx', 'DOMAIN', mark, call='RxCisCabinEnvironment'), 'environment of the new run')
    need(rx['status'] == DOMAIN_IF_OK and rx['temperature'] == 1980, rx)
    q2 = w.query('environment')
    need(q2['temperature'] == 1980 and q2['temperature_q']['sequence'] == 4 and q2['epoch'] == 2, q2)
    need(q2['admitted'] == BOOT['CIS'] + 1 and q2['revision'] == 2, q2)
    return {'first': {'boot': q1['admitted'], 'epoch': q1['epoch'], 'temperature': q1['temperature']},
            'restart': {'boot': q2['admitted'], 'epoch': q2['epoch'], 'revision': q2['revision'],
                        'temperature': q2['temperature']}}


def domain_occupant_transition(w):
    """CIS_OCCUPANT through its barrier, keyed by the Pi generation: Pi 6401 gets
    epoch 1; the Pi restarts (6402, the same CIS run), epoch 2, and the Occupant
    of the new generation is delivered again."""
    pi = BOOT['PI']
    mark = len(w.trace)
    for vision in (21, 22):
        w.command('PI', f'vision {vision} 2')
        w.run_for(20)
    rx = one(w.events('c2_rx', 'DOMAIN', mark, call='RxCisOccupant'), 'Occupant of the first Pi run')
    need(rx['status'] == DOMAIN_IF_OK and rx['generation'] == pi and rx['count'] == 2, rx)
    q1 = w.query('occupant')
    need(q1['count'] == 2 and q1['generation'] == pi and q1['epoch'] == 1 and q1['admitted'] == pi, q1)
    w.restart('PI', pi + 1)
    mark = len(w.trace)
    w.command('PI', 'vision 1 1')
    w.run_for(20)
    b = barrier(w, 'occupant')
    need(b['state'] == BARRIER_OPEN and b['id'] == pi + 1 and b['next_epoch'] == 3, b)
    need(not w.events('c2_rx', 'DOMAIN', mark, call='RxCisOccupant'), 'first frame of the new Pi run delivered')
    w.command('PI', 'vision 2 1')
    w.run_for(20)
    rx = one(w.events('c2_rx', 'DOMAIN', mark, call='RxCisOccupant'), 'Occupant of the new Pi run')
    need(rx['status'] == DOMAIN_IF_OK and rx['generation'] == pi + 1 and rx['count'] == 1, rx)
    q2 = w.query('occupant')
    need(q2['count'] == 1 and q2['generation'] == pi + 1 and q2['epoch'] == 2 and q2['revision'] == 2, q2)
    return {'first': {'generation': q1['generation'], 'epoch': q1['epoch']},
            'restart': {'generation': q2['generation'], 'epoch': q2['epoch'], 'revision': q2['revision'],
                        'count': q2['count']}}


DOMAIN_CHECKS = (domain_node_ready, domain_cis_rear_rx, domain_permission_withdraw_immediate,
                 domain_permission_repeat_kept, domain_bcm_command_result, domain_vss_warning_repeat,
                 domain_env_transition, domain_occupant_transition)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin-dir', required=True, type=pathlib.Path)
    p.add_argument('--trace', type=pathlib.Path)
    p.add_argument('--domain-node', action='store_true',
                   help='DOMAIN side of CAN: the DOMAIN node worker, DOMAIN checks')
    args = p.parse_args()
    out = args.trace.open('w', encoding='utf8') if args.trace else None

    def emit(item):
        if out:
            out.write(json.dumps(item, separators=(',', ':')) + '\n')
    failed = 0
    try:
        world = World(args.bin_dir, emit, args.domain_node)
        try:
            for check in (DOMAIN_CHECKS if args.domain_node else CHECKS):
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
