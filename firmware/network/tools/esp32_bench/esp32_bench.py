#!/usr/bin/env python3
"""esp32_bench: host stand-in for the ESP32 on the UART-M link of a DOMAIN node (board tests).

The laptop plays the ESP32. Over a serial port (the DOMAIN board's UART-M) it
sends M_CONTEXT periodically and, at chosen times, M_REQUEST, M_QUERY,
M_WARNING_ACK, a session change or a damaged frame. It decodes every frame
DOMAIN sends, checks it and ends with a verdict. Frame layout and payloads
follow docs/network/interface/03_COMMUNICATION/20_UART_M.md:

  B0..1 SYNC 0xa5 0x5a, B2 VERSION 2, B3 TYPE, B4..5 PAYLOAD_LENGTH (u16 LE),
  B6..7 LINK_SEQUENCE (u16 LE, per direction, modulo 65536), payload,
  CRC16-CCITT-FALSE (poly 0x1021, init 0xffff, no reflection, xorout 0) over
  VERSION .. payload end, stored little-endian. Frame = payload + 10 bytes.

Field names, order and widths are those of the repository codec structures
(net_wire_uart_dto.h, net_wire_uart_m_core.h, net_wire_uart_m_projection.h).
The value checks of the four messages this tool sends, and of M_DOMAIN_ALIVE,
mirror the codec (net_wire_uart_dto.c, net_wire_uart_m_core.c): a record the
codec refuses is not sent unless --allow-invalid is given. Other DOMAIN frames
get the frame checks only (SYNC, VERSION, type, length, CRC, assembly time),
the context, LINK_SEQUENCE and AGE checks below. tests/host/test_esp32_bench.py
compares the bytes of every UART-M message with the codec through
tools/vss_bench (uartm encode / decode), both ways.

Commands (frame and decode need only the standard library; ports, send and run
need pyserial: python -m pip install pyserial):

  esp32_bench.py frame context|request|query|ack [--device N] [--session N]
                    [--link N] [--id N] [--bad KIND] [--allow-invalid]
                    [field=value ...]
  esp32_bench.py decode HEX ...
  esp32_bench.py ports
  esp32_bench.py send --port PORT context|request|query|ack [--state FILE]
                    [--new-session] [--device N] [--session N] [--link N]
                    [--bad KIND] [--listen S] [--log FILE] [field=value ...]
  esp32_bench.py run --port PORT --duration S [--state FILE] [--new-session]
                    [--device N] [--session N] [--link N]
                    [--context-every MS] [--context-mode new|repeat]
                    [--session-change-at S ...] [--request KIND:OP[:ARGS]@S ...]
                    [--query SCOPE@S ...] [--ack TYPE:OCCURRENCE@S ...]
                    [--inject KIND@S ...] [--log FILE] [field=value ...]

field=value sets a payload field of the message(s) sent by its codec name;
device_context_id, session_id, the update sequences and the request / query
numbers come from the options and the state. Damaged frames (--bad, --inject,
always an M_CONTEXT): crc (last byte changed), trunc (first 12 bytes only), len
(PAYLOAD_LENGTH + 1, CRC recomputed), version (VERSION 1, CRC recomputed), type
(TYPE 0x6f, CRC recomputed), noise (8 bytes without a SYNC). A damaged frame
does not use a LINK_SEQUENCE number. run keeps 80 ms of silence after one
(longer than the 50 ms assembly limit of the DOMAIN node).

Numbers across runs (--state FILE): DOMAIN keeps the last LINK_SEQUENCE of each
uplink type of the current device and session and drops older ones, so a second
run with the same session must continue them. --state reads the device, the
session, the next LINK_SEQUENCE, the three M_CONTEXT update sequences and the
last REQUEST_ID / QUERY_ID before the run and writes them after it; --device,
--session, --link override it. --new-session plays a new app session: SESSION_ID
+ 1 (or --session), the other numbers continue.

Checks of the received frames (DOMAIN -> ESP32):
  - frame: SYNC, VERSION 2, a downlink type, its exact length, CRC, complete
    within 50 ms; anything else is a line error. When the port opens, the
    driver buffer is flushed and what arrives in the next 200 ms is read and
    dropped (a USB bridge may hold frames from before); bytes dropped after that
    before the first good frame, fewer than the longest downlink frame (the tail
    of a frame cut by the drain), are counted apart, and so are the bytes of a
    frame still arriving when reading stops.
  - LINK_SEQUENCE: one counter for the downlink. +1 is normal; 2..32767 ahead
    is a skip (allowed, counted); the same or an older number is a failure. A
    new non-zero DOMAIN_BOOT_ID (DOMAIN restarted) starts the counter again
    (counted).
  - context: a frame with DEVICE_CONTEXT_ID / SESSION_ID (every type but
    M_DOMAIN_ALIVE) must carry a context this tool sent. One of an earlier
    session of this run, or the context of the --state file (an earlier run;
    DOMAIN keeps a context until the link or DOMAIN restarts), is counted
    apart. An unknown context fails once DOMAIN has taken one of this run;
    before that it is counted apart (DOMAIN may hold one this tool never knew).
  - AGE: a frame that repeats the same observation never carries a smaller
    AGE. The observation of each AGE field is named by the fields of
    net_upper_uart_original_age (net_upper_source.c), AGE_KEYS below, and
    compared only under its conditions (AGE_GUARDS). 65535 is unknown.
  - M_DOMAIN_ALIVE: codec values, at least one, no gap over 750 ms (the ESP32
    receive timeout of 20_UART_M.md), counting the time from the port open to
    the first one and from the last one to the end of reading. INIT / NO_DATA /
    AGE 65535 is what the DOMAIN board sends while its alive_state hook is empty
    (noted, not failed).
  - context taken: at least one frame with the current context arrived. None
    is the HOLD path (DOMAIN does not take M_CONTEXT while its owner functions
    for it are not bound), not a failure.
  - each M_REQUEST: an M_RESULT with its REQUEST_SESSION / REQUEST_ID; each
    M_QUERY: frames with its QUERY_ID and an M_QUERY_END whose ITEM_COUNT is
    the number of them. Missing answers are HOLD when the context the request
    was sent with was never taken, else OPEN (no documented deadline), not
    failures.

--context-mode repeat: DOMAIN holds a repeat whose AGE is below the earlier AGE
plus the time it measured between the two frames. The tool rounds the real
elapsed time up and adds one tenth of the M_CONTEXT period (at least 3 ms, at
most 10 ms) per repeat, so a timing difference between the laptop and DOMAIN up
to about that step per frame does not make a repeat look younger. An
observation's AGE is then above the real one by at most about 100 ms at the
default period (about 300 ms at a 10 ms period).

send and run print each received frame as one line, then "SUMMARY {json}" and
(run) "VERDICT {json}". --log appends every sent and received frame as JSON
lines, then the summary and the verdict. Exit: 0 = done (the verdict is in the
output), 1 = decode found a damaged or refused frame, 2 = usage, a port that
cannot be opened, or a value the codec refuses.
"""
from __future__ import annotations

import argparse
import json
import sys
import threading
import time

SYNC = (0xA5, 0x5A)
VERSION = 2
OVERHEAD = 10
UPLINK, DOWNLINK = 'uplink', 'downlink'          # ESP32 -> DOMAIN, DOMAIN -> ESP32
ASSEMBLY_LIMIT_MS = 50.0                          # the DOMAIN node's UART-M assembly limit
ALIVE_TIMEOUT_MS = 750.0                          # M_DOMAIN_ALIVE receive timeout (period 250 ms)
DEFAULT_DEVICE = 7
DEFAULT_SESSION = 1
QUIET_AFTER_DAMAGE_S = 0.08
CONTEXT_REFRESH_MS = 1000                         # --context-mode repeat: a new observation this often
REPEAT_AGE_STEP_MS = (3.0, 10.0)                  # repeat: added AGE per repeat, period / 10 within these
OPEN_DRAIN_S = 0.2                                # after the port opens: read and drop (bridge buffers)
ALIVE_VALUES_MAX = 16                             # distinct M_DOMAIN_ALIVE values kept for the summary
BAD_KINDS = ('crc', 'trunc', 'len', 'version', 'type', 'noise')
AGE_UNKNOWN = 0xFFFF

