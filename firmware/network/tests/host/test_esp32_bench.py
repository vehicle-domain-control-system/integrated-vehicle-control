#!/usr/bin/env python3
"""Checks of tools/esp32_bench/esp32_bench.py, the ESP32 stand-in of the DOMAIN board test.

Its UART-M bytes are compared with the repository codec through the vss_bench
executable (uartm encode / decode), both ways and for every UART-M message: the
frames vss_bench builds with the codec decode to the same fields in the tool,
the tool builds the same bytes from those fields, the codec decodes every frame
the tool sends to the same fields, and the codec gives the same verdict on
refused values. The damaged frames, the receive stream, the checks of received
frames, the offline commands and whole serial sessions on a stand-in port (no
hardware, no pyserial) are checked too.

    test_esp32_bench.py CASE --vss-bench PATH

Exit 0 = pass, 1 = fail, 2 = usage.
"""
from __future__ import annotations

import sys

sys.dont_write_bytecode = True

import argparse
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

NETWORK = Path(__file__).resolve().parents[2]
TOOL = NETWORK / "tools" / "esp32_bench" / "esp32_bench.py"

spec = importlib.util.spec_from_file_location("esp32_bench_under_test", TOOL)
eb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(eb)

VSS_BENCH = None
FAILS = []
BY_NAME = {name: t for t, name in eb.NAMES.items()}
DOMAIN_BOOT = 0x444F4D31


def fail(text):
    FAILS.append(text)
    print("FAIL: " + text)


def expect(cond, text):
    if not cond:
        fail(text)
    return cond


def vss_bench(*args):
    r = subprocess.run([str(VSS_BENCH), *map(str, args)], capture_output=True, text=True, timeout=30)
    return r.returncode, r.stdout + r.stderr


def fields_of(text):
    """'fields: a=1 items[0].reason=17 t=-250 ...' -> {name: int}."""
    line = next((l for l in text.splitlines() if l.startswith("fields:")), "")
    return {name: int(value) for name, value in re.findall(r"([\w\[\]\.]+)=(-?\d+)", line)}


def frame_of(text):
    line = next((l for l in text.splitlines() if l.startswith("frame:")), "")
    return bytes(int(t, 16) for t in line[len("frame:"):].split())


def codec_verdict(text):
    m = re.search(r"^codec: (\w+)", text, re.M)
    return m.group(1) if m else None


def c_encode(name, values, link=1):
    return vss_bench("uartm", "encode", name, "--link", link, *["%s=%d" % kv for kv in values.items()])


def full(type_, values):
    out = {name: 0 for name, _ in eb.FIELDS[type_]}
    out.update(values)
    return out


# One valid record of every UART-M message (the codec accepts it), with
# distinct values in the wide fields so that a shifted or swapped field shows.
D = dict(device_context_id=0x01020304, session_id=0x1112131415161718, domain_boot_id=0x21222324, query_id=0x31323334)
Q = dict(D, original_can_boot_id=0x41424344, original_can_tx_sequence=0x51525354)
P = dict(Q, update_sequence=0x61626364, source_age_ms=0x0717)
VALID = {
 'M_CONTEXT': dict(device_context_id=0x01020304, session_id=0x1112131415161718, device_registration_state=1,
      bt_connection_state=0, app_active_state=255, app_quality=2, app_update_sequence=0x71727374,
      app_age_ms=0x0102, proximity_state=1, proximity_validity=1, proximity_reason=0,
      proximity_update_sequence=0x81828384, proximity_age_ms=0x0304, registration_connection_quality=1,
      registration_connection_update_sequence=0x91929394, registration_connection_age_ms=0x0506),
 'M_REQUEST': {'device_context_id': 0x01020304, 'session_id': 0x1112131415161718, 'request_id': 0x41424344,
      'kind': 3, 'operation': 3, 'args[0]': 0x11, 'args[1]': 0x22, 'args[2]': 0xff},
 'M_QUERY': dict(device_context_id=0x01020304, session_id=0x1112131415161718, query_id=0x31323334, query_scope=1,
      request_session=0x2122232425262728, request_id=0x41424344),
 'M_WARNING_ACK': dict(device_context_id=0x01020304, session_id=0x1112131415161718, warning_type=8,
      occurrence_id=0x51525354, warning_domain_boot=0x21222324),
 'M_DOMAIN_ALIVE': dict(domain_boot_id=0x21222324, update_sequence=0x61626364, source_age_ms=0x0717,
      domain_state=2, value_quality=1),
 'M_RESULT': dict(D, request_session=0x2122232425262728, request_id=0x41424344, result=2, result_reason=25,
      result_confirmation=1, command_target=1, command_id=0x51525354, origin=5, result_age_ms=0x0809),
 'M_WARNING': dict(D, warning_type=7, occurrence_id=0x51525354, severity=2, warning_state=1, read_state=1,
      value_quality=2, quality_reason=9, source_boot=0x41424344, source_sequence=0x61626364, source_age_ms=0x0717),
 'M_AVAILABILITY': dict(D, update_sequence=0x61626364, source_age_ms=0x0717,
      **{'items[%d].function_id' % i: i + 1 for i in range(9)},
      **{'items[%d].availability' % i: i % 3 for i in range(9)},
      **{'items[%d].reason' % i: 17 + i for i in range(9)}),
 'M_DIGITAL_STATUS': dict(D, digital_key_setting_state=1, digital_key_availability=2, reason=25,
      update_sequence=0x61626364, source_age_ms=0x0717),
 'M_DIGITAL_RESULT': dict(D, auto_occurrence_id=0x51525354, related_door_command_id=0x71727374, origin=2, result=3,
      result_reason=24, result_confirmation=1, result_age_ms=0x0809),
 'M_USER_SETTINGS': dict(D, target_temperature_centi_c=-250, climate_auto=1, user_fan_level=3,
      interior_light_enabled=1, normal_light_level_percent=55, **{'normal_light_rgb[0]': 0x11,
      'normal_light_rgb[1]': 0x22, 'normal_light_rgb[2]': 0xfe}, value_quality=0, update_sequence=0x61626364,
      source_age_ms=0x0717),
 'M_QUERY_END': dict(D, query_scope=3, query_status=2, item_count=0x0102),
 'M_BCM_DOOR_STATE': dict(P, door_lock_state=1, door_open_state=255, door_composite_state=2, lock_quality=1,
      open_quality=2, composite_quality=3),
 'M_BCM_CLIMATE_STATE': dict(P, fan_command_level=2, fan_measured_level=3, thermal_direction_state=1,
      thermal_output_percent=40, heat_removal_state=1, fan_command_quality=0, fan_measured_quality=1,
      thermal_direction_quality=2, thermal_output_quality=3, heat_removal_quality=0),
 'M_BCM_LIGHT_STATE': dict(P, interior_light_type_state=1, light_level_percent=70, color_r=1, color_g=2, color_b=3,
      interior_light_apply_result=1, light_physical_feedback_capability=1, light_type_quality=0,
      light_level_quality=1, light_color_quality=2, light_apply_quality=3),
 'M_BCM_STATUS': dict(P, bcm_ecu_state=2, fault_mask=0x00f1, active_fault_category=3, active_fault_code=8,
      affected_function=5, recovering=1),
 'M_CIS_ENVIRONMENT': dict(Q, temperature_centi_c=-1234, temperature_sequence=0x71727374,
      temperature_age_ms=0x0102, temperature_validity=1, temperature_reason=0, humidity_centi_percent=4500,
      humidity_sequence=0x81828384, humidity_age_ms=0x0304, humidity_validity=0, humidity_reason=5,
      illuminance_lux=0x00012345, illuminance_sequence=0x91929394, illuminance_age_ms=0x0506,
      illuminance_validity=1, illuminance_reason=0),
 'M_CIS_OCCUPANT': dict(Q, pi_boot_id=0x71727374, vision_sequence=0x81828384, vision_age_ms=0x0102,
      occupant_presence=1, presence_validity=1, presence_reason=0, occupant_count=2, count_validity=1,
      count_reason=0, vision_function_status=2),
 'M_CIS_REAR': dict(P, rear_distance_deci_cm=0x0405, validity=0, quality_reason=3, proximity_status=3),
 'M_CIS_STATUS': dict(P, cis_state=3, vision_status=4, temperature_status=1, humidity_status=2,
      illuminance_status=3, rear_status=0, pi_uart_status=2, can_interface_status=1, fault_mask=0x01ff,
      active_fault=9, recovering=1, last_fault=8, last_fault_occurrence=0x71727374, affected_function=0x3f),
 'M_VSS_STATUS': dict(P, vss_state=2, vss_availability=1, vss_accepting_events=1, vss_fault_active=1,
      fault_mask=0x0f, vss_last_fault=0x0003, last_fault_occurrence=0x71727374, last_fault_age_ms=0x0102,
      last_fault_quality=0),
 'M_WINDOW_STATE': dict(P, motion_state=1, window_ecu_state=2, window_position=50, window_value_quality=1,
      fully_open_state=0, fully_closed_state=0, antipinch_protection_status=1, reverse_state=0),
 'M_WINDOW_FAULT': dict(P, fault_category=3, fault_code=255, fault_status=2, occurrence_id=0x71727374),
}


