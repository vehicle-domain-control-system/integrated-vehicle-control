#!/usr/bin/env python3
"""pi_bench: host stand-in for the Pi on the UART-C link of a CIS node (board tests).

The laptop plays the Pi. Over a serial port (the CIS board's UART-C) it sends
P_VISION_RESULT (0x60) and P_VISION_STATUS (0x61), once or periodically, and
prints every P_PERMISSION (0x62) the CIS relays. It can also send damaged
frames on purpose. Frame layout, CRC and payload offsets follow
docs/network/interface/03_COMMUNICATION/30_UART_C.md and the shared UART
framing of 20_UART_M.md:

  B0..1 SYNC 0xa5 0x5a, B2 VERSION 1, B3 TYPE, B4..5 PAYLOAD_LENGTH (u16 LE),
  B6..7 LINK_SEQUENCE (u16 LE, per direction, modulo 65536), payload,
  CRC16-CCITT-FALSE (poly 0x1021, init 0xffff, no reflection, xorout 0) over
  VERSION .. payload end, stored little-endian. Frame = payload + 10 bytes.

The value checks mirror the repository codec (net_wire_uart_dto.c,
net_wire_uart_c_status.c): a record the codec refuses is not sent unless
--allow-invalid is given. tests/host/test_pi_bench.py compares the bytes with
the codec through tools/vss_bench, both ways.

Commands (frame and decode need only the standard library; ports, send and run
need pyserial: python -m pip install pyserial):

  pi_bench.py frame result|status|permission [--boot HEX] [--seq N] [--link N]
                    [--bad KIND] [--allow-invalid] [field=value ...]
  pi_bench.py decode HEX ...
  pi_bench.py ports
  pi_bench.py send --port PORT result|status [--state FILE] [--new-boot]
                    [--boot HEX] [--seq N] [--link N] [--bad KIND]
                    [--listen S] [--log FILE] [field=value ...]
  pi_bench.py run --port PORT --duration S [--state FILE] [--new-boot]
                    [--boot HEX] [--result-every MS] [--status-every MS]
                    [--result-mode new|repeat] [--result-seq N]
                    [--status-seq N] [--link N] [--inject KIND@S ...]
                    [--log FILE] [field=value ...]

field=value sets a payload field by its name (the names of vss_bench). A name
of the other record of a run is applied there, so one list can set both.
Damaged frames (--bad, --inject): crc (last byte changed), trunc (first 12
bytes only), len (PAYLOAD_LENGTH + 1, CRC recomputed), version (VERSION 2, CRC
recomputed), type (TYPE 0x6f, CRC recomputed), noise (8 bytes without a SYNC).
A damaged frame does not use a LINK_SEQUENCE number: the next good frame
carries the number the damaged one showed. run keeps 80 ms of silence after an
injected one (longer than the 50 ms assembly limit of the CIS).

Numbers across runs (--state FILE): the CIS keeps the last LINK_SEQUENCE and
judgement numbers it accepted from one Pi BOOT and drops older ones, so a
second run with the same BOOT must continue them. --state reads the BOOT, the
next LINK_SEQUENCE and the last VISION / STATUS numbers before the run and
writes them after it; --boot, --link, --result-seq, --status-seq, --seq
override it. --new-boot plays a Pi restart: the next BOOT (state BOOT + 1, or
--boot), LINK_SEQUENCE 1 and both numbers from 1.

send and run print each received frame (P_PERMISSION) as one line, and at the
end one line "SUMMARY {json}" with the counts. --log appends every sent and
received frame as JSON lines. Exit: 0 = done (what happened on the line is in
the summary), 1 = decode found a damaged or refused frame, 2 = usage, a port
that cannot be opened, or a value the codec refuses.
"""
from __future__ import annotations

import argparse
import json
import sys
import threading
import time

SYNC = (0xA5, 0x5A)
VERSION = 1
OVERHEAD = 10
TYPE_RESULT, TYPE_STATUS, TYPE_PERMISSION = 0x60, 0x61, 0x62
UPLINK, DOWNLINK = 'uplink', 'downlink'          # Pi -> CIS, CIS -> Pi
ASSEMBLY_LIMIT_MS = 50.0                          # the CIS node's UART-C assembly limit
DEFAULT_BOOT = 0x50493031
QUIET_AFTER_DAMAGE_S = 0.08                       # run --inject: silence after a damaged frame
BAD_KINDS = ('crc', 'trunc', 'len', 'version', 'type', 'noise')

# (name, width in bytes, settable): payload order of each record.
RESULT_FIELDS = (('pi_boot_id', 4, False), ('vision_sequence', 4, False), ('vision_age_ms', 2, True),
                 ('occupant_presence', 1, True), ('presence_validity', 1, True), ('presence_reason', 1, True),
                 ('occupant_count', 1, True), ('count_validity', 1, True), ('count_reason', 1, True),
                 ('vision_status', 1, True), ('vision_fault_code', 2, True))