# Payload fields of every UART-M message in order: (codec member name, width in
# bytes; negative = signed two's complement).
FIELDS = {
    0x01: (('device_context_id', 4), ('session_id', 8), ('device_registration_state', 1),
           ('bt_connection_state', 1), ('app_active_state', 1), ('app_quality', 1), ('app_update_sequence', 4),
           ('app_age_ms', 2), ('proximity_state', 1), ('proximity_validity', 1), ('proximity_reason', 1),
           ('proximity_update_sequence', 4), ('proximity_age_ms', 2), ('registration_connection_quality', 1),
           ('registration_connection_update_sequence', 4), ('registration_connection_age_ms', 2)),
    0x02: (('domain_boot_id', 4), ('update_sequence', 4), ('source_age_ms', 2), ('domain_state', 1),
           ('value_quality', 1)),
    0x10: (('device_context_id', 4), ('session_id', 8), ('request_id', 4), ('kind', 1), ('operation', 1),
           ('args[0]', 1), ('args[1]', 1), ('args[2]', 1), ('args[3]', 1), ('args[4]', 1), ('args[5]', 1)),
    0x11: (('device_context_id', 4), ('session_id', 8), ('query_id', 4), ('query_scope', 1), ('reserved', 1),
           ('request_session', 8), ('request_id', 4), ('cursor', 2)),
    0x12: (('device_context_id', 4), ('session_id', 8), ('warning_type', 1), ('occurrence_id', 4),
           ('warning_domain_boot', 4)),
    0x20: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('request_session', 8), ('request_id', 4), ('result', 1), ('result_reason', 2),
           ('result_confirmation', 1), ('command_target', 1), ('command_id', 4), ('origin', 1),
           ('result_age_ms', 2)),
    0x21: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('warning_type', 1), ('occurrence_id', 4), ('severity', 1), ('warning_state', 1), ('read_state', 1),
           ('value_quality', 1), ('quality_reason', 1), ('source_boot', 4), ('source_sequence', 4),
           ('source_age_ms', 2)),
    0x22: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('items[0].function_id', 1), ('items[0].availability', 1), ('items[0].reason', 2),
           ('items[1].function_id', 1), ('items[1].availability', 1), ('items[1].reason', 2),
           ('items[2].function_id', 1), ('items[2].availability', 1), ('items[2].reason', 2),
           ('items[3].function_id', 1), ('items[3].availability', 1), ('items[3].reason', 2),
           ('items[4].function_id', 1), ('items[4].availability', 1), ('items[4].reason', 2),
           ('items[5].function_id', 1), ('items[5].availability', 1), ('items[5].reason', 2),
           ('items[6].function_id', 1), ('items[6].availability', 1), ('items[6].reason', 2),
           ('items[7].function_id', 1), ('items[7].availability', 1), ('items[7].reason', 2),
           ('items[8].function_id', 1), ('items[8].availability', 1), ('items[8].reason', 2),
           ('update_sequence', 4), ('source_age_ms', 2)),
    0x23: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('digital_key_setting_state', 1), ('digital_key_availability', 1), ('reason', 2),
           ('update_sequence', 4), ('source_age_ms', 2)),
    0x24: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('auto_occurrence_id', 4), ('related_door_command_id', 4), ('origin', 1), ('result', 1),
           ('result_reason', 2), ('result_confirmation', 1), ('result_age_ms', 2)),
    0x25: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('target_temperature_centi_c', -2), ('climate_auto', 1), ('user_fan_level', 1),
           ('interior_light_enabled', 1), ('normal_light_level_percent', 1), ('normal_light_rgb[0]', 1),
           ('normal_light_rgb[1]', 1), ('normal_light_rgb[2]', 1), ('value_quality', 1), ('update_sequence', 4),
           ('source_age_ms', 2)),
    0x30: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('door_lock_state', 1),
           ('door_open_state', 1), ('door_composite_state', 1), ('lock_quality', 1), ('open_quality', 1),
           ('composite_quality', 1), ('update_sequence', 4), ('source_age_ms', 2)),
    0x31: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('fan_command_level', 1),
           ('fan_measured_level', 1), ('thermal_direction_state', 1), ('thermal_output_percent', 1),
           ('heat_removal_state', 1), ('fan_command_quality', 1), ('fan_measured_quality', 1),
           ('thermal_direction_quality', 1), ('thermal_output_quality', 1), ('heat_removal_quality', 1),
           ('update_sequence', 4), ('source_age_ms', 2)),
    0x32: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('interior_light_type_state', 1),
           ('light_level_percent', 1), ('color_r', 1), ('color_g', 1), ('color_b', 1),
           ('interior_light_apply_result', 1), ('light_physical_feedback_capability', 1),
           ('light_type_quality', 1), ('light_level_quality', 1), ('light_color_quality', 1),
           ('light_apply_quality', 1), ('update_sequence', 4), ('source_age_ms', 2)),
    0x33: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('bcm_ecu_state', 1),
           ('fault_mask', 2), ('active_fault_category', 1), ('active_fault_code', 2), ('affected_function', 1),
           ('recovering', 1), ('update_sequence', 4), ('source_age_ms', 2)),
    0x34: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('temperature_centi_c', -2),
           ('temperature_sequence', 4), ('temperature_age_ms', 2), ('temperature_validity', 1),
           ('temperature_reason', 1), ('humidity_centi_percent', 2), ('humidity_sequence', 4),
           ('humidity_age_ms', 2), ('humidity_validity', 1), ('humidity_reason', 1), ('illuminance_lux', 4),
           ('illuminance_sequence', 4), ('illuminance_age_ms', 2), ('illuminance_validity', 1),
           ('illuminance_reason', 1)),
    0x35: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('pi_boot_id', 4),
           ('vision_sequence', 4), ('vision_age_ms', 2), ('occupant_presence', 1), ('presence_validity', 1),
           ('presence_reason', 1), ('occupant_count', 1), ('count_validity', 1), ('count_reason', 1),
           ('vision_function_status', 1)),
    0x36: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('update_sequence', 4),
           ('source_age_ms', 2), ('rear_distance_deci_cm', 2), ('validity', 1), ('quality_reason', 1),
           ('proximity_status', 1)),
    0x37: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('cis_state', 1), ('vision_status', 1),
           ('temperature_status', 1), ('humidity_status', 1), ('illuminance_status', 1), ('rear_status', 1),
           ('pi_uart_status', 1), ('can_interface_status', 1), ('fault_mask', 2), ('active_fault', 2),
           ('recovering', 1), ('last_fault', 2), ('last_fault_occurrence', 4), ('affected_function', 1),
           ('update_sequence', 4), ('source_age_ms', 2)),
    0x38: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('motion_state', 1),
           ('window_ecu_state', 1), ('window_position', 1), ('window_value_quality', 1),
           ('fully_open_state', 1), ('fully_closed_state', 1), ('antipinch_protection_status', 1),
           ('reverse_state', 1), ('update_sequence', 4), ('source_age_ms', 2)),
    0x39: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('fault_category', 1),
           ('fault_code', 2), ('fault_status', 1), ('occurrence_id', 4), ('update_sequence', 4),
           ('source_age_ms', 2)),
    0x3A: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('original_can_boot_id', 4), ('original_can_tx_sequence', 4), ('vss_state', 1),
           ('vss_availability', 1), ('vss_accepting_events', 1), ('vss_fault_active', 1), ('fault_mask', 1),
           ('vss_last_fault', 2), ('last_fault_occurrence', 4), ('last_fault_age_ms', 2),
           ('last_fault_quality', 1), ('update_sequence', 4), ('source_age_ms', 2)),
    0x40: (('device_context_id', 4), ('session_id', 8), ('domain_boot_id', 4), ('query_id', 4),
           ('query_scope', 1), ('query_status', 1), ('item_count', 2)),
}
NAMES = {0x01: 'M_CONTEXT', 0x02: 'M_DOMAIN_ALIVE', 0x10: 'M_REQUEST', 0x11: 'M_QUERY', 0x12: 'M_WARNING_ACK',
         0x20: 'M_RESULT', 0x21: 'M_WARNING', 0x22: 'M_AVAILABILITY', 0x23: 'M_DIGITAL_STATUS',
         0x24: 'M_DIGITAL_RESULT', 0x25: 'M_USER_SETTINGS', 0x30: 'M_BCM_DOOR_STATE',
         0x31: 'M_BCM_CLIMATE_STATE', 0x32: 'M_BCM_LIGHT_STATE', 0x33: 'M_BCM_STATUS',
         0x34: 'M_CIS_ENVIRONMENT', 0x35: 'M_CIS_OCCUPANT', 0x36: 'M_CIS_REAR', 0x37: 'M_CIS_STATUS',
         0x38: 'M_WINDOW_STATE', 0x39: 'M_WINDOW_FAULT', 0x3A: 'M_VSS_STATUS', 0x40: 'M_QUERY_END'}