def case_esp32_crc():
    expect(eb.crc16(b"123456789") == 0x29B1, "CRC16-CCITT-FALSE check value 0x29b1")
    rc, out = c_encode("M_DOMAIN_ALIVE", VALID["M_DOMAIN_ALIVE"])
    data = frame_of(out)
    expect(rc == 0 and eb.from_le(data[-2:]) == eb.crc16(data[2:-2]), "tool CRC of a codec frame: %s" % out)


def case_esp32_tables_vs_codec():
    """Every message: codec frame -> tool fields, tool bytes == codec bytes."""
    expect(sorted(VALID) == sorted(eb.NAMES.values()), "a valid record for every UART-M message")
    for name, values in sorted(VALID.items()):
        type_ = BY_NAME[name]
        rc, out = c_encode(name, values, link=0x0203)
        if not expect(rc == 0 and codec_verdict(out) == "OK", "%s: codec refused the valid record: %s" % (name, out)):
            continue
        data = frame_of(out)
        c_fields = fields_of(out)
        expect(len(data) == eb.payload_length(type_) + eb.OVERHEAD, "%s: frame length %d" % (name, len(data)))
        events = eb.Stream(None, assembly_limit_ms=1e9).feed(data, 0.0)
        if not expect(len(events) == 1 and events[0][0] == "frame", "%s: tool stream %r" % (name, events)):
            continue
        _, t, link, values_tool, _, _ = events[0]
        expect(t == type_ and link == 0x0203, "%s: type 0x%02x link %d" % (name, t, link))
        expect(values_tool == c_fields == full(type_, values),
               "%s: fields differ\n tool  %r\n codec %r" % (name, values_tool, c_fields))
        mine = eb.frame_bytes(type_, 0x0203, eb.encode_payload(type_, full(type_, values)))
        expect(mine == data, "%s: tool bytes differ\n tool  %s\n codec %s" % (name, eb.hex_text(mine),
                                                                         eb.hex_text(data)))
        expect(eb.direction(type_) == ("uplink" if type_ in eb.UPLINK_TYPES else "downlink"), name)
    # Values do not show a swap of two fields that hold the same value; the offsets the codec
    # encoders write each member at do.
    offsets = encoder_offsets()
    expect(sorted(offsets) == sorted(eb.FIELDS), "an encoder for every UART-M message: %r" %
           sorted("0x%02x" % t for t in offsets))
    for type_, (size, items) in sorted(offsets.items()):
        mine, at = [], 0
        for fname, width in eb.FIELDS.get(type_, ()):
            mine.append((at, width, fname))
            at += abs(width)
        expect(items == mine and size == at, "%s: encoder offsets differ from FIELDS\n codec %r\n tool  %r" % (
            eb.NAMES.get(type_), [x for x in items if x not in mine], [x for x in mine if x not in items]))
    # The AGE identities of the received-frame check are those of the repository receiver.
    guards = {}
    keys = c_age_keys(guards)
    expect(guards == eb.AGE_GUARDS, "AGE_GUARDS %r, net_upper_uart_original_age %r" % (eb.AGE_GUARDS, guards))
    for type_ in sorted(t for t in eb.FIELDS if t not in eb.UPLINK_TYPES):
        ages = [f for f, _ in eb.FIELDS[type_] if f.endswith("_age_ms")]
        expect(eb.AGE_KEYS.get(type_, {}) == keys.get(type_, {}) and sorted(keys.get(type_, {})) == sorted(ages),
               "%s: AGE_KEYS %r, net_upper_uart_original_age %r, AGE fields %r" % (
                   eb.NAMES[type_], eb.AGE_KEYS.get(type_), keys.get(type_), ages))


CODEC_DIR = NETWORK / "src" / "l04_frame_codec" / "m06_uart_codec"
SOURCE_C = NETWORK / "src" / "l01_node_context" / "m01_source_context" / "net_upper_source.c"
WRITE_WIDTH = {"u16": 2, "i16": -2, "u32": 4, "u64": 8}