STATUS_FIELDS = (('pi_boot_id', 4, False), ('status_sequence', 4, False), ('vision_status', 1, True),
                 ('active_fault', 2, True), ('last_fault', 2, True), ('status_age_ms', 2, True),
                 ('affected_function', 1, True))
PERMISSION_FIELDS = (('cis_boot_id', 4, False), ('update_sequence', 4, False),
                     ('vehicle_power_permission', 1, True), ('value_quality', 1, True),
                     ('quality_reason', 1, True), ('use_limit_ms', 2, True), ('source_age_ms', 2, True))
# The documented valid defaults of tools/vss_bench (30_UART_C.md).
RESULT_DEFAULTS = {'vision_age_ms': 20, 'occupant_presence': 1, 'presence_validity': 1, 'presence_reason': 0,
                   'occupant_count': 1, 'count_validity': 1, 'count_reason': 0, 'vision_status': 2,
                   'vision_fault_code': 0}
STATUS_DEFAULTS = {'vision_status': 1, 'active_fault': 0, 'last_fault': 0, 'status_age_ms': 20,
                   'affected_function': 0}
PERMISSION_DEFAULTS = {'vehicle_power_permission': 1, 'value_quality': 0, 'quality_reason': 0,
                       'use_limit_ms': 1500, 'source_age_ms': 20}


class Msg:
    def __init__(self, word, type_, name, direction, fields, defaults, identity):
        self.word, self.type, self.name, self.direction = word, type_, name, direction
        self.fields, self.defaults, self.identity = fields, defaults, identity
        self.payload_length = sum(w for _, w, _ in fields)


MSGS = (Msg('result', TYPE_RESULT, 'P_VISION_RESULT', UPLINK, RESULT_FIELDS, RESULT_DEFAULTS,
            ('pi_boot_id', 'vision_sequence')),
        Msg('status', TYPE_STATUS, 'P_VISION_STATUS', UPLINK, STATUS_FIELDS, STATUS_DEFAULTS,
            ('pi_boot_id', 'status_sequence')),
        Msg('permission', TYPE_PERMISSION, 'P_PERMISSION', DOWNLINK, PERMISSION_FIELDS, PERMISSION_DEFAULTS,
            ('cis_boot_id', 'update_sequence')))
BY_WORD = {m.word: m for m in MSGS}
BY_TYPE = {m.type: m for m in MSGS}


class UsageError(Exception):
    pass


# ------------------------------------------------------------------ bytes ---