UPLINK_TYPES = (0x01, 0x10, 0x11, 0x12)
TYPE_CONTEXT, TYPE_ALIVE, TYPE_REQUEST, TYPE_QUERY, TYPE_ACK = 0x01, 0x02, 0x10, 0x11, 0x12
TYPE_RESULT, TYPE_QUERY_END = 0x20, 0x40
WORDS = {'context': TYPE_CONTEXT, 'request': TYPE_REQUEST, 'query': TYPE_QUERY, 'ack': TYPE_ACK}
# Set by the options and the state, never by field=value.
IDENTITY = {TYPE_CONTEXT: ('device_context_id', 'session_id', 'app_update_sequence', 'proximity_update_sequence',
                           'registration_connection_update_sequence'),
            TYPE_REQUEST: ('device_context_id', 'session_id', 'request_id'),
            TYPE_QUERY: ('device_context_id', 'session_id', 'query_id'),
            TYPE_ACK: ('device_context_id', 'session_id')}
# Documented valid values (20_UART_M.md): registered, BT connected, app active,
# proximity FAR, every quality OK, AGE 20 ms; a door LOCK request; a status
# query; an acknowledgement of warning type 1, occurrence 1.
DEFAULTS = {
    TYPE_CONTEXT: {'device_registration_state': 1, 'bt_connection_state': 1, 'app_active_state': 1,
                   'app_quality': 0, 'app_age_ms': 20, 'proximity_state': 0, 'proximity_validity': 1,
                   'proximity_reason': 0, 'proximity_age_ms': 20, 'registration_connection_quality': 0,
                   'registration_connection_age_ms': 20},
    TYPE_REQUEST: {'kind': 1, 'operation': 0},
    TYPE_QUERY: {'query_scope': 0},
    TYPE_ACK: {'warning_type': 1, 'occurrence_id': 1, 'warning_domain_boot': 0},
}
# AGE fields of the downlink messages and the fields that name their original
# observation, as net_upper_uart_original_age (net_upper_source.c) compares them:
# a repeat of the same observation never carries a smaller AGE.
SOURCE_KEY = ('original_can_boot_id', 'update_sequence')
AGE_KEYS = {
    0x02: {'source_age_ms': ('domain_boot_id', 'update_sequence')},
    0x20: {'result_age_ms': ('domain_boot_id', 'request_session', 'request_id', 'command_id')},
    0x21: {'source_age_ms': ('domain_boot_id', 'source_boot', 'source_sequence')},
    0x22: {'source_age_ms': ('domain_boot_id', 'update_sequence')},
    0x23: {'source_age_ms': ('domain_boot_id', 'update_sequence')},
    0x24: {'result_age_ms': ('domain_boot_id', 'auto_occurrence_id', 'related_door_command_id')},
    0x25: {'source_age_ms': ('domain_boot_id', 'update_sequence')},
    0x30: {'source_age_ms': SOURCE_KEY},
    0x31: {'source_age_ms': SOURCE_KEY},
    0x32: {'source_age_ms': SOURCE_KEY},
    0x33: {'source_age_ms': SOURCE_KEY},
    0x34: {'temperature_age_ms': ('original_can_boot_id', 'temperature_sequence'),
           'humidity_age_ms': ('original_can_boot_id', 'humidity_sequence'),
           'illuminance_age_ms': ('original_can_boot_id', 'illuminance_sequence')},
    0x35: {'vision_age_ms': ('original_can_boot_id', 'pi_boot_id', 'vision_sequence')},
    0x36: {'source_age_ms': SOURCE_KEY},
    0x37: {'source_age_ms': SOURCE_KEY},
    0x38: {'source_age_ms': SOURCE_KEY},
    0x39: {'source_age_ms': SOURCE_KEY},
    0x3A: {'last_fault_age_ms': ('original_can_boot_id', 'last_fault_occurrence'), 'source_age_ms': SOURCE_KEY},
}
# Conditions on both frames under which that AGE is compared at all.
AGE_GUARDS = {(0x3A, 'last_fault_age_ms'): (('last_fault_quality', 0),)}


def payload_length(type_):
    return sum(abs(w) for _, w in FIELDS[type_])


# Fewer bytes than the longest downlink frame: what is left of a frame cut when reading starts.
LEAD_LIMIT = max(payload_length(t) for t in FIELDS if t not in UPLINK_TYPES) + OVERHEAD


def direction(type_):
    return UPLINK if type_ in UPLINK_TYPES else DOWNLINK


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


def encode_payload(type_, values):
    out = b''
    for name, width in FIELDS[type_]:
        value, n = values.get(name, 0), abs(width)
        if width < 0:
            if not -(1 << (8 * n - 1)) <= value < (1 << (8 * n - 1)):
                raise UsageError('%s=%s does not fit a signed %d-byte field' % (name, value, n))
            value &= (1 << (8 * n)) - 1
        elif not 0 <= value < (1 << (8 * n)):
            raise UsageError('%s=%s does not fit %d byte(s)' % (name, value, n))
        out += le(value, n)
    return out


def decode_payload(type_, payload):
    values, pos = {}, 0
    for name, width in FIELDS[type_]:
        n = abs(width)
        v = from_le(payload[pos:pos + n])
        if width < 0 and v >= 1 << (8 * n - 1):
            v -= 1 << (8 * n)
        values[name] = v
        pos += n
    return values


def check(type_, v):
    """The codec verdict on the values (OK, MALFORMED, CONTRACT_HOLD; the C checks in the
    same order) for the uplink messages and M_DOMAIN_ALIVE; None for the other types."""
    def tristate(x):
        return x <= 1 or x == 0xFF

    if type_ == TYPE_CONTEXT:
        if (v['device_context_id'] == 0 or not tristate(v['device_registration_state']) or
                not tristate(v['bt_connection_state']) or not tristate(v['app_active_state']) or
                not tristate(v['proximity_state']) or v['app_quality'] > 3 or
                v['registration_connection_quality'] > 3):
            return 'MALFORMED'
        if v['proximity_validity'] > 1 or v['proximity_reason'] > 9:
            return 'MALFORMED'
        if v['proximity_validity'] == 1 and (v['proximity_state'] == 0xFF or v['proximity_reason'] != 0):
            return 'CONTRACT_HOLD'
        if ((v['app_quality'] == 0 and v['app_active_state'] == 0xFF) or
                (v['registration_connection_quality'] == 0 and
                 (v['device_registration_state'] == 0xFF or v['bt_connection_state'] == 0xFF))):
            return 'CONTRACT_HOLD'
        return 'OK'
    if type_ == TYPE_REQUEST:
        args = [v['args[%d]' % i] for i in range(6)]
        kind, op = v['kind'], v['operation']
        if v['device_context_id'] == 0:
            return 'MALFORMED'
        if kind == 1:
            if op > 1:
                return 'MALFORMED'
            used = 0
        elif kind == 2:
            if op == 1:
                used = 2
            elif op == 2 and args[0] <= 1:
                used = 1
            elif op == 3 and args[0] <= 3:
                used = 1
            else:
                return 'MALFORMED'
        elif kind == 3:
            if op == 1 and args[0] <= 1:
                used = 1
            elif op == 2 and args[0] <= 100:
                used = 1
            elif op == 3:
                used = 3
            else:
                return 'MALFORMED'
        elif kind == 4:
            if op != 1 or args[0] > 1:
                return 'MALFORMED'
            used = 1
        else:
            return 'MALFORMED'
        return 'MALFORMED' if any(args[used:]) else 'OK'
    if type_ == TYPE_QUERY:
        if (v['device_context_id'] == 0 or v['query_id'] == 0 or v['query_scope'] > 3 or
                v['reserved'] != 0 or v['cursor'] != 0):
            return 'MALFORMED'
        if v['query_scope'] == 0 and (v['request_session'] != 0 or v['request_id'] != 0):
            return 'MALFORMED'
        if ((v['query_scope'] == 1 and (v['request_session'] == 0) != (v['request_id'] == 0)) or
                (v['query_scope'] >= 2 and (v['request_session'] != 0 or v['request_id'] != 0))):
            return 'CONTRACT_HOLD'
        return 'OK'
    if type_ == TYPE_ACK:
        if v['device_context_id'] == 0 or not 1 <= v['warning_type'] <= 8:
            return 'MALFORMED'
        return 'CONTRACT_HOLD' if v['warning_domain_boot'] == 0 else 'OK'
    if type_ == TYPE_ALIVE:
        if v['domain_boot_id'] == 0 or v['domain_state'] > 3 or v['value_quality'] > 3:
            return 'MALFORMED'
        return 'OK'
    return None