def encoder_offsets():
    """{type: (payload size, [(offset, width, member)])} read from the UART-M encoders of the codec."""
    src = "".join((CODEC_DIR / f).read_text(encoding="utf-8") for f in (
        "net_wire_uart_dto.c", "net_wire_uart_m_core.c", "net_wire_uart_m_projection.c"))
    out = {}
    for body in re.findall(r"NetWireStatus net_wire_m_\w+_encode\(.*?\n\{(.*?)\n\}", src, re.S):
        mt = re.search(r"NET_WIRE_UART_M, NET_WIRE_(?:UP|DOWN)LINK,\s*0x([0-9A-Fa-f]+)[uU]", body)
        size = re.search(r"uint8_t p\[(\d+)\]", body)
        if not mt or not size:
            continue
        items = [(int(o), WRITE_WIDTH[w], m) for w, o, m in
                 re.findall(r"nw_write_(u16|u32|u64|i16)\(p \+ (\d+)[uU]?, m->([\w\.\[\]]+)\)", body)]
        items += [(0, WRITE_WIDTH[w], m) for w, m in re.findall(r"nw_write_(u16|u32|u64|i16)\(p, m->([\w\.\[\]]+)\)",
                                                               body)]
        items += [(int(o), 1, m) for o, m in re.findall(r"p\[(\d+)\] = m->([\w\.\[\]]+);", body)]
        cp = re.search(r"memcpy\(p \+ (\d+)[uU]?, m->args, sizeof\(m->args\)\)", body)
        if cp:   # one byte per argument up to the next member written (or the payload end)
            at = int(cp.group(1))
            end = min([o for o, _, _ in items if o > at] + [int(size.group(1))])
            items += [(at + i, 1, "args[%d]" % i) for i in range(end - at)]
        out[int(mt.group(1), 16)] = (int(size.group(1)), sorted(items))
    return out


def c_age_keys(guards=None):
    """{type: {AGE field: identity fields}} of the UART-M cases of net_upper_uart_original_age; guards
    (a dict, if given) gets {(type, AGE field): ((field, value), ...)} of the conditions on both frames."""
    src = SOURCE_C.read_text(encoding="utf-8")
    start = src.index("NetStatus net_upper_uart_original_age(")
    body = src[start:src.index("\n}\n", start)]
    parts = re.split(r"\n    case (0x[0-9A-Fa-f]+)u:", body)
    out = {}
    for i in range(1, len(parts), 2):
        type_ = int(parts[i], 16)
        for cond, age in re.findall(r"if \((.*?)younger\(old->data\.\w+\.([\w\[\]\.]+),m->data", parts[i + 1]):
            out.setdefault(type_, {})[age] = tuple(re.findall(r"old->data\.\w+\.([\w\[\]\.]+)==m->data", cond))
            both = tuple((f, int(v)) for f, v, f2, v2 in re.findall(
                r"old->data\.\w+\.([\w\[\]\.]+)==(\d+)u && m->data\.\w+\.([\w\[\]\.]+)==(\d+)u", cond)
                if f == f2 and v == v2)
            if guards is not None and both and type_ in eb.FIELDS:
                guards[(type_, age)] = both
    return {t: v for t, v in out.items() if t in eb.FIELDS}


def tool_frame(word, args):
    rc = io.StringIO()
    code = eb.main(["frame", word] + args, rc)
    lines = rc.getvalue().strip().splitlines()
    return code, lines, bytes(int(t, 16) for t in lines[-1].split()) if code == 0 else b""


def case_esp32_codec_decodes_tool():
    """Every frame the tool builds decodes in the codec to the fields the tool printed."""
    sets = [("context", []), ("context", ["--device", "9", "--session", "0x0102030405060708", "--id", "77",
                                          "app_quality=3", "app_active_state=255", "proximity_state=255",
                                          "proximity_validity=0", "proximity_reason=6"]),
            ("request", []), ("request", ["kind=1", "operation=1"]), ("request", ["kind=2", "operation=1",
                                                                                 "args[0]=0x6a", "args[1]=0xff"]),
            ("request", ["kind=2", "operation=3", "args[0]=3"]), ("request", ["kind=3", "operation=3",
                                                                            "args[0]=255", "args[1]=1",
                                                                            "args[2]=2"]),
            ("request", ["kind=4", "operation=1", "args[0]=1", "--id", "0xfffffffe"]),
            ("query", []), ("query", ["query_scope=1", "request_session=5", "request_id=6"]),
            ("query", ["query_scope=3", "--id", "9"]),
            ("ack", ["warning_domain_boot=0x444F4D31"]), ("ack", ["warning_type=8", "occurrence_id=1234",
                                                                  "warning_domain_boot=1"])]
    for word, args in sets:
        code, lines, data = tool_frame(word, args + ["--link", "65535"])
        if not expect(code == 0, "frame %s %s: %s" % (word, args, lines)):
            continue
        rc, out = vss_bench("uartm", "decode", eb.hex_text(data))
        tool_fields = {k: int(v, 0) for k, v in re.findall(r"([\w\[\]\.]+)=(0x[0-9A-Fa-f]+|-?\d+)", lines[1])}
        expect(rc == 0 and "codec: OK" in out and "link=65535" in out and fields_of(out) == tool_fields,
               "codec decode of tool %s %s:\n%s\n%s" % (word, args, lines[1], out))


# Values the codec refuses, by message: (fields, codec verdict).
REFUSED = {
    "M_CONTEXT": [({"device_context_id": 0}, "MALFORMED"), ({"device_registration_state": 2}, "MALFORMED"),
                  ({"bt_connection_state": 3}, "MALFORMED"), ({"app_quality": 4}, "MALFORMED"),
                  ({"registration_connection_quality": 4}, "MALFORMED"), ({"proximity_validity": 2}, "MALFORMED"),
                  ({"proximity_reason": 10}, "MALFORMED"), ({"proximity_state": 255}, "CONTRACT_HOLD"),
                  ({"proximity_reason": 3}, "CONTRACT_HOLD"), ({"app_active_state": 255}, "CONTRACT_HOLD"),
                  ({"bt_connection_state": 255}, "CONTRACT_HOLD"),
                  ({"device_registration_state": 255}, "CONTRACT_HOLD"),
                  ({"device_registration_state": 255, "registration_connection_quality": 1}, "OK"),
                  ({"proximity_state": 255, "proximity_validity": 0, "proximity_reason": 6}, "OK")],
    "M_REQUEST": [({"device_context_id": 0}, "MALFORMED"), ({"kind": 0}, "MALFORMED"),
                  ({"kind": 5, "operation": 1}, "MALFORMED"), ({"operation": 2}, "MALFORMED"),
                  ({"args[0]": 1}, "MALFORMED"), ({"kind": 2, "operation": 2, "args[0]": 2}, "MALFORMED"),
                  ({"kind": 2, "operation": 1, "args[2]": 1}, "MALFORMED"),
                  ({"kind": 2, "operation": 4}, "MALFORMED"), ({"kind": 3, "operation": 2, "args[0]": 101},
                                                               "MALFORMED"),
                  ({"kind": 3, "operation": 3, "args[3]": 1}, "MALFORMED"),
                  ({"kind": 4, "operation": 1, "args[0]": 2}, "MALFORMED"), ({"kind": 4, "operation": 2},
                                                                            "MALFORMED"),
                  ({"kind": 2, "operation": 3, "args[0]": 3}, "OK"), ({"kind": 3, "operation": 2, "args[0]": 100},
                                                                       "OK")],
    "M_QUERY": [({"query_id": 0}, "MALFORMED"), ({"query_scope": 4}, "MALFORMED"), ({"reserved": 1}, "MALFORMED"),
                ({"cursor": 1}, "MALFORMED"), ({"request_id": 3}, "MALFORMED"),
                ({"query_scope": 1, "request_session": 3}, "CONTRACT_HOLD"),
                ({"query_scope": 2, "request_id": 3}, "CONTRACT_HOLD"), ({"query_scope": 3}, "OK")],
    "M_WARNING_ACK": [({"device_context_id": 0}, "MALFORMED"), ({"warning_type": 0}, "MALFORMED"),
                      ({"warning_type": 9}, "MALFORMED"), ({"warning_domain_boot": 0}, "CONTRACT_HOLD")],
    "M_DOMAIN_ALIVE": [({"domain_boot_id": 0}, "MALFORMED"), ({"domain_state": 4}, "MALFORMED"),
                       ({"value_quality": 4}, "MALFORMED"), ({"domain_state": 0, "value_quality": 3}, "OK")],
}