def crc16(data):
    """CRC16-CCITT-FALSE: poly 0x1021, init 0xffff, no reflection, xorout 0."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def le(value, width):
    return bytes((value >> (8 * i)) & 0xFF for i in range(width))


def from_le(data):
    return sum(b << (8 * i) for i, b in enumerate(data))


def frame_bytes(type_, link, payload, version=VERSION, length=None):
    """A whole frame; length overrides PAYLOAD_LENGTH (damaged frames)."""
    head = bytes((version, type_)) + le(len(payload) if length is None else length, 2) + le(link & 0xFFFF, 2)
    body = head + bytes(payload)
    return bytes(SYNC) + body + le(crc16(body), 2)


def encode_payload(msg, values):
    out = b''
    for name, width, _ in msg.fields:
        value = values[name]
        if not 0 <= value < (1 << (8 * width)):
            raise UsageError('%s=%s does not fit %d byte(s)' % (name, value, width))
        out += le(value, width)
    return out


def decode_payload(msg, payload):
    values, pos = {}, 0
    for name, width, _ in msg.fields:
        values[name] = from_le(payload[pos:pos + width])
        pos += width
    return values


def check(msg, v):
    """The codec verdict on the values: OK, MALFORMED or CONTRACT_HOLD (the C checks, same order)."""
    def tristate(x):
        return x <= 1 or x == 0xFF

    def pair(value, validity, reason):
        if validity > 1 or reason > 9:
            return 'MALFORMED'
        if validity == 1 and (value == 0xFF or reason != 0):
            return 'CONTRACT_HOLD'
        return 'OK'
    if msg.type == TYPE_RESULT:
        if (v['pi_boot_id'] == 0 or not tristate(v['occupant_presence']) or
                (v['occupant_count'] > 5 and v['occupant_count'] != 0xFF) or
                v['vision_status'] > 4 or v['vision_fault_code'] > 4):
            return 'MALFORMED'
        for verdict in (pair(v['occupant_presence'], v['presence_validity'], v['presence_reason']),
                        pair(v['occupant_count'], v['count_validity'], v['count_reason'])):
            if verdict != 'OK':
                return verdict
        if (v['presence_validity'] == 1 and v['count_validity'] == 1 and
                ((v['occupant_presence'] == 0 and v['occupant_count'] != 0) or
                 (v['occupant_presence'] == 1 and v['occupant_count'] == 0))):
            return 'CONTRACT_HOLD'
        if ((v['presence_validity'] == 1 or v['count_validity'] == 1) and
                (v['vision_fault_code'] != 0 or v['vision_status'] in (0, 3, 4))):
            return 'CONTRACT_HOLD'
        return 'OK'
    if msg.type == TYPE_STATUS:
        if (v['pi_boot_id'] == 0 or v['vision_status'] > 4 or v['active_fault'] > 4 or
                v['last_fault'] > 4 or v['affected_function'] & 0xFC):
            return 'MALFORMED'
        return 'OK'
    if (v['cis_boot_id'] == 0 or not tristate(v['vehicle_power_permission']) or
            v['value_quality'] > 3 or v['quality_reason'] > 9):
        return 'MALFORMED'
    if v['value_quality'] == 0 and (v['vehicle_power_permission'] == 0xFF or v['quality_reason'] != 0):
        return 'CONTRACT_HOLD'
    return 'OK'


def damage(kind, msg, link, payload):
    """Bytes of a damaged frame of this record (see the module text)."""
    good = frame_bytes(msg.type, link, payload)
    if kind == 'crc':
        return good[:-1] + bytes((good[-1] ^ 0xFF,))
    if kind == 'trunc':
        return good[:12]
    if kind == 'len':
        return frame_bytes(msg.type, link, payload, length=len(payload) + 1)
    if kind == 'version':
        return frame_bytes(msg.type, link, payload, version=2)
    if kind == 'type':
        return frame_bytes(0x6F, link, payload)
    if kind == 'noise':
        return bytes((0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x5A))
    raise UsageError('unknown damage %r (one of %s)' % (kind, ', '.join(BAD_KINDS)))


def hex_text(data):
    return ' '.join('%02x' % b for b in data)


def parse_hex_words(words):
    text = ' '.join(words).replace(',', ' ')
    tokens = []
    for tok in text.split():
        for prefix in ('hex=', 'data='):
            if tok.lower().startswith(prefix):
                tok = tok[len(prefix):]
        if tok.endswith(':') or not tok:
            continue
        if tok.lower().startswith('0x'):
            tok = tok[2:]
        if len(tok) % 2:
            raise UsageError('odd number of hex digits in %r' % tok)
        try:
            tokens += [int(tok[i:i + 2], 16) for i in range(0, len(tok), 2)]
        except ValueError:
            raise UsageError('not hex: %r' % tok)
    return bytes(tokens)


# ------------------------------------------------------------ the stream ---

class Stream:
    """Receive side of one UART-C direction (None: both), like the node's
    stream: resync one byte at a time on a bad SYNC, header or CRC, drop a
    frame not complete within the assembly limit. Events: ('frame', msg, link,
    values, bytes, t), ('bad_header', bytes), ('bad_crc', bytes),
    ('timeout', bytes), ('noise', n)."""

    def __init__(self, direction=DOWNLINK, assembly_limit_ms=ASSEMBLY_LIMIT_MS):
        self.direction = direction
        self.limit = assembly_limit_ms / 1000.0
        self.buf, self.at = bytearray(), []
        self.counts = {'frames': 0, 'bad_header': 0, 'bad_crc': 0, 'timeout': 0, 'noise_bytes': 0}

    def _drop(self, n, kind):
        self.counts[kind] = self.counts.get(kind, 0) + (n if kind == 'noise_bytes' else 1)
        gone = bytes(self.buf[:n])
        del self.buf[:n]
        del self.at[:n]
        return gone

    def feed(self, data, t):
        """Bytes received at time t (seconds, monotonic). Returns the events."""
        events = []
        for b in data:
            self.buf.append(b)
            self.at.append(t)
        events += self.poll(t)
        return events

    def poll(self, t):
        events = []
        while self.buf:
            if self.buf[0] != SYNC[0] or (len(self.buf) >= 2 and self.buf[1] != SYNC[1]):
                self._drop(1, 'noise_bytes')
                if not events or events[-1][0] != 'noise':
                    events.append(['noise', 1])
                else:
                    events[-1][1] += 1
                continue
            wanted = None
            if len(self.buf) >= 6:
                msg = BY_TYPE.get(self.buf[3])
                length = from_le(self.buf[4:6])
                if (self.buf[2] != VERSION or msg is None or
                        (self.direction is not None and msg.direction != self.direction) or
                        length != msg.payload_length):
                    events.append(('bad_header', self._drop(1, 'bad_header')))
                    continue
                wanted = length + OVERHEAD
            complete = wanted is not None and len(self.buf) >= wanted
            last = self.at[wanted - 1] if complete else t
            if last - self.at[0] > self.limit:
                events.append(('timeout', self._drop(1, 'timeout')))
                continue
            if not complete:
                break
            raw = bytes(self.buf[:wanted])
            if from_le(raw[-2:]) != crc16(raw[2:-2]):
                events.append(('bad_crc', self._drop(1, 'bad_crc')))
                continue
            msg = BY_TYPE[raw[3]]
            received = self.at[wanted - 1]
            del self.buf[:wanted]
            del self.at[:wanted]
            self.counts['frames'] += 1
            events.append(('frame', msg, from_le(raw[6:8]), decode_payload(msg, raw[8:-2]), raw, received))
        return [tuple(e) for e in events]


# -------------------------------------------------------------- records ---

def parse_int(text, base=0):
    try:
        return int(text, base)
    except ValueError:
        raise UsageError('not a number: %r' % text)


def parse_boot(text):
    t = text[2:] if text.lower().startswith('0x') else text
    if not t or len(t) > 8:
        raise UsageError('--boot %r is not a 32-bit hex number' % text)
    return parse_int(t, 16)


def field_settings(words, msgs):
    """field=value words -> {word: {field: value}}; a name of none of msgs is an error."""
    out = {m.word: {} for m in msgs}
    for word in words:
        if '=' not in word or word.startswith('='):
            raise UsageError('expected field=value, got %r' % word)
        name, value = word.split('=', 1)
        hit = False
        for m in msgs:
            for fname, width, settable in m.fields:
                if fname == name:
                    if not settable:
                        raise UsageError('%s is set by --boot / --seq, not field=value' % name)
                    v = parse_int(value)
                    if not 0 <= v < (1 << (8 * width)):
                        raise UsageError('%s=%s does not fit %d byte(s)' % (name, value, width))
                    out[m.word][name] = v
                    hit = True
        if not hit:
            names = sorted({f for m in msgs for f, _, s in m.fields if s})
            raise UsageError('unknown field %r; settable: %s' % (name, ' '.join(names)))
    return out


def record(msg, boot, seq, settings):
    if not 0 < boot <= 0xFFFFFFFF or not 0 <= seq <= 0xFFFFFFFF:
        raise UsageError('BOOT must be 1..0xffffffff and SEQ 0..0xffffffff')
    values = dict(msg.defaults)
    values.update(settings)
    values[msg.identity[0]] = boot
    values[msg.identity[1]] = seq
    return values


def fields_text(msg, values):
    parts = []
    for name, width, _ in msg.fields:
        if name.endswith('boot_id'):
            parts.append('%s=0x%08X' % (name, values[name]))
        else:
            parts.append('%s=%d' % (name, values[name]))
    return ' '.join(parts)


# -------------------------------------------------------- offline commands ---

def cmd_frame(args, out):
    msg = BY_WORD[args.message]
    settings = field_settings(args.fields, [msg])[msg.word]
    boot = parse_boot(args.boot) if args.boot else (DEFAULT_BOOT if msg.type != TYPE_PERMISSION else 0x43495331)
    values = record(msg, boot, parse_int(args.seq), settings)
    verdict = check(msg, values)
    if verdict != 'OK' and not args.allow_invalid:
        out.write('error: the codec refuses these %s values (%s); nothing built (--allow-invalid builds it)\n'
                  % (msg.name, verdict))
        return 2
    payload = encode_payload(msg, values)
    link = parse_int(args.link) & 0xFFFF
    data = damage(args.bad, msg, link, payload) if args.bad else frame_bytes(msg.type, link, payload)
    out.write('# %s type=0x%02x payload=%d B frame=%d B link=%d codec=%s%s\n' % (
        msg.name, msg.type, msg.payload_length, msg.payload_length + OVERHEAD, link, verdict,
        (' damaged=%s' % args.bad) if args.bad else ''))
    out.write('# fields: %s\n' % fields_text(msg, values))
    out.write('# payload: %s\n' % hex_text(payload))
    out.write('%s\n' % hex_text(data))
    return 0


def describe(event):
    kind = event[0]
    if kind == 'frame':
        _, msg, link, values, raw, _ = event
        return 'frame %s link=%d crc=OK codec=%s %s' % (msg.name, link, check(msg, values), fields_text(msg, values))
    if kind == 'noise':
        return 'noise %d byte(s)' % event[1]
    return '%s at %s' % (kind, hex_text(event[1]))


def cmd_decode(args, out):
    data = parse_hex_words(args.hex)
    if not data:
        raise UsageError('no bytes given')
    stream = Stream(None, assembly_limit_ms=1e9)
    bad = 0
    for e in stream.feed(data, 0.0):
        out.write(describe(e) + '\n')
        bad += e[0] != 'frame' or check(e[1], e[3]) != 'OK'
    if stream.buf:
        out.write('incomplete %d byte(s) at the end: %s\n' % (len(stream.buf), hex_text(stream.buf)))
        bad += 1
    return 1 if bad else 0


# --------------------------------------------------------- serial session ---

def open_port(port, baud):
    try:
        import serial  # pyserial
    except ImportError:
        raise UsageError('pyserial is not installed: python -m pip install pyserial')
    try:
        return serial.Serial(port=port, baudrate=baud, bytesize=8, parity='N', stopbits=1,
                             timeout=0.02, write_timeout=1.0)
    except (serial.SerialException, ValueError) as e:
        raise UsageError('cannot open %s: %s' % (port, e))


def cmd_ports(args, out):
    try:
        from serial.tools import list_ports
    except ImportError:
        raise UsageError('pyserial is not installed: python -m pip install pyserial')
    for p in sorted(list_ports.comports(), key=lambda p: p.device):
        out.write('%s  %s\n' % (p.device, p.description))
    return 0


class Session:
    """One open port: a reader thread feeds the CIS -> Pi stream; the main
    thread sends. Prints received frames, keeps counts, writes the log."""

    def __init__(self, port, baud, log_path, out):
        self.out = out
        self.lock = threading.Lock()       # console lines
        self.log_lock = threading.Lock()   # log lines (reader thread and main thread)
        try:
            self.log = open(log_path, 'a', encoding='utf-8') if log_path else None
        except OSError as e:
            raise UsageError('--log %s: %s' % (log_path, e))
        try:
            self.ser = open_port(port, baud)
        except UsageError:
            if self.log:
                self.log.close()
            raise
        self.t0 = time.monotonic()
        self.stream = Stream(DOWNLINK)
        self.tx = {'P_VISION_RESULT': 0, 'P_VISION_STATUS': 0, 'damaged': {}}
        self.rx_frames = {}
        self.permissions = []          # (t, link, values, codec verdict)
        self.rx_events = {'bad_header': 0, 'bad_crc': 0, 'timeout': 0, 'noise_bytes': 0}
        self.serial_errors = 0
        self.stop = threading.Event()
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()
        self._log({'event': 'open', 'port': port, 'baud': baud})

    def now(self):
        return time.monotonic() - self.t0

    def _log(self, item):
        with self.log_lock:
            if self.log:
                item = dict(item, t=round(self.now(), 4))
                self.log.write(json.dumps(item, sort_keys=True) + '\n')
                self.log.flush()

    def say(self, line):
        with self.lock:
            self.out.write('%9.3f %s\n' % (self.now(), line))
            self.out.flush()

    def _read(self):
        while not self.stop.is_set():
            try:
                data = self.ser.read(self.ser.in_waiting or 1)
            except Exception as e:  # serial errors end the reader; the summary shows them
                self.serial_errors += 1
                self.say('serial read error: %s' % e)
                return
            t = self.now()
            events = self.stream.feed(data, t) if data else self.stream.poll(t)
            for e in events:
                self._event(e)

    def _event(self, e):
        kind = e[0]
        if kind == 'frame':
            _, msg, link, values, raw, t = e
            self.rx_frames[msg.name] = self.rx_frames.get(msg.name, 0) + 1
            verdict = check(msg, values)
            if msg.type == TYPE_PERMISSION:
                self.permissions.append((t, link, values, verdict))
            self.say('rx %s link=%d codec=%s %s' % (msg.name, link, verdict, fields_text(msg, values)))
            self._log({'event': 'rx', 'type': msg.name, 'link': link, 'codec': verdict, 'fields': values,
                       'hex': hex_text(raw)})
            return
        if kind == 'noise':
            self.rx_events['noise_bytes'] += e[1]
        else:
            self.rx_events[kind] += 1
        self.say('rx %s' % describe(e))
        self._log({'event': 'rx_' + kind, 'detail': describe(e)})

    def send(self, data, label, fields=None, link=None):
        try:
            self.ser.write(data)
            self.ser.flush()
        except Exception as e:
            self.serial_errors += 1
            self.say('serial write error: %s' % e)
            return False
        self._log({'event': 'tx', 'what': label, 'link': link, 'fields': fields, 'hex': hex_text(data)})
        return True

    def close(self):
        """Stop reading (the last bytes still count) and close the port; the log stays open for finish()."""
        time.sleep(0.1)
        self.stop.set()
        self.reader.join(timeout=1.0)
        try:
            self.ser.close()
        except Exception:
            pass
        for e in self.stream.poll(self.now() + 1.0):
            self._event(e)

    def finish(self, extra):
        """The summary of the session (also the last log line); closes the log."""
        perms = self.permissions
        ups = [v['update_sequence'] for _, _, v, _ in perms]
        links = [link for _, link, _, _ in perms]
        gaps = [round((b[0] - a[0]) * 1000.0, 1) for a, b in zip(perms, perms[1:])]
        s = {
            'duration_s': round(self.now(), 3),
            'tx': self.tx,
            'rx_frames': self.rx_frames,
            'rx_damaged': self.rx_events,
            'serial_errors': self.serial_errors,
            'permission': {
                'count': len(perms),
                'codec_not_ok': sum(1 for p in perms if p[3] != 'OK'),
                'cis_boot_ids': sorted({'0x%08X' % v['cis_boot_id'] for _, _, v, _ in perms}),
                'update_sequence_first': ups[0] if ups else None,
                'update_sequence_last': ups[-1] if ups else None,
                'update_sequence_steps_not_plus_one': sum(1 for a, b in zip(ups, ups[1:]) if b != a + 1),
                'link_steps_not_plus_one': sum(1 for a, b in zip(links, links[1:]) if b != (a + 1) & 0xFFFF),
                'use_limit_ms': sorted({v['use_limit_ms'] for _, _, v, _ in perms}),
                'interval_ms_min': min(gaps) if gaps else None,
                'interval_ms_max': max(gaps) if gaps else None,
                'intervals_over_1500ms': sum(1 for g in gaps if g > 1500.0),
                'last': perms[-1][2] if perms else None,
            },
        }
        s.update(extra)
        self._log({'event': 'summary', 'summary': s})
        with self.log_lock:
            if self.log:
                self.log.close()
                self.log = None
        return s


# ------------------------------------------------------- numbers and state ---

class Numbers:
    """BOOT, next LINK_SEQUENCE and the last VISION / STATUS numbers of the Pi
    this tool plays, read from and written to --state (see the module text)."""

    def __init__(self, args):
        self.path = args.state
        st = {}
        if self.path:
            try:
                with open(self.path, encoding='utf-8') as fh:
                    st = json.load(fh)
            except FileNotFoundError:
                st = {}
            except (OSError, ValueError) as e:
                raise UsageError('--state %s unreadable: %s' % (self.path, e))
        if args.new_boot:
            self.boot = parse_boot(args.boot) if args.boot else (int(st['boot']) + 1 if 'boot' in st else DEFAULT_BOOT)
            self.boot &= 0xFFFFFFFF
            if self.boot == 0:
                self.boot = 1
            self.link, self.result_seq, self.status_seq = 1, 0, 0
        else:
            self.boot = parse_boot(args.boot) if args.boot else int(st.get('boot', DEFAULT_BOOT))
            self.link = int(st.get('next_link', 1))
            self.result_seq = int(st.get('result_seq', 0))
            self.status_seq = int(st.get('status_seq', 0))
        if args.link is not None:
            self.link = parse_int(args.link)
        self.link &= 0xFFFF
        if getattr(args, 'result_seq', None) is not None:
            self.result_seq = parse_int(args.result_seq)
        if getattr(args, 'status_seq', None) is not None:
            self.status_seq = parse_int(args.status_seq)

    def save(self):
        if not self.path:
            return
        with open(self.path, 'w', encoding='utf-8') as fh:
            json.dump({'boot': self.boot, 'next_link': self.link, 'result_seq': self.result_seq,
                       'status_seq': self.status_seq}, fh, sort_keys=True)
            fh.write('\n')

    def as_dict(self):
        return {'boot': '0x%08X' % self.boot, 'next_link': self.link, 'result_seq_last': self.result_seq,
                'status_seq_last': self.status_seq}


def built(msg, boot, seq, settings, allow_invalid):
    values = record(msg, boot, seq, settings)
    verdict = check(msg, values)
    if verdict != 'OK' and not allow_invalid:
        raise UsageError('the codec refuses these %s values (%s); --allow-invalid sends them' % (msg.name, verdict))
    return values, encode_payload(msg, values)


def cmd_send(args, out):
    msg = BY_WORD[args.message]
    settings = field_settings(args.fields, [msg])[msg.word]
    n = Numbers(args)
    last = n.result_seq if msg.type == TYPE_RESULT else n.status_seq
    seq = parse_int(args.seq) if args.seq is not None else last + 1
    values, payload = built(msg, n.boot, seq, settings, args.allow_invalid)
    link = n.link
    data = damage(args.bad, msg, link, payload) if args.bad else frame_bytes(msg.type, link, payload)
    s = Session(args.port, args.baud, args.log, out)
    try:
        label = ('damaged_' + args.bad) if args.bad else msg.name
        if s.send(data, label, values, link):
            if args.bad:
                s.tx['damaged'][args.bad] = 1
                s.say('tx %s (LINK_SEQUENCE %d not used)' % (label, link))
            else:
                s.tx[msg.name] += 1
                n.link = (link + 1) & 0xFFFF
                if msg.type == TYPE_RESULT:
                    n.result_seq = max(n.result_seq, seq)
                else:
                    n.status_seq = max(n.status_seq, seq)
                s.say('tx %s link=%d %s' % (label, link, fields_text(msg, values)))
        time.sleep(max(0.0, float(args.listen)))
    finally:
        s.close()
        n.save()
    out.write('SUMMARY %s\n' % json.dumps(s.finish(n.as_dict()), sort_keys=True))
    return 0


def parse_injections(items):
    out = []
    for item in items or ():
        if '@' not in item:
            raise UsageError('--inject wants KIND@SECONDS, got %r' % item)
        kind, at = item.split('@', 1)
        if kind not in BAD_KINDS:
            raise UsageError('unknown damage %r (one of %s)' % (kind, ', '.join(BAD_KINDS)))
        try:
            out.append((float(at), kind))
        except ValueError:
            raise UsageError('--inject time %r is not a number' % at)
    return sorted(out)


def cmd_run(args, out):
    result_msg, status_msg = BY_WORD['result'], BY_WORD['status']
    settings = field_settings(args.fields, [result_msg, status_msg])
    result_every = parse_int(args.result_every) / 1000.0
    status_every = parse_int(args.status_every) / 1000.0
    try:
        duration = float(args.duration)
    except ValueError:
        raise UsageError('--duration %r is not a number' % args.duration)
    if duration <= 0 or result_every < 0 or status_every < 0:
        raise UsageError('--duration must be > 0 and the periods >= 0')
    n = Numbers(args)
    base_age = settings['result'].get('vision_age_ms', RESULT_DEFAULTS['vision_age_ms'])
    # The records once before the port opens: a refused value is a usage error, nothing is sent.
    built(result_msg, n.boot, n.result_seq + 1, settings['result'], args.allow_invalid)
    built(status_msg, n.boot, n.status_seq + 1, settings['status'], args.allow_invalid)
    injections = parse_injections(args.inject)
    s = Session(args.port, args.baud, args.log, out)
    s.say('run: boot=0x%08X link from %d, result every %s ms (%s) from %d, status every %s ms from %d, %s s' % (
        n.boot, n.link, args.result_every, args.result_mode, n.result_seq + 1, args.status_every,
        n.status_seq + 1, args.duration))
    next_result = 0.0 if result_every > 0 else None
    next_status = 0.0 if status_every > 0 else None
    judged_at = None   # time of the current judgement (repeat mode)
    try:
        while True:
            t = s.now()
            if t >= duration:
                break
            if injections and t >= injections[0][0]:
                _, kind = injections.pop(0)
                values, payload = built(result_msg, n.boot, max(n.result_seq, 1), settings['result'], True)
                if s.send(damage(kind, result_msg, n.link, payload), 'damaged_' + kind, values, n.link):
                    s.tx['damaged'][kind] = s.tx['damaged'].get(kind, 0) + 1
                    s.say('tx damaged_%s (LINK_SEQUENCE %d not used)' % (kind, n.link))
                # Silence after it, longer than the CIS assembly limit: a truncated frame ends in a
                # timeout there instead of running into the next frame.
                quiet = s.now() + QUIET_AFTER_DAMAGE_S
                next_result = max(next_result, quiet) if next_result is not None else None
                next_status = max(next_status, quiet) if next_status is not None else None
                continue
            if next_result is not None and t >= next_result:
                fields = dict(settings['result'])
                if args.result_mode == 'new' or judged_at is None:
                    seq = n.result_seq + 1
                    judged_at = t
                    fields['vision_age_ms'] = base_age
                else:
                    seq = n.result_seq
                    fields['vision_age_ms'] = min(base_age + int((t - judged_at) * 1000.0), 0xFFFF)
                values, payload = built(result_msg, n.boot, seq, fields, args.allow_invalid)
                link = n.link
                if s.send(frame_bytes(result_msg.type, link, payload), result_msg.name, values, link):
                    s.tx[result_msg.name] += 1
                    n.result_seq, n.link = seq, (link + 1) & 0xFFFF
                    if args.verbose:
                        s.say('tx %s link=%d %s' % (result_msg.name, link, fields_text(result_msg, values)))
                next_result += result_every
                if next_result <= t:      # fell a whole period behind: no catch-up burst
                    next_result = t + result_every
                continue
            if next_status is not None and t >= next_status:
                seq = n.status_seq + 1
                values, payload = built(status_msg, n.boot, seq, settings['status'], args.allow_invalid)
                link = n.link
                if s.send(frame_bytes(status_msg.type, link, payload), status_msg.name, values, link):
                    s.tx[status_msg.name] += 1
                    n.status_seq, n.link = seq, (link + 1) & 0xFFFF
                    if args.verbose:
                        s.say('tx %s link=%d %s' % (status_msg.name, link, fields_text(status_msg, values)))
                next_status += status_every
                if next_status <= t:
                    next_status = t + status_every
                continue
            waits = [x for x in (next_result, next_status, injections[0][0] if injections else None, duration)
                     if x is not None]
            time.sleep(max(0.0, min(0.005, min(waits) - t)))
    except KeyboardInterrupt:
        s.say('stopped (Ctrl+C)')
    finally:
        s.close()
        n.save()
    out.write('SUMMARY %s\n' % json.dumps(s.finish(n.as_dict()), sort_keys=True))
    return 0


# ------------------------------------------------------------------- main ---

def build_parser():
    p = argparse.ArgumentParser(prog='pi_bench.py', description=__doc__.split('\n', 1)[0])
    sub = p.add_subparsers(dest='command')

    f = sub.add_parser('frame', help='print one frame (no serial port)')
    f.add_argument('message', choices=('result', 'status', 'permission'))
    f.add_argument('--boot', help='PI_BOOT_ID (CIS_BOOT_ID for permission), hex')
    f.add_argument('--seq', default='1', help='VISION / STATUS / UPDATE sequence (default 1)')
    f.add_argument('--link', default='1', help='LINK_SEQUENCE (default 1)')
    f.add_argument('--bad', choices=BAD_KINDS)
    f.add_argument('--allow-invalid', action='store_true')
    f.add_argument('fields', nargs='*')

    d = sub.add_parser('decode', help='decode frames given as hex (no serial port)')
    d.add_argument('hex', nargs='+')

    sub.add_parser('ports', help='list serial ports (pyserial)')

    for name, helptext in (('send', 'send one frame, then listen'),
                           ('run', 'send periodically and print what the CIS relays')):
        c = sub.add_parser(name, help=helptext)
        c.add_argument('--port', required=True, help='serial port of the CIS UART-C (e.g. COM5)')
        c.add_argument('--baud', type=int, default=115200)
        c.add_argument('--state', help='JSON file with the BOOT and numbers to continue (read, then written)')
        c.add_argument('--new-boot', action='store_true', help='play a Pi restart: next BOOT, numbers from 1')
        c.add_argument('--boot', help='PI_BOOT_ID, hex (default: state, else 0x%08X)' % DEFAULT_BOOT)
        c.add_argument('--link', help='LINK_SEQUENCE of the first frame (default: state, else 1)')
        c.add_argument('--log', help='JSON lines of every sent / received frame (appended)')
        c.add_argument('--allow-invalid', action='store_true')
        c.add_argument('--verbose', action='store_true', help='also print each sent frame')
        if name == 'send':
            c.add_argument('message', choices=('result', 'status'))
            c.add_argument('--seq', help='VISION / STATUS sequence (default: state + 1)')
            c.add_argument('--bad', choices=BAD_KINDS)
            c.add_argument('--listen', default='1.0', help='seconds to read after sending (default 1)')
        else:
            c.add_argument('--duration', required=True, help='seconds')
            c.add_argument('--result-every', default='100', help='ms, 0 = none (default 100)')
            c.add_argument('--status-every', default='1000', help='ms, 0 = none (default 1000)')
            c.add_argument('--result-mode', choices=('new', 'repeat'), default='new',
                           help='new: VISION_SEQUENCE + 1 each frame; repeat: one judgement, AGE grows')
            c.add_argument('--result-seq', help='last VISION_SEQUENCE used (default: state, else 0)')
            c.add_argument('--status-seq', help='last STATUS_SEQUENCE used (default: state, else 0)')
            c.add_argument('--inject', action='append', help='KIND@SECONDS: a damaged result frame at that time')
        c.add_argument('fields', nargs='*')
    return p


def main(argv=None, out=None):
    out = out or sys.stdout
    parser = build_parser()
    # field=value words may stand before, between or after the options. argparse fills the
    # nargs='*' positional only once (before Python 3.13 not at all once an option came first),
    # so the rest come back as extras: keep them in order, refuse anything that looks like an option.
    args, extra = parser.parse_known_args(argv)
    unknown = [w for w in extra if w.startswith('-') or not hasattr(args, 'fields')]
    if unknown:
        parser.error('unrecognized arguments: %s' % ' '.join(unknown))
    if extra:
        args.fields = list(args.fields) + extra
    handlers = {'frame': cmd_frame, 'decode': cmd_decode, 'ports': cmd_ports, 'send': cmd_send, 'run': cmd_run}
    if args.command not in handlers:
        parser.print_help(out)
        return 2
    try:
        return handlers[args.command](args, out)
    except UsageError as e:
        out.write('error: %s\n' % e)
        return 2


if __name__ == '__main__':
    sys.exit(main())