def damage(kind, type_, link, payload):
    """Bytes of a damaged frame of this record (see the module text)."""
    good = frame_bytes(type_, link, payload)
    if kind == 'crc':
        return good[:-1] + bytes((good[-1] ^ 0xFF,))
    if kind == 'trunc':
        return good[:12]
    if kind == 'len':
        return frame_bytes(type_, link, payload, length=len(payload) + 1)
    if kind == 'version':
        return frame_bytes(type_, link, payload, version=1)
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
    """Receive side of one UART-M direction (None: both), like the node's
    stream: resync one byte at a time on a bad SYNC, header or CRC, drop a
    frame not complete within the assembly limit. Events: ('frame', type, link,
    values, bytes, t), ('bad_header', bytes), ('bad_crc', bytes),
    ('timeout', bytes), ('noise', n)."""

    def __init__(self, direction_=DOWNLINK, assembly_limit_ms=ASSEMBLY_LIMIT_MS):
        self.direction = direction_
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
        for b in data:
            self.buf.append(b)
            self.at.append(t)
        return self.poll(t)

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
                type_ = self.buf[3]
                length = from_le(self.buf[4:6])
                if (self.buf[2] != VERSION or type_ not in FIELDS or
                        (self.direction is not None and direction(type_) != self.direction) or
                        length != payload_length(type_)):
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
            received = self.at[wanted - 1]
            del self.buf[:wanted]
            del self.at[:wanted]
            self.counts['frames'] += 1
            events.append(('frame', raw[3], from_le(raw[6:8]), decode_payload(raw[3], raw[8:-2]), raw, received))
        return [tuple(e) for e in events]


# -------------------------------------------------------------- records ---

def parse_int(text, base=0):
    try:
        return int(text, base)
    except (TypeError, ValueError):
        raise UsageError('not a number: %r' % text)


def field_settings(words, types):
    """field=value words -> {type: {field: value}}; a name of none of the types is an error."""
    out = {t: {} for t in types}
    for word in words:
        if '=' not in word or word.startswith('='):
            raise UsageError('expected field=value, got %r' % word)
        name, value = word.split('=', 1)
        hit = False
        for t in types:
            for fname, width in FIELDS[t]:
                if fname == name:
                    if fname in IDENTITY[t]:
                        raise UsageError('%s is set by the options and the state, not field=value' % name)
                    v = parse_int(value)
                    n = abs(width)
                    lo, hi = (-(1 << (8 * n - 1)), 1 << (8 * n - 1)) if width < 0 else (0, 1 << (8 * n))
                    if not lo <= v < hi:
                        raise UsageError('%s=%s does not fit %d byte(s)' % (name, value, n))
                    out[t][name] = v
                    hit = True
        if not hit:
            names = sorted({f for t in types for f, _ in FIELDS[t] if f not in IDENTITY[t]})
            raise UsageError('unknown field %r; settable: %s' % (name, ' '.join(names)))
    return out


def record(type_, identity, settings):
    """All fields: zero, then the documented defaults, field=value, the identity fields."""
    values = {name: 0 for name, _ in FIELDS[type_]}
    values.update(DEFAULTS.get(type_, {}))
    values.update(settings)
    values.update(identity)
    return values


def fields_text(type_, values):
    parts = []
    for name, _ in FIELDS[type_]:
        v = values[name]
        if name.endswith('boot_id') or name.endswith('_boot') or name == 'session_id' or name == 'request_session':
            parts.append('%s=0x%X' % (name, v))
        else:
            parts.append('%s=%d' % (name, v))
    return ' '.join(parts)


def built(type_, identity, settings, allow_invalid):
    values = record(type_, identity, settings)
    verdict = check(type_, values)
    if verdict != 'OK' and not allow_invalid:
        raise UsageError('the codec refuses these %s values (%s); --allow-invalid sends them'
                         % (NAMES[type_], verdict))
    return values, encode_payload(type_, values)


# -------------------------------------------------------- offline commands ---

def offline_identity(type_, args):
    device = parse_int(args.device) if args.device is not None else DEFAULT_DEVICE
    session = parse_int(args.session) if args.session is not None else DEFAULT_SESSION
    num = parse_int(args.id)
    ident = {'device_context_id': device, 'session_id': session}
    if type_ == TYPE_CONTEXT:
        ident.update(app_update_sequence=num, proximity_update_sequence=num,
                     registration_connection_update_sequence=num)
    elif type_ == TYPE_REQUEST:
        ident['request_id'] = num
    elif type_ == TYPE_QUERY:
        ident['query_id'] = num
    return ident


def cmd_frame(args, out):
    type_ = WORDS[args.message]
    settings = field_settings(args.fields, [type_])[type_]
    values = record(type_, offline_identity(type_, args), settings)
    verdict = check(type_, values)
    if verdict != 'OK' and not args.allow_invalid:
        out.write('error: the codec refuses these %s values (%s); nothing built (--allow-invalid builds it)\n'
                  % (NAMES[type_], verdict))
        return 2
    payload = encode_payload(type_, values)
    link = parse_int(args.link) & 0xFFFF
    data = damage(args.bad, type_, link, payload) if args.bad else frame_bytes(type_, link, payload)
    out.write('# %s type=0x%02x payload=%d B frame=%d B link=%d codec=%s%s\n' % (
        NAMES[type_], type_, len(payload), len(payload) + OVERHEAD, link, verdict,
        (' damaged=%s' % args.bad) if args.bad else ''))
    out.write('# fields: %s\n' % fields_text(type_, values))
    out.write('# payload: %s\n' % hex_text(payload))
    out.write('%s\n' % hex_text(data))
    return 0


def describe(event):
    kind = event[0]
    if kind == 'frame':
        _, type_, link, values, raw, _ = event
        verdict = check(type_, values)
        return 'frame %s link=%d crc=OK codec=%s %s' % (NAMES[type_], link, verdict or '-',
                                                       fields_text(type_, values))
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
        bad += e[0] != 'frame' or check(e[1], e[3]) not in ('OK', None)
    if stream.buf:
        out.write('incomplete %d byte(s) at the end: %s\n' % (len(stream.buf), hex_text(stream.buf)))
        bad += 1
    return 1 if bad else 0


# ------------------------------------------------- received-frame checks ---

def age_pairs(type_, values):
    """(AGE field, identity tuple) of each observation a frame carries (AGE_KEYS); a field whose
    AGE_GUARDS do not hold in this frame is not compared."""
    return [(field, (type_, field) + tuple(values[k] for k in keys))
            for field, keys in AGE_KEYS.get(type_, {}).items()
            if all(values[g] == want for g, want in AGE_GUARDS.get((type_, field), ()))]