def case_esp32_check_vs_codec():
    for name, sets in sorted(REFUSED.items()):
        type_ = BY_NAME[name]
        base = {"M_REQUEST": {"device_context_id": 1, "session_id": 2, "request_id": 3, "kind": 1, "operation": 0},
                "M_QUERY": {"device_context_id": 1, "session_id": 2, "query_id": 3, "query_scope": 0}}.get(
            name, VALID[name])
        for change, wanted in sets:
            values = full(type_, dict(base, **change))
            if name == "M_CONTEXT":   # from the tool's documented defaults
                values = eb.record(type_, {"device_context_id": 7, "session_id": 1, "app_update_sequence": 1,
                                           "proximity_update_sequence": 1,
                                           "registration_connection_update_sequence": 1}, {})
                values.update(change)
            rc, out = c_encode(name, values)
            mine = eb.check(type_, values)
            expect(codec_verdict(out) == wanted and mine == wanted,
                   "%s %r: codec %s, tool %s, wanted %s" % (name, change, codec_verdict(out), mine, wanted))


def case_esp32_damaged_frames():
    values, payload = eb.built(eb.TYPE_CONTEXT, {"device_context_id": 7, "session_id": 1, "app_update_sequence": 1,
                                                 "proximity_update_sequence": 1,
                                                 "registration_connection_update_sequence": 1}, {}, False)
    wanted = {"crc": "bad_crc", "len": "bad_header", "version": "bad_header", "type": "bad_header",
              "noise": "noise"}
    for kind in eb.BAD_KINDS:
        data = eb.damage(kind, eb.TYPE_CONTEXT, 5, payload)
        if kind == "trunc":
            s = eb.Stream(eb.UPLINK)
            first = s.feed(data, 0.0)
            later = s.poll(0.051)
            expect(len(data) == 12 and first == [] and later and later[0][0] == "timeout",
                   "trunc: %r then %r" % (first, later))
            continue
        events = eb.Stream(eb.UPLINK, assembly_limit_ms=1e9).feed(data, 0.0)
        expect(events and events[0][0] == wanted[kind] and not any(e[0] == "frame" for e in events),
               "%s: %r" % (kind, events))
        if kind in ("crc", "len", "version", "type"):
            rc, out = vss_bench("uartm", "decode", eb.hex_text(data))
            expect(rc != 0, "codec must refuse damaged %s: %s" % (kind, out))
    good = eb.frame_bytes(eb.TYPE_CONTEXT, 5, payload)
    events = eb.Stream(eb.UPLINK, assembly_limit_ms=1e9).feed(eb.damage("crc", eb.TYPE_CONTEXT, 5, payload) + good,
                                                               0.0)
    expect([e for e in events if e[0] == "frame"] and events[-1][2] == 5, "resync after a damaged frame: %r" % events)


def downlink(name, link, **change):
    type_ = BY_NAME[name]
    values = full(type_, dict(VALID[name], **change))
    return eb.frame_bytes(type_, link, eb.encode_payload(type_, values))


def case_esp32_stream():
    s = eb.Stream(eb.DOWNLINK)
    a, b = downlink("M_DOMAIN_ALIVE", 1), downlink("M_RESULT", 2)
    ev = s.feed(a[:7], 0.0) + s.feed(a[7:] + b"\x00\x13" + b[:20], 0.030) + s.feed(b[20:], 0.060)
    kinds = [e[0] for e in ev]
    expect(kinds == ["frame", "noise", "frame"] and ev[0][1] == eb.TYPE_ALIVE and ev[2][1] == eb.TYPE_RESULT,
           "split frames and noise: %r" % kinds)
    ev = eb.Stream(eb.DOWNLINK).feed(eb.frame_bytes(eb.TYPE_CONTEXT, 1, bytes(38)), 0.0)
    expect(ev and ev[0][0] == "bad_header", "an uplink type in the downlink stream: %r" % ev)
    s = eb.Stream(eb.DOWNLINK)
    s.feed(a[:10], 0.0)
    ev = s.feed(a[10:], 0.0505)
    expect(ev and ev[0][0] == "timeout", "frame over the 50 ms assembly limit: %r" % ev)
    data = b"".join(downlink(name, i) for i, name in enumerate(sorted(VALID)) if BY_NAME[name] not in eb.UPLINK_TYPES)
    ev = eb.Stream(eb.DOWNLINK, assembly_limit_ms=1e9).feed(data, 0.0)
    expect(len(ev) == len(VALID) - 4 and all(e[0] == "frame" for e in ev), "every downlink type: %r" % [e[0] for e in ev])