class Checker:
    """What the received frames show, against what this tool sent."""

    def __init__(self, prior=()):
        self.contexts = []             # (device, session) sent, in order
        self.prior = set(prior)        # (device, session) of an earlier run (--state)
        self.taken = {}                # (device, session) -> first time a DOMAIN frame carried it
        self.domain_boot = None
        self.link_last = None
        self.link = {'steps': 0, 'skipped': 0, 'skipped_numbers': 0, 'backward': 0, 'restarts': 0}
        self.by_type = {}              # name -> [count, first t, last t, min gap, max gap]
        self.codec_not_ok = 0
        self.ctx = {'current': 0, 'earlier_session': 0, 'earlier_run': 0, 'unknown_before_taken': 0,
                    'unknown': 0}
        self.ages = {}                 # identity -> last AGE
        self.age = {'backward': 0, 'max_ms': {}, 'unknown': 0}
        self.alive = {'count': 0, 'first_t': None, 'last_t': None, 'gap_max_ms': None, 'boots': [],
                      'values': {}, 'frames_beyond_values': 0, 'not_ok': 0}
        self.results = []              # M_RESULT values
        self.query_items = {}          # query_id -> frames that carry it (End excluded)
        self.query_end = {}            # query_id -> M_QUERY_END values

    def sent_context(self, device, session):
        if (device, session) not in self.contexts:
            self.contexts.append((device, session))

    def frame(self, type_, link, values, t):
        name = NAMES[type_]
        boot = values.get('domain_boot_id')
        if boot:                           # 0 is no DOMAIN_BOOT_ID (refused by the codec)
            if self.domain_boot is not None and boot != self.domain_boot:
                self.link['restarts'] += 1
                self.link_last = None      # a restarted DOMAIN numbers its frames again
            self.domain_boot = boot
        if self.link_last is not None:
            step = (link - self.link_last) & 0xFFFF
            self.link['steps'] += 1
            if step == 0 or step >= 0x8000:
                self.link['backward'] += 1
            elif step > 1:
                self.link['skipped'] += 1
                self.link['skipped_numbers'] += step - 1
        self.link_last = link
        st = self.by_type.setdefault(name, [0, t, t, None, None])
        if st[0]:
            gap = (t - st[2]) * 1000.0
            st[3] = gap if st[3] is None else min(st[3], gap)
            st[4] = gap if st[4] is None else max(st[4], gap)
        st[0] += 1
        st[2] = t
        verdict = check(type_, values)
        if verdict not in ('OK', None):
            self.codec_not_ok += 1
        if 'session_id' in values:
            pair = (values['device_context_id'], values['session_id'])
            if pair in self.contexts:
                self.taken.setdefault(pair, t)
                self.ctx['current' if pair == self.contexts[-1] else 'earlier_session'] += 1
            elif pair in self.prior:
                self.ctx['earlier_run'] += 1
            elif self.taken:
                self.ctx['unknown'] += 1
            else:
                self.ctx['unknown_before_taken'] += 1
        for field, key in age_pairs(type_, values):
            age = values[field]
            if age == AGE_UNKNOWN:
                self.age['unknown'] += 1
                continue
            if key in self.ages and age < self.ages[key]:
                self.age['backward'] += 1
            self.ages[key] = age
            self.age['max_ms'][name] = max(age, self.age['max_ms'].get(name, 0))
        if type_ == TYPE_ALIVE:
            a = self.alive
            if a['last_t'] is None:
                a['first_t'] = t
            else:
                gap = round((t - a['last_t']) * 1000.0, 1)
                a['gap_max_ms'] = gap if a['gap_max_ms'] is None else max(a['gap_max_ms'], gap)
            a['last_t'] = t
            a['count'] += 1
            if not a['boots'] or a['boots'][-1] != values['domain_boot_id']:
                a['boots'].append(values['domain_boot_id'])
            key = (values['domain_state'], values['value_quality'], values['update_sequence'])
            age = values['source_age_ms']
            slot = a['values'].get(key)
            if slot is not None:
                slot[0] += 1
                slot[1], slot[2] = min(slot[1], age), max(slot[2], age)
            elif len(a['values']) < ALIVE_VALUES_MAX:
                a['values'][key] = [1, age, age]
            else:
                a['frames_beyond_values'] += 1
            a['not_ok'] += verdict != 'OK'
        if type_ == TYPE_RESULT:
            self.results.append(values)
        if type_ == TYPE_QUERY_END:
            self.query_end[values['query_id']] = values
        elif values.get('query_id'):
            self.query_items[values['query_id']] = self.query_items.get(values['query_id'], 0) + 1

    def last_domain_boot(self):
        return self.alive['boots'][-1] if self.alive['boots'] else 0

    def summary(self):
        types = {}
        for name, (n, first, last, gmin, gmax) in sorted(self.by_type.items()):
            types[name] = {'count': n, 'interval_ms_min': None if gmin is None else round(gmin, 1),
                           'interval_ms_max': None if gmax is None else round(gmax, 1)}
        a = self.alive
        return {'rx_types': types, 'link_sequence': dict(self.link), 'context_frames': dict(self.ctx),
                'codec_not_ok': self.codec_not_ok, 'age': {'backward': self.age['backward'],
                                                           'unknown': self.age['unknown'],
                                                           'max_ms': dict(sorted(self.age['max_ms'].items()))},
                'alive': {'count': a['count'], 'interval_ms_max': a['gap_max_ms'],
                          'domain_boot_ids': ['0x%08X' % b for b in a['boots']],
                          'values': [{'domain_state': s, 'value_quality': q, 'update_sequence': u, 'count': n,
                                      'source_age_ms_min': lo, 'source_age_ms_max': hi}
                                     for (s, q, u), (n, lo, hi) in a['values'].items()],
                          'frames_beyond_values': a['frames_beyond_values']}}

    def verdict(self, rx_damaged, requests, queries, end_t=None):
        """requests: (t, REQUEST_ID, device, session); queries: (t, QUERY_ID, device, session); end_t: when
        reading stopped (seconds from the port open)."""
        v = {}
        line_bad = sum(rx_damaged.values())
        v['line'] = {'verdict': 'FAIL' if line_bad else 'PASS', 'errors': dict(rx_damaged)}
        v['codec'] = {'verdict': 'FAIL' if self.codec_not_ok else 'PASS', 'not_ok': self.codec_not_ok}
        v['link_sequence'] = {'verdict': 'FAIL' if self.link['backward'] else 'PASS',
                              'backward': self.link['backward'], 'skipped': self.link['skipped'],
                              'restarts': self.link['restarts']}
        a = self.alive
        first = None if a['first_t'] is None else round(a['first_t'] * 1000.0, 1)
        last = None if a['last_t'] is None or end_t is None else round((end_t - a['last_t']) * 1000.0, 1)
        worst = max([x for x in (a['gap_max_ms'], first, last) if x is not None] or [0.0])
        alive_ok = a['count'] > 0 and a['not_ok'] == 0 and worst <= ALIVE_TIMEOUT_MS
        notes = []
        if any(s == 0 and q == 3 and lo == hi == AGE_UNKNOWN for (s, q, _), (_, lo, hi) in a['values'].items()):
            notes.append('INIT / NO_DATA / AGE 65535: HOLD path, DOMAIN alive_state hook empty')
        if len(a['boots']) > 1:
            notes.append('DOMAIN_BOOT_ID changed %d time(s): DOMAIN restarted' % (len(a['boots']) - 1))
        v['alive'] = {'verdict': 'PASS' if alive_ok else 'FAIL', 'count': a['count'],
                      'interval_ms_max': a['gap_max_ms'], 'first_after_open_ms': first,
                      'last_before_end_ms': last, 'limit_ms': ALIVE_TIMEOUT_MS, 'notes': notes}
        cnote = None
        if self.ctx['unknown_before_taken']:
            cnote = ('frames with a context this tool did not send, before DOMAIN carried one of this run '
                     '(DOMAIN may still hold an earlier run\'s context; --state keeps it known)')
        v['context'] = {'verdict': 'FAIL' if self.ctx['unknown'] else 'PASS', 'unknown': self.ctx['unknown'],
                        'unknown_before_taken': self.ctx['unknown_before_taken'],
                        'earlier_session': self.ctx['earlier_session'], 'earlier_run': self.ctx['earlier_run'],
                        'note': cnote}
        current = self.contexts[-1] if self.contexts else None
        taken = current in self.taken
        v['context_taken'] = {'verdict': 'TAKEN' if taken else 'HOLD',
                              'taken': [{'device': d, 'session': '0x%X' % s, 'at_s': round(t, 3)}
                                        for (d, s), t in self.taken.items()],
                              'note': None if taken else 'no frame with the current context: HOLD path, '
                                                         'DOMAIN does not take M_CONTEXT (owner functions '
                                                         'for it not bound)'}
        v['age'] = {'verdict': 'FAIL' if self.age['backward'] else 'PASS', 'backward': self.age['backward']}

        def waiting(device, session):
            return 'OPEN' if (device, session) in self.taken else 'HOLD'
        reqs = []
        for t, rid, device, session in requests:
            hit = [r for r in self.results if r['request_id'] == rid and r['request_session'] == session]
            reqs.append({'request_id': rid, 'at_s': round(t, 3),
                         'verdict': 'ANSWERED' if hit else waiting(device, session),
                         'result': hit[-1]['result'] if hit else None,
                         'result_reason': hit[-1]['result_reason'] if hit else None})
        v['requests'] = reqs
        qs, query_fail = [], 0
        for t, qid, device, session in queries:
            end = self.query_end.get(qid)
            items = self.query_items.get(qid, 0)
            if end is None:
                verdict = waiting(device, session)
            elif end['item_count'] != items:
                verdict = 'FAIL'
                query_fail += 1
            else:
                verdict = 'ENDED'
            qs.append({'query_id': qid, 'at_s': round(t, 3), 'verdict': verdict, 'items': items,
                       'item_count': end['item_count'] if end else None,
                       'query_status': end['query_status'] if end else None})
        v['queries'] = qs
        failed = [k for k in ('line', 'codec', 'link_sequence', 'alive', 'context', 'age') if v[k]['verdict'] == 'FAIL']
        if query_fail:
            failed.append('queries')
        v['overall'] = 'FAIL' if failed else 'PASS'
        v['failed'] = failed
        return v


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
    """One open port: a reader thread feeds the DOMAIN -> ESP32 stream; the main
    thread sends. Prints received frames, keeps counts, writes the log."""

    def __init__(self, port, baud, log_path, out, verbose=False, prior=()):
        self.out = out
        self.verbose = verbose
        self.lock = threading.Lock()       # console lines and the checker
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
        try:
            self.ser.reset_input_buffer()  # bytes the driver kept from before this run
        except Exception:
            pass
        self.rx_drained_bytes = 0          # read and dropped while a bridge empties its buffer
        until = time.monotonic() + OPEN_DRAIN_S
        while time.monotonic() < until:
            try:
                self.rx_drained_bytes += len(self.ser.read(self.ser.in_waiting or 1))
            except Exception:
                break
        self.t0 = time.monotonic()
        self.stream = Stream(DOWNLINK)
        self.checker = Checker(prior)
        self.tx = {}
        self.tx_damaged = {}
        self.rx_events = {'bad_header': 0, 'bad_crc': 0, 'timeout': 0, 'noise_bytes': 0}
        self.frames_seen = False
        self.rx_lead_bytes = 0             # dropped before the first good frame (a frame cut by the open)
        self.rx_tail_bytes = 0             # a frame still arriving when reading stopped
        self.end_t = None
        self.serial_errors = 0
        self.stop = threading.Event()
        self._log({'event': 'open', 'port': port, 'baud': baud, 'drained_bytes': self.rx_drained_bytes})
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

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
            _, type_, link, values, raw, t = e
            with self.lock:
                self.checker.frame(type_, link, values, t)
            verdict = check(type_, values)
            if self.verbose or type_ != TYPE_ALIVE or self.checker.alive['count'] <= 3:
                self.say('rx %s link=%d codec=%s %s' % (NAMES[type_], link, verdict or '-',
                                                       fields_text(type_, values)))
            self._log({'event': 'rx', 'type': NAMES[type_], 'link': link, 'codec': verdict, 'fields': values,
                       'hex': hex_text(raw)})
            self.frames_seen = True
            return
        dropped = e[1] if kind == 'noise' else 1
        if not self.frames_seen and self.rx_lead_bytes + dropped < LEAD_LIMIT:
            self.rx_lead_bytes += dropped
            self._log({'event': 'rx_lead_' + kind, 'detail': describe(e)})
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

    def sent(self, type_, values, link):
        self.tx[NAMES[type_]] = self.tx.get(NAMES[type_], 0) + 1
        if 'session_id' in values:
            with self.lock:
                self.checker.sent_context(values['device_context_id'], values['session_id'])
        if self.verbose or type_ != TYPE_CONTEXT:
            self.say('tx %s link=%d %s' % (NAMES[type_], link, fields_text(type_, values)))

    def close(self):
        """Stop reading and close the port; the log stays open for finish(). The reader polled the
        assembly limit up to its last read; a frame still arriving then is counted, not failed."""
        time.sleep(0.1)
        self.stop.set()
        self.reader.join(timeout=1.0)
        self.end_t = self.now()
        try:
            self.ser.close()
        except Exception:
            pass
        self.rx_tail_bytes = len(self.stream.buf)

    def finish(self, extra, requests=(), queries=(), verdict=True):
        """The summary (and, for run, the verdict) of the session; closes the log."""
        s = {'duration_s': round(self.now(), 3), 'tx': self.tx, 'tx_damaged': self.tx_damaged,
             'rx_damaged': self.rx_events, 'rx_drained_bytes': self.rx_drained_bytes,
             'rx_lead_bytes': self.rx_lead_bytes,
             'rx_tail_bytes': self.rx_tail_bytes, 'serial_errors': self.serial_errors}
        s.update(self.checker.summary())
        s.update(extra)
        self._log({'event': 'summary', 'summary': s})
        v = None
        if verdict:
            v = self.checker.verdict(self.rx_events, requests, queries, self.end_t)
            if self.serial_errors:
                v['overall'] = 'FAIL'
                v['failed'].append('serial')
            self._log({'event': 'verdict', 'verdict': v})
        with self.log_lock:
            if self.log:
                self.log.close()
                self.log = None
        return s, v


# ------------------------------------------------------- numbers and state ---

class Numbers:
    """Device, session, next LINK_SEQUENCE, the M_CONTEXT update sequences and
    the last REQUEST_ID / QUERY_ID of the ESP32 this tool plays, read from and
    written to --state (see the module text)."""

    KEYS = ('device', 'session', 'next_link', 'app_seq', 'proximity_seq', 'registration_seq', 'request_id',
            'query_id')

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
        # The context of the earlier run: DOMAIN may still carry it until the link or DOMAIN restarts.
        self.prior = ((int(st['device']), int(st['session'])),) if 'device' in st and 'session' in st else ()
        self.device = parse_int(args.device) if args.device is not None else int(st.get('device', DEFAULT_DEVICE))
        self.session = int(st.get('session', DEFAULT_SESSION))
        if args.session is not None:
            self.session = parse_int(args.session)
        elif args.new_session:
            self.session = (self.session + 1) & 0xFFFFFFFFFFFFFFFF
        self.link = parse_int(args.link) if args.link is not None else int(st.get('next_link', 1))
        self.link &= 0xFFFF
        self.app_seq = int(st.get('app_seq', 0))
        self.proximity_seq = int(st.get('proximity_seq', 0))
        self.registration_seq = int(st.get('registration_seq', 0))
        self.request_id = int(st.get('request_id', 0))
        self.query_id = int(st.get('query_id', 0))
        if not 0 < self.device <= 0xFFFFFFFF or not 0 <= self.session <= 0xFFFFFFFFFFFFFFFF:
            raise UsageError('DEVICE must be 1..0xffffffff and SESSION 0..0xffffffffffffffff')

    def save(self):
        if not self.path:
            return
        with open(self.path, 'w', encoding='utf-8') as fh:
            json.dump(self.as_state(), fh, sort_keys=True)
            fh.write('\n')

    def as_state(self):
        return {'device': self.device, 'session': self.session, 'next_link': self.link,
                'app_seq': self.app_seq, 'proximity_seq': self.proximity_seq,
                'registration_seq': self.registration_seq, 'request_id': self.request_id,
                'query_id': self.query_id}

    def as_dict(self):
        d = self.as_state()
        d['session'] = '0x%X' % self.session
        return {'numbers': d}

    def next_link(self):
        link = self.link
        self.link = (self.link + 1) & 0xFFFF
        return link

    def context_identity(self, new):
        if new:
            self.app_seq = (self.app_seq + 1) & 0xFFFFFFFF
            self.proximity_seq = (self.proximity_seq + 1) & 0xFFFFFFFF
            self.registration_seq = (self.registration_seq + 1) & 0xFFFFFFFF
        return {'device_context_id': self.device, 'session_id': self.session,
                'app_update_sequence': self.app_seq, 'proximity_update_sequence': self.proximity_seq,
                'registration_connection_update_sequence': self.registration_seq}

    def identity(self, type_):
        ident = {'device_context_id': self.device, 'session_id': self.session}
        if type_ == TYPE_REQUEST:
            self.request_id = (self.request_id + 1) & 0xFFFFFFFF
            ident['request_id'] = self.request_id
        elif type_ == TYPE_QUERY:
            self.query_id = (self.query_id + 1) & 0xFFFFFFFF or 1
            ident['query_id'] = self.query_id
        return ident