def case_esp32_checker():
    c = eb.Checker()
    c.sent_context(1, 1)
    alive = lambda link, t, **k: c.frame(eb.TYPE_ALIVE, link, full(eb.TYPE_ALIVE, dict(VALID["M_DOMAIN_ALIVE"], **k)), t)
    alive(1, 0.0)
    alive(2, 0.25)
    alive(5, 0.5)                  # skip of 2 numbers
    alive(5, 0.75)                 # same number again
    alive(4, 1.0)                  # older
    expect(c.link == {"steps": 4, "skipped": 1, "skipped_numbers": 2, "backward": 2, "restarts": 0},
           "link: %r" % c.link)
    v = c.verdict({"bad_header": 0}, [], [], 1.2)
    expect(v["link_sequence"]["verdict"] == "FAIL" and v["alive"]["verdict"] == "PASS" and
           v["context_taken"]["verdict"] == "HOLD" and v["overall"] == "FAIL", "verdict: %r" % v)
    c = eb.Checker()
    c.sent_context(1, 1)
    c.sent_context(1, 2)
    rear = lambda link, t, **k: c.frame(BY_NAME["M_CIS_REAR"], link, full(BY_NAME["M_CIS_REAR"], dict(
        VALID["M_CIS_REAR"], device_context_id=1, **k)), t)
    rear(1, 0.0, session_id=2, source_age_ms=100)
    rear(2, 0.1, session_id=2, source_age_ms=200)
    rear(3, 0.2, session_id=1, source_age_ms=300)                 # earlier session of this run
    rear(4, 0.3, session_id=2, source_age_ms=250)                 # AGE back for the same observation
    rear(5, 0.4, session_id=2, source_age_ms=10, update_sequence=7)   # another observation: fine
    rear(6, 0.5, session_id=2, source_age_ms=0xFFFF, update_sequence=7)  # unknown AGE: not compared
    rear(7, 0.6, session_id=9)                                    # a context never sent, after one was taken
    expect(c.ctx == {"current": 5, "earlier_session": 1, "earlier_run": 0, "unknown_before_taken": 0,
                     "unknown": 1} and c.age["backward"] == 1 and c.age["unknown"] == 1,
           "context %r age %r" % (c.ctx, c.age))
    alive(8, 0.0)
    alive(9, 0.9, domain_boot_id=5)           # 900 ms gap, DOMAIN restarted
    v = c.verdict({}, [(0.1, 1, 1, 2)], [(0.2, 0x31323334, 1, 2)], 1.0)
    expect(v["alive"]["verdict"] == "FAIL" and any("restarted" in n for n in v["alive"]["notes"]) and
           v["context"]["verdict"] == "FAIL" and v["age"]["verdict"] == "FAIL" and
           v["context_taken"]["verdict"] == "TAKEN" and v["requests"][0]["verdict"] == "OPEN" and
           v["queries"][0]["verdict"] == "OPEN" and v["queries"][0]["items"] == 7 and
           v["link_sequence"]["restarts"] == 1 and v["link_sequence"]["backward"] == 0 and
           sorted(v["failed"]) == sorted(["alive", "context", "age"]), "verdict: %r" % v)
    # The DOMAIN board today: ALIVE only, INIT / NO_DATA / 0 / 65535.
    c = eb.Checker()
    c.sent_context(7, 1)
    for i in range(8):
        alive(1 + i, 0.25 * i, update_sequence=0, source_age_ms=0xFFFF, domain_state=0, value_quality=3)
    v = c.verdict({"bad_crc": 0}, [(0.3, 1, 7, 1)], [(0.5, 1, 7, 1)], 2.0)
    expect(v["overall"] == "PASS" and v["context_taken"]["verdict"] == "HOLD" and
           v["requests"][0]["verdict"] == "HOLD" and v["queries"][0]["verdict"] == "HOLD" and
           any("HOLD path" in n for n in v["alive"]["notes"]) and c.age["unknown"] == 8 and
           c.summary()["alive"]["values"] == [{"domain_state": 0, "value_quality": 3, "update_sequence": 0,
                                               "count": 8, "source_age_ms_min": 0xFFFF,
                                               "source_age_ms_max": 0xFFFF}], "board today: %r" % v)
    # M_DOMAIN_ALIVE also within 750 ms of the port open and of the end of reading.
    for times, end, field, wanted in (((0.8, 1.0), 1.2, "first_after_open_ms", "FAIL"),
                                      ((0.0, 0.25), 1.5, "last_before_end_ms", "FAIL"),
                                      ((0.1,), 0.5, "last_before_end_ms", "PASS")):
        c = eb.Checker()
        for i, at in enumerate(times):
            alive(1 + i, at)
        v = c.verdict({}, [], [], end)
        expect(v["alive"]["verdict"] == wanted and v["alive"][field] is not None,
               "ALIVE at %r, end %r: %r" % (times, end, v["alive"]))
    # Distinct ALIVE values are kept up to a limit, AGE as min / max.
    c = eb.Checker()
    for i in range(eb.ALIVE_VALUES_MAX + 4):
        alive(1 + i, 0.25 * i, update_sequence=i + 1, source_age_ms=i)
    s = c.summary()["alive"]
    expect(len(s["values"]) == eb.ALIVE_VALUES_MAX and s["frames_beyond_values"] == 4, "ALIVE values %r" % s)
    # LINK_SEQUENCE 65535 -> 0 is the next number; a new DOMAIN_BOOT_ID starts the count again.
    c = eb.Checker()
    alive(65534, 0.0)
    alive(65535, 0.25)
    alive(0, 0.5)
    alive(1, 0.75, domain_boot_id=0x21222325)
    expect(c.link == {"steps": 2, "skipped": 0, "skipped_numbers": 0, "backward": 0, "restarts": 1},
           "wrap / restart: %r" % c.link)
    # AGE identities are those of the repository receiver (AGE_KEYS).
    c = eb.Checker()
    warn = lambda link, **k: c.frame(BY_NAME["M_WARNING"], link, full(BY_NAME["M_WARNING"], dict(
        VALID["M_WARNING"], **k)), 0.1 * link)
    warn(1, source_boot=1, source_age_ms=500)
    warn(2, source_boot=2, source_age_ms=100)          # another source with the same SOURCE_SEQUENCE
    res = lambda link, **k: c.frame(eb.TYPE_RESULT, link, full(eb.TYPE_RESULT, dict(VALID["M_RESULT"], **k)),
                                    0.1 * link)
    res(3, command_id=1, result_age_ms=400)
    res(4, command_id=2, result_age_ms=50)             # another command of the same request
    env = lambda link, **k: c.frame(BY_NAME["M_CIS_ENVIRONMENT"], link, full(BY_NAME["M_CIS_ENVIRONMENT"], dict(
        VALID["M_CIS_ENVIRONMENT"], **k)), 0.1 * link)
    env(5, original_can_boot_id=1, temperature_age_ms=300)
    env(6, original_can_boot_id=2, temperature_age_ms=20)   # CIS restarted, same sequence
    expect(c.age["backward"] == 0, "different observations: %r" % c.age)
    warn(7, source_boot=2, source_age_ms=90)
    expect(c.age["backward"] == 1, "same warning observation, AGE back: %r" % c.age)
    # The VSS last-fault AGE is compared only while both frames carry its quality OK (AGE_GUARDS).
    c = eb.Checker()
    vss = lambda link, **k: c.frame(BY_NAME["M_VSS_STATUS"], link, full(BY_NAME["M_VSS_STATUS"], dict(
        VALID["M_VSS_STATUS"], **k)), 0.1 * link)
    vss(1, last_fault_quality=0, last_fault_age_ms=500)
    vss(2, last_fault_quality=3, last_fault_age_ms=0, update_sequence=1)
    expect(c.age["backward"] == 0, "last fault AGE with a quality that is not OK: %r" % c.age)
    vss(3, last_fault_quality=0, last_fault_age_ms=400, update_sequence=2)
    expect(c.age["backward"] == 1, "last fault AGE back with quality OK: %r" % c.age)
    # A frame with DOMAIN_BOOT_ID 0 does not reset the LINK_SEQUENCE count.
    c = eb.Checker()
    alive(5, 0.0)
    rear = lambda link, t, **k: c.frame(BY_NAME["M_CIS_REAR"], link, full(BY_NAME["M_CIS_REAR"], dict(
        VALID["M_CIS_REAR"], **k)), t)
    rear(5, 0.1, domain_boot_id=0)
    alive(5, 0.25)
    expect(c.link["backward"] == 2 and c.link["restarts"] == 0, "DOMAIN_BOOT_ID 0: %r" % c.link)
    # Contexts: the --state one, an unknown one before / after DOMAIN took one of this run, per session.
    c = eb.Checker(prior=[(7, 1)])
    ctx = lambda link, session: c.frame(BY_NAME["M_CIS_REAR"], link, full(BY_NAME["M_CIS_REAR"], dict(
        VALID["M_CIS_REAR"], device_context_id=7, session_id=session)), 0.1 * link)
    ctx(1, 1)                      # the earlier run's context, still held by DOMAIN
    ctx(2, 99)                     # unknown, nothing of this run taken yet
    c.sent_context(7, 2)
    ctx(3, 2)
    c.sent_context(7, 3)           # session change; DOMAIN never takes the new one
    ctx(4, 99)                     # unknown after a context of this run was taken
    v = c.verdict({}, [(0.25, 1, 7, 2), (0.35, 2, 7, 3)], [(0.36, 5, 7, 3)], 0.5)
    expect(c.ctx == {"current": 1, "earlier_session": 0, "earlier_run": 1, "unknown_before_taken": 1, "unknown": 1}
           and v["context"]["verdict"] == "FAIL" and
           v["context"]["note"] and v["context_taken"]["verdict"] == "HOLD" and
           [r["verdict"] for r in v["requests"]] == ["OPEN", "HOLD"] and v["queries"][0]["verdict"] == "HOLD",
           "contexts %r verdict %r" % (c.ctx, v))