def context_values(n, settings, new, age_ms, allow_invalid):
    fields = dict(settings)
    if age_ms is not None:
        for name in ('app_age_ms', 'proximity_age_ms', 'registration_connection_age_ms'):
            fields[name] = min(fields.get(name, DEFAULTS[TYPE_CONTEXT][name]) + age_ms, AGE_UNKNOWN - 1)
    return built(TYPE_CONTEXT, n.context_identity(new), fields, allow_invalid)


def cmd_send(args, out):
    type_ = WORDS[args.message]
    settings = field_settings(args.fields, [type_])[type_]
    n = Numbers(args)
    if type_ == TYPE_CONTEXT:
        values, payload = context_values(n, settings, True, None, args.allow_invalid)
    else:
        values, payload = built(type_, n.identity(type_), settings, args.allow_invalid)
    s = Session(args.port, args.baud, args.log, out, args.verbose, n.prior)
    try:
        link = n.link
        if args.bad:
            if s.send(damage(args.bad, type_, link, payload), 'damaged_' + args.bad, values, link):
                s.tx_damaged[args.bad] = 1
                s.say('tx damaged_%s (LINK_SEQUENCE %d not used)' % (args.bad, link))
        elif s.send(frame_bytes(type_, link, payload), NAMES[type_], values, link):
            n.next_link()
            s.sent(type_, values, link)
        time.sleep(max(0.0, float(args.listen)))
    finally:
        s.close()
        n.save()
    summary, _ = s.finish(n.as_dict(), verdict=False)
    out.write('SUMMARY %s\n' % json.dumps(summary, sort_keys=True))
    return 0


def parse_events(items, option, parse):
    out = []
    for item in items or ():
        if '@' not in item:
            raise UsageError('%s wants VALUE@SECONDS, got %r' % (option, item))
        value, at = item.rsplit('@', 1)
        try:
            t = float(at)
        except ValueError:
            raise UsageError('%s time %r is not a number' % (option, at))
        out.append((t, parse(value)))
    return out


def parse_request(text):
    """KIND:OP[:A0[:A1 ...]] -> request fields (args as bytes; a temperature as A0 = i16 centi-degC)."""
    parts = text.split(':')
    if len(parts) < 2 or len(parts) > 8:
        raise UsageError('--request wants KIND:OP[:ARG ...]@S, got %r' % text)
    kind, op = parse_int(parts[0]), parse_int(parts[1])
    args = [parse_int(p) for p in parts[2:]]
    fields = {'kind': kind, 'operation': op}
    if kind == 2 and op == 1 and len(args) == 1:      # target temperature, i16 LE in ARG0..1
        if not -0x8000 <= args[0] <= 0x7FFF:
            raise UsageError('--request %r: the temperature (centi-degC) does not fit i16' % text)
        raw = args[0] & 0xFFFF
        args = [raw & 0xFF, raw >> 8]
    if len(args) > 6 or any(not 0 <= a <= 0xFF for a in args):
        raise UsageError('--request %r: at most 6 byte arguments' % text)
    for i in range(6):
        fields['args[%d]' % i] = args[i] if i < len(args) else 0
    return fields


def parse_ack(text):
    parts = text.split(':')
    if len(parts) not in (2, 3):
        raise UsageError('--ack wants TYPE:OCCURRENCE[:DOMAIN_BOOT]@S, got %r' % text)
    out = {'warning_type': parse_int(parts[0]), 'occurrence_id': parse_int(parts[1])}
    if len(parts) == 3:
        out['warning_domain_boot'] = parse_int(parts[2])
    return out


def parse_damage(text):
    if text not in BAD_KINDS:
        raise UsageError('unknown damage %r (one of %s)' % (text, ', '.join(BAD_KINDS)))
    return text


def cmd_run(args, out):
    settings = field_settings(args.fields, list(UPLINK_TYPES))
    every = parse_int(args.context_every) / 1000.0
    try:
        duration = float(args.duration)
    except ValueError:
        raise UsageError('--duration %r is not a number' % args.duration)
    if duration <= 0 or every < 0:
        raise UsageError('--duration must be > 0 and --context-every >= 0')
    n = Numbers(args)
    # The records once before the port opens: a refused value is a usage error, nothing is sent.
    check_n = Numbers(args)
    context_values(check_n, settings[TYPE_CONTEXT], True, None, args.allow_invalid)
    events = []
    for at in args.session_change_at or ():
        try:
            events.append((float(at), 'session', None))
        except ValueError:
            raise UsageError('--session-change-at %r is not a number of seconds' % at)
    events += [(t, 'request', f) for t, f in parse_events(args.request, '--request', parse_request)]
    events += [(t, 'query', parse_int(q)) for t, q in parse_events(args.query, '--query', lambda v: v)]
    events += [(t, 'ack', f) for t, f in parse_events(args.ack, '--ack', parse_ack)]
    events += [(t, 'inject', k) for t, k in parse_events(args.inject, '--inject', parse_damage)]
    for _, kind, value in events:
        if kind == 'request':
            built(TYPE_REQUEST, check_n.identity(TYPE_REQUEST), dict(settings[TYPE_REQUEST], **value),
                  args.allow_invalid)
        elif kind == 'query':
            q = dict(settings[TYPE_QUERY], query_scope=value)
            built(TYPE_QUERY, check_n.identity(TYPE_QUERY), q, args.allow_invalid)
        elif kind == 'ack':
            a = dict(settings[TYPE_ACK], **value)
            if 'warning_domain_boot' not in value:
                a['warning_domain_boot'] = 1   # the DOMAIN_BOOT_ID received by then (never 0 when sent)
            built(TYPE_ACK, check_n.identity(TYPE_ACK), a, args.allow_invalid)
    events.sort(key=lambda e: e[0])
    requests, queries = [], []
    skipped = 0
    s = Session(args.port, args.baud, args.log, out, args.verbose, n.prior)
    s.say('run: device=%d session=0x%X link from %d, M_CONTEXT every %s ms (%s), %s s, %d event(s)' % (
        n.device, n.session, n.link, args.context_every, args.context_mode, args.duration, len(events)))
    next_context = 0.0 if every > 0 else None
    observed_at = None         # time of the current context observation (repeat mode)
    repeats = 0                # repeats of it sent so far
    age_step = min(max(every * 100.0, REPEAT_AGE_STEP_MS[0]), REPEAT_AGE_STEP_MS[1])   # period / 10, in ms
    try:
        while True:
            t = s.now()
            if t >= duration:
                break
            if events and t >= events[0][0]:
                _, kind, value = events.pop(0)
                if kind == 'session':
                    n.session = (n.session + 1) & 0xFFFFFFFFFFFFFFFF
                    observed_at = None
                    s.say('session -> 0x%X (next M_CONTEXT carries it)' % n.session)
                    s._log({'event': 'session_change', 'session': n.session})
                    if next_context is not None:
                        next_context = t
                    continue
                if kind == 'inject':
                    values, payload = context_values(n, settings[TYPE_CONTEXT], False, None, True)
                    if s.send(damage(value, TYPE_CONTEXT, n.link, payload), 'damaged_' + value, values, n.link):
                        s.tx_damaged[value] = s.tx_damaged.get(value, 0) + 1
                        s.say('tx damaged_%s (LINK_SEQUENCE %d not used)' % (value, n.link))
                    quiet = s.now() + QUIET_AFTER_DAMAGE_S
                    next_context = max(next_context, quiet) if next_context is not None else None
                    if events and events[0][0] < quiet:
                        events[0] = (quiet,) + events[0][1:]
                    continue
                if kind == 'request':
                    type_, fields = TYPE_REQUEST, dict(settings[TYPE_REQUEST], **value)
                elif kind == 'query':
                    type_, fields = TYPE_QUERY, dict(settings[TYPE_QUERY], query_scope=value)
                else:
                    fields = dict(settings[TYPE_ACK], **value)
                    if 'warning_domain_boot' not in value:
                        with s.lock:
                            fields['warning_domain_boot'] = s.checker.last_domain_boot()
                    type_ = TYPE_ACK
                    if not fields['warning_domain_boot'] and not args.allow_invalid:
                        skipped += 1
                        s.say('ack skipped: no DOMAIN_BOOT_ID seen yet (give TYPE:OCCURRENCE:BOOT)')
                        s._log({'event': 'ack_skipped'})
                        continue
                try:
                    values, payload = built(type_, n.identity(type_), fields, args.allow_invalid)
                except UsageError as e:    # checked before the port opened; a value from DOMAIN is left
                    skipped += 1
                    s.say('%s skipped: %s' % (kind, e))
                    s._log({'event': kind + '_skipped', 'error': str(e)})
                    continue
                link = n.link
                if s.send(frame_bytes(type_, link, payload), NAMES[type_], values, link):
                    n.next_link()
                    s.sent(type_, values, link)
                    if type_ == TYPE_REQUEST:
                        requests.append((t, values['request_id'], values['device_context_id'], values['session_id']))
                    elif type_ == TYPE_QUERY:
                        queries.append((t, values['query_id'], values['device_context_id'], values['session_id']))
                continue
            if next_context is not None and t >= next_context:
                new = (args.context_mode == 'new' or observed_at is None or
                       (t - observed_at) * 1000.0 >= CONTEXT_REFRESH_MS)
                if new:
                    observed_at, repeats = t, 0
                else:
                    repeats += 1
                age = None if new else int((t - observed_at) * 1000.0 + age_step * repeats) + 1   # rounded up
                values, payload = context_values(n, settings[TYPE_CONTEXT], new, age, args.allow_invalid)
                link = n.link
                if s.send(frame_bytes(TYPE_CONTEXT, link, payload), NAMES[TYPE_CONTEXT], values, link):
                    n.next_link()
                    s.sent(TYPE_CONTEXT, values, link)
                next_context += every
                if next_context <= t:      # fell a whole period behind: no catch-up burst
                    next_context = t + every
                continue
            waits = [x for x in (next_context, events[0][0] if events else None, duration) if x is not None]
            time.sleep(max(0.0, min(0.005, min(waits) - t)))
    except KeyboardInterrupt:
        s.say('stopped (Ctrl+C)')
    finally:
        s.close()
        n.save()
    summary, verdict = s.finish(dict(n.as_dict(), events_skipped=skipped), requests, queries)
    out.write('SUMMARY %s\n' % json.dumps(summary, sort_keys=True))
    out.write('VERDICT %s\n' % json.dumps(verdict, sort_keys=True))
    return 0


# ------------------------------------------------------------------- main ---

def build_parser():
    p = argparse.ArgumentParser(prog='esp32_bench.py', description=__doc__.split('\n', 1)[0])
    sub = p.add_subparsers(dest='command')

    f = sub.add_parser('frame', help='print one frame (no serial port)')
    f.add_argument('message', choices=sorted(WORDS))
    f.add_argument('--device', help='DEVICE_CONTEXT_ID (default %d)' % DEFAULT_DEVICE)
    f.add_argument('--session', help='SESSION_ID (default %d)' % DEFAULT_SESSION)
    f.add_argument('--id', default='1', help='update sequences (context), REQUEST_ID, QUERY_ID (default 1)')
    f.add_argument('--link', default='1', help='LINK_SEQUENCE (default 1)')
    f.add_argument('--bad', choices=BAD_KINDS)
    f.add_argument('--allow-invalid', action='store_true')
    f.add_argument('fields', nargs='*')

    d = sub.add_parser('decode', help='decode frames given as hex (no serial port)')
    d.add_argument('hex', nargs='+')

    sub.add_parser('ports', help='list serial ports (pyserial)')

    for name, helptext in (('send', 'send one frame, then listen'),
                           ('run', 'play the ESP32 for a while, then judge what DOMAIN sent')):
        c = sub.add_parser(name, help=helptext)
        c.add_argument('--port', required=True, help='serial port of the DOMAIN UART-M (e.g. COM7)')
        c.add_argument('--baud', type=int, default=115200)
        c.add_argument('--state', help='JSON file with the numbers to continue (read, then written)')
        c.add_argument('--new-session', action='store_true', help='SESSION_ID + 1, the other numbers continue')
        c.add_argument('--device', help='DEVICE_CONTEXT_ID (default: state, else %d)' % DEFAULT_DEVICE)
        c.add_argument('--session', help='SESSION_ID (default: state, else %d)' % DEFAULT_SESSION)
        c.add_argument('--link', help='LINK_SEQUENCE of the first frame (default: state, else 1)')
        c.add_argument('--log', help='JSON lines of every sent / received frame (appended)')
        c.add_argument('--allow-invalid', action='store_true')
        c.add_argument('--verbose', action='store_true', help='also print every M_CONTEXT and M_DOMAIN_ALIVE')
        if name == 'send':
            c.add_argument('message', choices=sorted(WORDS))
            c.add_argument('--bad', choices=BAD_KINDS)
            c.add_argument('--listen', default='1.0', help='seconds to read after sending (default 1)')
        else:
            c.add_argument('--duration', required=True, help='seconds')
            c.add_argument('--context-every', default='100', help='ms, 0 = none (default 100)')
            c.add_argument('--context-mode', choices=('new', 'repeat'), default='new',
                           help='new: new update sequences each frame; repeat: one observation, AGE grows, '
                                'a new one every %d ms' % CONTEXT_REFRESH_MS)
            c.add_argument('--session-change-at', action='append', metavar='S',
                           help='SESSION_ID + 1 at that time')
            c.add_argument('--request', action='append', metavar='KIND:OP[:ARG ...]@S',
                           help='an M_REQUEST at that time (1:0 door lock, 1:1 unlock, 2:1:-150 temperature, ...)')
            c.add_argument('--query', action='append', metavar='SCOPE@S', help='an M_QUERY (scope 0..3) at that time')
            c.add_argument('--ack', action='append', metavar='TYPE:OCCURRENCE[:BOOT]@S',
                           help='an M_WARNING_ACK at that time (BOOT: last DOMAIN_BOOT_ID seen)')
            c.add_argument('--inject', action='append', metavar='KIND@S', help='a damaged M_CONTEXT at that time')
        c.add_argument('fields', nargs='*')
    return p


def main(argv=None, out=None):
    out = out or sys.stdout
    parser = build_parser()
    # field=value words may stand before, between or after the options (see pi_bench.py).
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