def run_tool(*args):
    r = subprocess.run([sys.executable, "-B", str(TOOL), *map(str, args)], capture_output=True, text=True,
                       timeout=60)
    return r.returncode, r.stdout + r.stderr


def case_esp32_offline_cli():
    rc, out = run_tool("frame", "context", "--session", "0x1122", "app_age_ms=40")
    last = out.strip().splitlines()[-1] if out.strip() else ""
    expect(rc == 0 and re.fullmatch(r"([0-9a-f]{2} ){47}[0-9a-f]{2}", last) and "app_age_ms=40" in out,
           "frame context: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "request", "kind=9")
    expect(rc == 2 and "refuses" in out, "a refused value must be exit 2: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "request", "--allow-invalid", "kind=9")
    expect(rc == 0 and "codec=MALFORMED" in out, "--allow-invalid: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "ack")
    expect(rc == 2 and "CONTRACT_HOLD" in out, "ack without WARNING_DOMAIN_BOOT: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "query", "session_id=3")
    expect(rc == 2 and "set by the options" in out, "identity field by field=value: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "query", "no_such_field=1")
    expect(rc == 2 and "unknown field" in out, "unknown field: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "query", "--sek", "3")
    expect(rc == 2 and "unrecognized arguments: --sek" in out, "unknown option: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "context", "--bad", "crc")
    rc2, out2 = run_tool("decode", out.strip().splitlines()[-1])
    expect(rc == 0 and rc2 == 1 and "bad_crc" in out2, "decode of a damaged frame: rc=%d %s" % (rc2, out2))
    rc, out = vss_bench("uartm", "encode", "M_QUERY_END", *["%s=%d" % kv for kv in VALID["M_QUERY_END"].items()])
    rc2, out2 = run_tool("decode", "hex=" + frame_of(out).hex())
    expect(rc2 == 0 and "frame M_QUERY_END link=1 crc=OK codec=-" in out2, "decode: rc=%d %s" % (rc2, out2))
    rc, out = run_tool("send", "--port", "NO_SUCH_PORT_FOR_ESP32_BENCH_TEST", "context")
    expect(rc == 2 and ("pyserial" in out or "cannot open" in out), "send without a port: rc=%d %s" % (rc, out))
    # run checks every scenario value before it opens the port.
    for event, word in ((["--ack", "9:1:5@0.3"], "M_WARNING_ACK"), (["--request", "5:1@0.3"], "M_REQUEST"),
                        (["--query", "4@0.3"], "M_QUERY"), (["--request", "2:1:40000@0.3"], "i16"),
                        (["--inject", "zz@0.3"], "unknown damage")):
        rc, out = run_tool("run", "--port", "NO_SUCH_PORT_FOR_ESP32_BENCH_TEST", "--duration", "1", *event)
        expect(rc == 2 and word in out and "pyserial" not in out and "cannot open" not in out,
               "run %s must stop before the port: rc=%d %s" % (event, rc, out))
    rc, out = run_tool("run", "--port", "NO_SUCH_PORT_FOR_ESP32_BENCH_TEST", "--duration", "1", "--ack", "2:7@0.3")
    expect(rc == 2 and ("pyserial" in out or "cannot open" in out),
           "a valid --ack reaches the port: rc=%d %s" % (rc, out))
    # vss_bench uartm: a value the codec refuses and a number beyond 64 bits are usage errors (exit 2).
    rc, out = vss_bench("uartm", "encode", "M_QUERY", "query_id=0")
    expect(rc == 2 and codec_verdict(out) == "MALFORMED", "uartm encode refused: rc=%d %s" % (rc, out))
    rc, out = vss_bench("uartm", "encode", "M_QUERY", "query_id=1", "session_id=99999999999999999999999")
    expect(rc == 2 and "does not fit" in out, "uartm encode beyond 64 bits: rc=%d %s" % (rc, out))


class FakePort:
    """Serial port stand-in: records writes; reads return scripted bytes once their time has come."""

    def __init__(self, script):
        import time as _t
        self._t = _t
        self.t0 = _t.monotonic()
        self.script = sorted(script, key=lambda x: x[0])     # (seconds, bytes)
        self.written = bytearray()
        self.writes = []
        self.closed = False
        self.input_reset = False

    @property
    def in_waiting(self):
        now = self._t.monotonic() - self.t0
        return sum(len(b) for at, b in self.script if at <= now)

    def read(self, n):
        now = self._t.monotonic() - self.t0
        out = bytearray()
        while self.script and self.script[0][0] <= now and len(out) < n:
            at, b = self.script.pop(0)
            out += b
        if not out:
            self._t.sleep(0.005)
        return bytes(out)

    def write(self, data):
        self.written += data
        self.writes.append((self._t.monotonic() - self.t0, bytes(data)))
        return len(data)

    def flush(self):
        pass

    def reset_input_buffer(self):
        self.input_reset = True

    def close(self):
        self.closed = True


def written_frames(raw):
    return [e for e in eb.Stream(eb.UPLINK, assembly_limit_ms=1e9).feed(bytes(raw), 0.0) if e[0] == "frame"]


ALIVE_TODAY = dict(domain_boot_id=DOMAIN_BOOT, update_sequence=0, source_age_ms=0xFFFF, domain_state=0,
                   value_quality=3)


def domain_script(until, extra=(), link0=1):
    """What DOMAIN sends: M_DOMAIN_ALIVE every 250 ms (as the board today) and the extra
    (time, message, field changes), LINK_SEQUENCE +1 in time order."""
    items = [(0.02 + 0.25 * i, "M_DOMAIN_ALIVE", ALIVE_TODAY) for i in range(int((until - 0.02) / 0.25) + 1)]
    items = sorted(items + list(extra), key=lambda x: x[0])
    return [(t, downlink(name, link0 + i, **change)) for i, (t, name, change) in enumerate(items)]


def session(argv, script):
    ports = []

    def fake_open(port, baud):
        ports.append(FakePort(script))
        return ports[-1]
    eb.open_port = fake_open
    out = io.StringIO()
    rc = eb.main(argv, out)
    text = out.getvalue()
    sm, vm = re.search(r"^SUMMARY (.*)$", text, re.M), re.search(r"^VERDICT (.*)$", text, re.M)
    return rc, text, json.loads(sm.group(1)) if sm else None, json.loads(vm.group(1)) if vm else None, ports[0]


def case_esp32_session():
    work = Path(tempfile.mkdtemp(prefix="esp32_bench_test_"))
    try:
        sessions(work)
    finally:
        for f in work.iterdir():
            f.unlink()
        work.rmdir()


def sessions(work):
    drain = eb.OPEN_DRAIN_S
    eb.OPEN_DRAIN_S = 0.0            # the scripts start at the port open; run 8 checks the drain
    try:
        session_runs(work)
    finally:
        eb.OPEN_DRAIN_S = drain


def session_runs(work):
    state, log = work / "state.json", work / "log.jsonl"
    # 1. The DOMAIN board today: only M_DOMAIN_ALIVE (INIT / NO_DATA), M_CONTEXT is not taken.
    script = domain_script(1.2)
    script.append((0.33, b"\x11\x22"))
    rc, text, summ, v, port = session(["run", "--port", "FAKE", "--duration", "1.0", "--state", str(state),
                                       "--log", str(log), "--request", "1:1@0.3", "--query", "0@0.5",
                                       "--ack", "2:7@0.6", "--inject", "crc@0.7", "app_age_ms=40"], script)
    if not expect(rc == 0 and summ and v, "run 1: rc=%d %s" % (rc, text)):
        return
    sent = written_frames(port.written)
    ctx = [e for e in sent if e[1] == eb.TYPE_CONTEXT]
    req = [e for e in sent if e[1] == eb.TYPE_REQUEST]
    qry = [e for e in sent if e[1] == eb.TYPE_QUERY]
    ack = [e for e in sent if e[1] == eb.TYPE_ACK]
    expect(8 <= len(ctx) <= 12 and len(req) == 1 and len(qry) == 1 and len(ack) == 1,
           "run 1: %d contexts, %d requests, %d queries, %d acks" % (len(ctx), len(req), len(qry), len(ack)))
    expect([e[2] for e in sent] == list(range(1, len(sent) + 1)), "run 1: LINK_SEQUENCE %r" % [e[2] for e in sent])
    expect([e[3]["app_update_sequence"] for e in ctx] == list(range(1, len(ctx) + 1)) and
           all(e[3]["app_age_ms"] == 40 and e[3]["session_id"] == 1 and e[3]["device_context_id"] == 7 for e in ctx),
           "run 1: context sequences / field=value / identity")
    expect(req and req[0][3]["request_id"] == 1 and req[0][3]["kind"] == 1 and req[0][3]["operation"] == 1 and
           qry and qry[0][3]["query_id"] == 1 and ack and ack[0][3]["warning_type"] == 2 and
           ack[0][3]["occurrence_id"] == 7 and ack[0][3]["warning_domain_boot"] == DOMAIN_BOOT,
           "run 1: request / query / ack fields %r %r %r" % (req, qry, ack))
    expect(summ["tx_damaged"] == {"crc": 1} and summ["rx_damaged"]["noise_bytes"] == 2 and
           summ["alive"]["count"] >= 4, "run 1: summary %r" % summ)
    expect(v["overall"] == "FAIL" and v["failed"] == ["line"], "run 1: the 2 noise bytes fail the line: %r" % v)
    expect(v["alive"]["verdict"] == "PASS" and v["context_taken"]["verdict"] == "HOLD" and
           v["requests"][0]["verdict"] == "HOLD" and v["queries"][0]["verdict"] == "HOLD" and
           any("HOLD path" in n for n in v["alive"]["notes"]), "run 1: verdict %r" % v)
    st = json.loads(state.read_text(encoding="utf-8"))
    expect(st == {"device": 7, "session": 1, "next_link": len(sent) + 1, "app_seq": len(ctx),
                  "proximity_seq": len(ctx), "registration_seq": len(ctx), "request_id": 1, "query_id": 1},
           "run 1: state %r" % st)
    lines = [json.loads(x) for x in log.read_text(encoding="utf-8").splitlines()]
    expect(lines[0]["event"] == "open" and [x["event"] for x in lines[-2:]] == ["summary", "verdict"] and
           sum(1 for x in lines if x["event"] == "rx") == summ["alive"]["count"], "run 1: log %r" %
           sorted({x["event"] for x in lines}))
    # 2. A DOMAIN that took the context: an M_RESULT for the request, a query answer and its End.
    st0 = json.loads(state.read_text(encoding="utf-8"))
    ctx_ids = dict(device_context_id=7, session_id=st0["session"], domain_boot_id=DOMAIN_BOOT)
    rid, qid = st0["request_id"] + 1, st0["query_id"] + 1
    script = domain_script(1.0, [
        (0.45, "M_RESULT", dict(ctx_ids, query_id=0, request_session=st0["session"], request_id=rid)),
        (0.62, "M_CIS_REAR", dict(ctx_ids, query_id=qid)),
        (0.64, "M_CIS_STATUS", dict(ctx_ids, query_id=qid)),
        (0.66, "M_QUERY_END", dict(ctx_ids, query_id=qid, item_count=2))], link0=100)
    rc, text, summ, v, port = session(["run", "--port", "FAKE", "--duration", "0.9", "--state", str(state),
                                       "--request", "2:1:-150@0.3", "--query", "0@0.5"], script)
    if not expect(rc == 0 and v, "run 2: rc=%d %s" % (rc, text)):
        return
    sent = written_frames(port.written)
    expect(sent and sent[0][2] == st0["next_link"], "run 2: LINK_SEQUENCE continues the state: %r" % sent[:1])
    req = [e for e in sent if e[1] == eb.TYPE_REQUEST]
    expect(req and req[0][3]["args[0]"] == 0x6A and req[0][3]["args[1]"] == 0xFF,
           "run 2: temperature -150 as i16 LE: %r" % req)
    expect(v["overall"] == "PASS" and v["context_taken"]["verdict"] == "TAKEN" and
           v["requests"][0]["verdict"] == "ANSWERED" and v["queries"][0]["verdict"] == "ENDED" and
           v["queries"][0]["items"] == 2, "run 2: verdict %r" % v)
    # 3. A new session in the middle; repeat mode keeps one observation and lets AGE grow.
    rc, text, summ, v, port = session(["run", "--port", "FAKE", "--duration", "0.6", "--state", str(state),
                                       "--context-mode", "repeat", "--session-change-at", "0.3"],
                                      domain_script(0.7))
    ctx = [e for e in written_frames(port.written) if e[1] == eb.TYPE_CONTEXT]
    first = [e for e in ctx if e[3]["session_id"] == st0["session"]]
    second = [e for e in ctx if e[3]["session_id"] == st0["session"] + 1]
    ages = [e[3]["app_age_ms"] for e in first]
    expect(rc == 0 and len(first) >= 2 and len(second) >= 2 and len({e[3]["app_update_sequence"] for e in first}) == 1
           and ages == sorted(ages) and ages[-1] >= ages[0] + 100, "run 3: %r" % [(e[2], e[3]["session_id"],
                                                                                 e[3]["app_update_sequence"],
                                                                                 e[3]["app_age_ms"]) for e in ctx])
    # Each repeat adds the time between the two frames plus the step (10 ms at 100 ms; checked with slack).
    timed = [(at, e) for at, data in port.writes for e in written_frames(data) if e[1] == eb.TYPE_CONTEXT]
    pairs = [(a, b) for a, b in zip(timed, timed[1:]) if a[1][3]["session_id"] == b[1][3]["session_id"] and
             a[1][3]["app_update_sequence"] == b[1][3]["app_update_sequence"]]
    expect(pairs and all(b[1][3]["app_age_ms"] - a[1][3]["app_age_ms"] >= (b[0] - a[0]) * 1000.0 + 3 for a, b in pairs),
           "run 3: AGE step per repeat %r" % [(round((b[0] - a[0]) * 1000.0, 1), b[1][3]["app_age_ms"] -
                                               a[1][3]["app_age_ms"]) for a, b in pairs])
    st = json.loads(state.read_text(encoding="utf-8"))
    expect(st["session"] == st0["session"] + 1, "run 3: state session %r" % st)
    # 4. send continues the state; --new-session moves the session on.
    rc, text, summ, v, port = session(["send", "--port", "FAKE", "query", "--state", str(state), "--new-session",
                                       "--listen", "0"], [])
    sent = written_frames(port.written)
    st2 = json.loads(state.read_text(encoding="utf-8"))
    expect(rc == 0 and len(sent) == 1 and sent[0][2] == st["next_link"] and sent[0][3]["session_id"] == st["session"] + 1
           and sent[0][3]["query_id"] == st["query_id"] + 1 and st2["session"] == st["session"] + 1 and v is None,
           "send: %r state %r" % (sent, st2))
    # 5. The tail of a frame cut by the port open and a frame still arriving when reading stops are
    # counted apart, not line errors (the assembly limit is raised so that the timing cannot matter).
    a = downlink("M_DOMAIN_ALIVE", 900, **ALIVE_TODAY)
    script = [(0.0, a[-13:])] + domain_script(0.6) + [(0.62, a[:20])]
    stream = eb.Stream
    eb.Stream = lambda d=eb.DOWNLINK, assembly_limit_ms=eb.ASSEMBLY_LIMIT_MS: stream(d, 400.0)
    try:
        rc, text, summ, v, port = session(["run", "--port", "FAKE", "--duration", "0.6", "--context-every", "0"],
                                          script)
    finally:
        eb.Stream = stream
    expect(rc == 0 and v and v["overall"] == "PASS" and summ["rx_lead_bytes"] == 13 and
           summ["rx_tail_bytes"] == 20 and port.input_reset, "run 5: rc=%d %s" % (rc, text))
    # 6. Garbage before the first frame, as long as the longest downlink frame or longer, is a line error.
    rc, text, summ, v, port = session(["run", "--port", "FAKE", "--duration", "0.5", "--context-every", "0"],
                                      [(0.0, bytes(range(1, 101)))] + domain_script(0.5))
    expect(rc == 0 and v and v["failed"] == ["line"] and summ["rx_lead_bytes"] == 0 and
           summ["rx_damaged"]["noise_bytes"] == 100, "run 6: rc=%d %s" % (rc, text))
    # 7. A new session: DOMAIN still carries the context of the state file (the earlier run).
    st3 = json.loads(state.read_text(encoding="utf-8"))
    held = dict(device_context_id=st3["device"], session_id=st3["session"], domain_boot_id=DOMAIN_BOOT)
    rc, text, summ, v, port = session(["run", "--port", "FAKE", "--duration", "0.5", "--state", str(state),
                                       "--new-session"], domain_script(0.5, [(0.05, "M_CIS_REAR", held)]))
    expect(rc == 0 and v and v["overall"] == "PASS" and v["context"]["earlier_run"] == 1 and
           v["context"]["unknown_before_taken"] == 0, "run 7: rc=%d %s" % (rc, text))
    # 8. A USB bridge hands over frames from before the open and a cut one: the drain drops them, then
    # the tail of a frame cut by the drain is counted apart.
    a = downlink("M_DOMAIN_ALIVE", 900, **ALIVE_TODAY)
    old_bytes = a + a + a[:10]
    live = [(0.25 + 0.25 * i, downlink("M_DOMAIN_ALIVE", 1 + i, **ALIVE_TODAY)) for i in range(3)]
    eb.OPEN_DRAIN_S = 0.15
    try:
        rc, text, summ, v, port = session(["run", "--port", "FAKE", "--duration", "0.5", "--context-every", "0"],
                                          [(0.0, old_bytes), (0.2, a[-13:])] + live)
    finally:
        eb.OPEN_DRAIN_S = 0.0
    expect(rc == 0 and v and v["overall"] == "PASS" and summ["rx_drained_bytes"] == len(old_bytes) and
           summ["rx_lead_bytes"] == 13, "run 8: rc=%d %s" % (rc, text))
    # 9. An acknowledgement before any M_DOMAIN_ALIVE (no DOMAIN_BOOT_ID yet) is skipped and counted.
    rc, text, summ, v, port = session(["run", "--port", "FAKE", "--duration", "0.6", "--context-every", "0",
                                       "--ack", "2:7@0.0"],
                                      [(0.3 + 0.25 * i, downlink("M_DOMAIN_ALIVE", 1 + i, **ALIVE_TODAY))
                                       for i in range(2)])
    expect(rc == 0 and v and v["overall"] == "PASS" and summ["events_skipped"] == 1 and
           not [e for e in written_frames(port.written) if e[1] == eb.TYPE_ACK], "run 9: rc=%d %s" % (rc, text))


CASES = {name[len("case_"):]: fn for name, fn in globals().items() if name.startswith("case_")}


def main():
    global VSS_BENCH
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("case", choices=sorted(CASES))
    ap.add_argument("--vss-bench", required=True, type=Path, help="the vss_bench executable of this build")
    args = ap.parse_args()
    VSS_BENCH = args.vss_bench
    if not os.path.isfile(str(VSS_BENCH)):
        print("usage: vss_bench not found: %s" % VSS_BENCH)
        return 2
    CASES[args.case]()
    print("%s: %s" % (args.case, "FAIL (%d)" % len(FAILS) if FAILS else "PASS"))
    return 1 if FAILS else 0


if __name__ == "__main__":
    raise SystemExit(main())
