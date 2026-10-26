#!/usr/bin/env python3
"""Checks of tools/pi_bench/pi_bench.py, the Pi stand-in of the CIS board test.

Its UART-C bytes are compared with the repository codec through the vss_bench
executable, both ways: the payloads and LINK_SEQUENCE 1 frames vss_bench builds
with the codec, the codec's decode of every frame the tool builds, and the
codec's verdict on refused values. The damaged frames, the receive stream, the
offline commands and a whole serial session on a stand-in port (no hardware,
no pyserial) are checked too.

    test_pi_bench.py CASE --vss-bench PATH

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
TOOL = NETWORK / "tools" / "pi_bench" / "pi_bench.py"
BOOT = 0x50493031

spec = importlib.util.spec_from_file_location("pi_bench_under_test", TOOL)
pb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pb)

VSS_BENCH = None
FAILS = []
# vss_bench prints the reference frame (LINK_SEQUENCE 1) as a note line in upper-case hex.
REF_FRAME = re.compile(r"^# (%02X %02X .*)$" % (0xA5, 0x5A), re.M)


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


def hex_bytes(text):
    return bytes(int(t, 16) for t in text.split())


def fields_of(line):
    """'fields: a=1 b=0x0000ABCD ...' (also '# fields:') -> {name: int}."""
    out = {}
    for name, value in re.findall(r"(\w+)=(0x[0-9A-Fa-f]+|\d+)", line):
        out[name] = int(value, 0)
    return out


def tool_frame(word, boot, seq, link, settings):
    msg = pb.BY_WORD[word]
    values = pb.record(msg, boot, seq, settings)
    payload = pb.encode_payload(msg, values)
    return msg, values, payload, pb.frame_bytes(msg.type, link, payload)


# Value sets: valid ones of both records, and ones the codec refuses.
RESULT_CASES = [
    {},
    {"occupant_presence": 0, "occupant_count": 0},
    {"occupant_count": 5, "vision_age_ms": 65535},
    {"occupant_presence": 255, "presence_validity": 0, "presence_reason": 6,
     "occupant_count": 255, "count_validity": 0, "count_reason": 6},
    {"presence_validity": 0, "presence_reason": 4, "count_validity": 0, "count_reason": 4,
     "vision_status": 3, "vision_fault_code": 2},
    {"vision_status": 1},
    {"count_validity": 0, "count_reason": 9, "occupant_count": 3},
    # refused by the codec
    {"occupant_count": 6},
    {"occupant_presence": 2},
    {"presence_reason": 10},
    {"presence_reason": 1},
    {"occupant_presence": 0},
    {"vision_status": 3},
    {"vision_status": 4},
    {"vision_status": 0},
    {"vision_fault_code": 1},
    {"vision_fault_code": 5},
    {"vision_status": 5, "presence_validity": 0, "count_validity": 0},
]
STATUS_CASES = [
    {},
    {"vision_status": 3, "active_fault": 2, "last_fault": 2, "affected_function": 3},
    {"vision_status": 4, "last_fault": 4, "status_age_ms": 1000},
    {"vision_status": 0},
    # refused by the codec
    {"vision_status": 5},
    {"active_fault": 5},
    {"affected_function": 4},
]
PERMISSION_CASES = [
    {},
    {"vehicle_power_permission": 0},
    {"vehicle_power_permission": 255, "value_quality": 3, "quality_reason": 6},
    {"value_quality": 1, "quality_reason": 5, "source_age_ms": 600},
    {"value_quality": 2, "quality_reason": 9, "use_limit_ms": 500},
]


def case_pi_crc():
    expect(pb.crc16(b"123456789") == 0x29B1, "CRC16 of '123456789' is not 0x29B1")
    _, _, _, frame = tool_frame("status", BOOT, 42, 1, {})
    # The P_VISION_STATUS reference frame of tools/vss_bench/README.md (link 1).
    expect(pb.hex_text(frame) == "a5 5a 01 61 10 00 01 00 31 30 49 50 2a 00 00 00 01 00 00 00 00 14 00 00 d2 e8",
           "the P_VISION_STATUS frame differs from the vss_bench README reference: " + pb.hex_text(frame))
    for word in ("result", "status", "permission"):
        for link in (0, 1, 2, 255, 256, 65535):
            _, _, _, f = tool_frame(word, 0x43495331 if word == "permission" else BOOT, 7, link, {})
            expect(pb.from_le(f[-2:]) == pb.crc16(f[2:-2]), "%s link %d: stored CRC wrong" % (word, link))
            expect(pb.from_le(f[6:8]) == link, "%s: LINK_SEQUENCE %d not at B6..7 LE" % (word, link))


def case_pi_payload_vs_codec():
    """vss_bench builds P_VISION_RESULT / STATUS with the codec: payload and the link-1 frame must match."""
    for word, cmd, cases in (("result", "vision-result", RESULT_CASES), ("status", "vision-status", STATUS_CASES)):
        msg = pb.BY_WORD[word]
        for k, settings in enumerate(cases):
            seq = 100 + k
            words = ["%s=%d" % kv for kv in sorted(settings.items())]
            rc, out = vss_bench("uart", cmd, "--boot", "%08x" % BOOT, "--seq", seq, *words)
            values = pb.record(msg, BOOT, seq, settings)
            verdict = pb.check(msg, values)
            if rc != 0:
                expect(verdict != "OK", "%s case %d: the codec refuses (%s) but the tool says OK" %
                       (word, k, out.strip().splitlines()[-1] if out.strip() else rc))
                continue
            if not expect(verdict == "OK", "%s case %d: the codec accepts but the tool says %s" % (word, k, verdict)):
                continue
            payload_line = re.search(r"^# payload: (.*)$", out, re.M)
            frame_line = REF_FRAME.search(out)
            fields_line = re.search(r"^# fields:(.*)$", out, re.M)
            if not expect(payload_line and frame_line and fields_line, "%s case %d: vss_bench output not understood" %
                          (word, k)):
                continue
            payload = pb.encode_payload(msg, values)
            frame = pb.frame_bytes(msg.type, 1, payload)
            expect(payload == hex_bytes(payload_line.group(1)), "%s case %d: payload differs: tool %s codec %s" %
                   (word, k, pb.hex_text(payload), payload_line.group(1)))
            expect(frame == hex_bytes(frame_line.group(1)), "%s case %d: frame differs: tool %s codec %s" %
                   (word, k, pb.hex_text(frame), frame_line.group(1)))
            expect(fields_of(fields_line.group(1)) == values, "%s case %d: fields differ" % (word, k))


def case_pi_codec_decodes_tool():
    """Every frame the tool builds is accepted by the codec decode with the same fields."""
    sets = [("result", BOOT, RESULT_CASES), ("status", BOOT, STATUS_CASES),
            ("permission", 0x43495331, PERMISSION_CASES)]
    for word, boot, cases in sets:
        msg = pb.BY_WORD[word]
        for k, settings in enumerate(cases):
            values = pb.record(msg, boot, 9 + k, settings)
            if pb.check(msg, values) != "OK":
                continue
            for link in (1, 2, 65535, 0):
                _, _, _, frame = tool_frame(word, boot, 9 + k, link, settings)
                rc, out = vss_bench("uart", "decode", pb.hex_text(frame))
                head = re.search(r"link_seq=(\d+)", out)
                fields = re.search(r"^fields:(.*)$", out, re.M)
                ok = (rc == 0 and "crc: OK" in out and ("codec: OK %s" % msg.name) in out and head and fields)
                if not expect(ok, "%s case %d link %d: the codec does not accept the tool frame: %s" %
                              (word, k, link, out.strip())):
                    continue
                expect(int(head.group(1)) == link, "%s case %d: codec reads link %s, tool wrote %d" %
                       (word, k, head.group(1), link))
                expect(fields_of(fields.group(1)) == values, "%s case %d link %d: codec fields differ" %
                       (word, k, link))


def case_pi_tool_decodes_codec():
    """Frames the codec builds (vss_bench reference frames, and P_PERMISSION payloads the codec frames
    itself) are decoded by the tool's stream with the codec's fields."""
    for word, cmd, cases in (("result", "vision-result", RESULT_CASES), ("status", "vision-status", STATUS_CASES)):
        msg = pb.BY_WORD[word]
        for k, settings in enumerate(cases):
            words = ["%s=%d" % kv for kv in sorted(settings.items())]
            rc, out = vss_bench("uart", cmd, "--boot", "%08x" % BOOT, "--seq", 300 + k, *words)
            if rc != 0:
                continue
            frame = hex_bytes(REF_FRAME.search(out).group(1))
            want = fields_of(re.search(r"^# fields:(.*)$", out, re.M).group(1))
            events = pb.Stream(pb.UPLINK).feed(frame, 0.0)
            ok = len(events) == 1 and events[0][0] == "frame" and events[0][1] is msg
            if expect(ok, "%s case %d: the tool does not decode the codec frame: %r" % (word, k, events)):
                expect(events[0][2] == 1 and events[0][3] == want, "%s case %d: tool fields differ" % (word, k))
    msg = pb.BY_WORD["permission"]
    for k, settings in enumerate(PERMISSION_CASES):
        values = pb.record(msg, 0x43495331, 500 + k, settings)
        payload = pb.encode_payload(msg, values)
        rc, out = vss_bench("uart", "decode", pb.hex_text(payload))   # payload only: the codec frames it
        fields = re.search(r"^fields:(.*)$", out, re.M)
        if not expect(rc == 0 and "codec: OK P_PERMISSION" in out and fields,
                      "permission case %d: codec payload decode failed: %s" % (k, out.strip())):
            continue
        expect(fields_of(fields.group(1)) == values, "permission case %d: codec fields differ" % k)
        events = pb.Stream(pb.DOWNLINK).feed(pb.frame_bytes(msg.type, 1, payload), 0.0)
        expect(len(events) == 1 and events[0][0] == "frame" and events[0][3] == fields_of(fields.group(1)),
               "permission case %d: tool decode differs from the codec" % k)


def case_pi_damaged_frames():
    msg = pb.BY_WORD["result"]
    values = pb.record(msg, BOOT, 77, {})
    payload = pb.encode_payload(msg, values)
    good = pb.frame_bytes(msg.type, 5, payload)
    codec_wants = {"crc": "crc: BAD", "trunc": "INVALID", "len": "INVALID", "version": "INVALID",
                   "type": "INVALID", "noise": "error:"}
    tool_wants = {"crc": "bad_crc", "trunc": "timeout", "len": "bad_header", "version": "bad_header",
                  "type": "bad_header", "noise": "noise"}
    for kind in pb.BAD_KINDS:
        bad = pb.damage(kind, msg, 5, payload)
        expect(bad != good, "%s: damaged frame equals the good one" % kind)
        rc, out = vss_bench("uart", "decode", pb.hex_text(bad))
        expect(rc != 0 and codec_wants[kind] in out, "%s: codec does not reject it as expected: rc=%d %s" %
               (kind, rc, out.strip()))
        # The tool's stream: the damaged bytes, 60 ms of silence, then the good frame (recovered).
        s = pb.Stream(pb.UPLINK)
        events = s.feed(bad, 0.0) + s.poll(0.06) + s.feed(good, 0.07)
        kinds = [e[0] for e in events]
        expect(tool_wants[kind] in kinds, "%s: tool stream events %s lack %s" % (kind, kinds, tool_wants[kind]))
        frames = [e for e in events if e[0] == "frame"]
        expect(len(frames) == 1 and frames[0][3] == values and frames[0][2] == 5,
               "%s: the good frame after the damaged one is not recovered: %s" % (kind, kinds))
        # Without the pause the stream resyncs too (a truncated head then needs the good frame's bytes).
        s = pb.Stream(pb.UPLINK)
        events = s.feed(bad + good, 0.0)
        frames = [e for e in events if e[0] == "frame"]
        expect(len(frames) == 1 and frames[0][3] == values, "%s: back-to-back good frame not recovered: %s" %
               (kind, [e[0] for e in events]))


def case_pi_stream():
    msg = pb.BY_WORD["permission"]
    frames = [pb.frame_bytes(msg.type, link, pb.encode_payload(msg, pb.record(msg, 0x43495331, link, {})))
              for link in (1, 2, 3)]
    # byte by byte, 87 us apart (115200 bit/s)
    s, got, t = pb.Stream(pb.DOWNLINK), [], 0.0
    for f in frames:
        for b in f:
            got += s.feed(bytes((b,)), t)
            t += 87e-6
    expect([e[2] for e in got if e[0] == "frame"] == [1, 2, 3], "byte-wise feed: frames %r" % got)
    # SYNC bytes inside a payload stay payload
    inner = pb.record(msg, 0x5AA55AA5, 0xA55AA55A, {})
    f = pb.frame_bytes(msg.type, 0x5AA5, pb.encode_payload(msg, inner))
    got = pb.Stream(pb.DOWNLINK).feed(b"\x00\xa5" + f, 0.0)
    fr = [e for e in got if e[0] == "frame"]
    expect(len(fr) == 1 and fr[0][3] == inner and fr[0][2] == 0x5AA5, "SYNC inside the payload: %r" % got)
    # an uplink type on the downlink stream is a bad header
    up = pb.BY_WORD["result"]
    f = pb.frame_bytes(up.type, 1, pb.encode_payload(up, pb.record(up, BOOT, 1, {})))
    got = pb.Stream(pb.DOWNLINK).feed(f, 0.0)
    expect(got and got[0][0] == "bad_header" and not [e for e in got if e[0] == "frame"],
           "uplink frame on the downlink stream: %r" % [e[0] for e in got])
    # assembly limit: a frame spread over more than 50 ms is dropped
    s = pb.Stream(pb.DOWNLINK)
    got = s.feed(frames[0][:10], 0.0) + s.feed(frames[0][10:], 0.051)
    expect("timeout" in [e[0] for e in got] and not [e for e in got if e[0] == "frame"],
           "a frame over 51 ms was accepted: %r" % [e[0] for e in got])
    s = pb.Stream(pb.DOWNLINK)
    got = s.feed(frames[0][:10], 0.0) + s.feed(frames[0][10:], 0.049)
    expect([e[0] for e in got] == ["frame"], "a frame within 49 ms was not accepted: %r" % [e[0] for e in got])


def run_tool(*args):
    r = subprocess.run([sys.executable, "-B", str(TOOL), *map(str, args)], capture_output=True, text=True,
                       timeout=60)
    return r.returncode, r.stdout + r.stderr


def case_pi_offline_cli():
    rc, out = run_tool("frame", "result", "--boot", "50493031", "--seq", "42", "occupant_count=2")
    last = out.strip().splitlines()[-1] if out.strip() else ""
    expect(rc == 0 and re.fullmatch(r"([0-9a-f]{2} ){28}[0-9a-f]{2}", last), "frame result: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "result", "occupant_count=6")
    expect(rc == 2 and "refuses" in out, "a refused value must be exit 2: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "result", "--allow-invalid", "occupant_count=6")
    expect(rc == 0 and "codec=MALFORMED" in out, "--allow-invalid: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "result", "occupant_count=2", "--seq", "7", "vision_age_ms=40", "--link", "3",
                       "occupant_count=3")
    expect(rc == 0 and "occupant_count=3" in out and "vision_age_ms=40" in out,
           "field=value words before, between and after options (the last one wins): rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "result", "--sek", "3")
    expect(rc == 2 and "unrecognized arguments: --sek" in out, "an unknown option is exit 2: rc=%d %s" % (rc, out))
    rc, out = run_tool("decode", "--hex", "00")
    expect(rc == 2 and "unrecognized arguments" in out, "decode takes no options: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "result", "no_such_field=1")
    expect(rc == 2 and "unknown field" in out, "unknown field: rc=%d %s" % (rc, out))
    rc, out = run_tool("frame", "result", "pi_boot_id=1")
    expect(rc == 2, "identity field set by field=value must be refused: rc=%d" % rc)
    rc, out = run_tool("frame", "status", "--bad", "crc")
    rc2, out2 = run_tool("decode", out.strip().splitlines()[-1])
    expect(rc == 0 and rc2 == 1 and "bad_crc" in out2, "decode of a damaged frame: rc=%d %s" % (rc2, out2))
    rc, out = run_tool("frame", "permission", "--seq", "4")
    rc2, out2 = run_tool("decode", "hex=" + out.strip().splitlines()[-1].replace(" ", ""))
    expect(rc2 == 0 and "frame P_PERMISSION link=1 crc=OK codec=OK" in out2, "decode: rc=%d %s" % (rc2, out2))
    rc, out = run_tool("send", "--port", "NO_SUCH_PORT_FOR_PI_BENCH_TEST", "result")
    expect(rc == 2 and ("pyserial" in out or "cannot open" in out), "send without a port: rc=%d %s" % (rc, out))


class FakePort:
    """Serial port stand-in: records writes; reads return scripted bytes once their time has come."""

    def __init__(self, script):
        import time as _t
        self._t = _t
        self.t0 = _t.monotonic()
        self.script = list(script)       # (seconds, bytes)
        self.written = bytearray()
        self.closed = False

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
        return len(data)

    def flush(self):
        pass

    def close(self):
        self.closed = True


def frames_written(raw):
    events = pb.Stream(pb.UPLINK, assembly_limit_ms=1e9).feed(bytes(raw), 0.0)
    return events


def case_pi_session():
    """send and run on a stand-in port: frames, numbers across runs (--state), the summary, the log."""
    perm = pb.BY_WORD["permission"]
    replies = [(0.05 + 0.1 * i, pb.frame_bytes(perm.type, 1 + i, pb.encode_payload(
        perm, pb.record(perm, 0x43495331, 1 + i, {"source_age_ms": 30 + i})))) for i in range(3)]
    replies.insert(1, (0.12, b"\x11\x22"))
    ports = []

    def fake_open(port, baud):
        ports.append(FakePort(replies if not ports else []))
        return ports[-1]
    pb.open_port = fake_open
    work = Path(tempfile.mkdtemp(prefix="pi_bench_test_"))
    state, log = work / "state.json", work / "log.jsonl"
    out = io.StringIO()
    rc = pb.main(["run", "--port", "FAKE", "--duration", "0.5", "--result-every", "50", "--status-every", "200",
                  "--state", str(state), "--log", str(log), "--inject", "crc@0.27", "occupant_count=2"], out)
    text = out.getvalue()
    m = re.search(r"^SUMMARY (.*)$", text, re.M)
    if not expect(rc == 0 and m, "run: rc=%d %s" % (rc, text)):
        return
    summ = json.loads(m.group(1))
    events = frames_written(ports[0].written)
    frames = [e for e in events if e[0] == "frame"]
    results = [e for e in frames if e[1].type == pb.TYPE_RESULT]
    statuses = [e for e in frames if e[1].type == pb.TYPE_STATUS]
    expect(8 <= len(results) <= 11 and 2 <= len(statuses) <= 3,
           "run: %d results, %d statuses in 0.5 s" % (len(results), len(statuses)))
    expect([e[2] for e in frames] == list(range(1, len(frames) + 1)),
           "run: LINK_SEQUENCE not 1, 2, 3 ... over results and statuses: %r" % [e[2] for e in frames])
    expect([e[3]["vision_sequence"] for e in results] == list(range(1, len(results) + 1)),
           "run: VISION_SEQUENCE not 1, 2, 3 ...")
    expect(all(e[3]["occupant_count"] == 2 and e[3]["pi_boot_id"] == pb.DEFAULT_BOOT for e in results),
           "run: field=value or the default BOOT not applied")
    expect("bad_crc" in [e[0] for e in events], "run: the injected damaged frame is not on the line")
    expect(summ["tx"]["P_VISION_RESULT"] == len(results) and summ["tx"]["damaged"] == {"crc": 1},
           "run: summary tx counts %r" % summ["tx"])
    p = summ["permission"]
    expect(p["count"] == 3 and p["update_sequence_first"] == 1 and p["update_sequence_last"] == 3 and
           p["update_sequence_steps_not_plus_one"] == 0 and p["cis_boot_ids"] == ["0x43495331"] and
           p["codec_not_ok"] == 0 and p["link_steps_not_plus_one"] == 0 and p["use_limit_ms"] == [1500] and
           p["last"]["source_age_ms"] == 32 and 50.0 <= p["interval_ms_min"] <= p["interval_ms_max"] < 1500.0,
           "run: permission summary %r" % p)
    expect(summ["rx_damaged"]["noise_bytes"] == 2, "run: noise bytes %r" % summ["rx_damaged"])
    st = json.loads(state.read_text(encoding="utf-8"))
    expect(st == {"boot": pb.DEFAULT_BOOT, "next_link": len(frames) + 1, "result_seq": len(results),
                  "status_seq": len(statuses)}, "run: state %r" % st)
    lines = [json.loads(x) for x in log.read_text(encoding="utf-8").splitlines()]
    expect(lines[0]["event"] == "open" and lines[-1]["event"] == "summary" and
           sum(1 for x in lines if x["event"] == "rx") == 3, "run: log lines %r" % [x["event"] for x in lines])
    # send continues the numbers of the state file
    out = io.StringIO()
    rc = pb.main(["send", "--port", "FAKE", "result", "--state", str(state), "--listen", "0"], out)
    sent = [e for e in frames_written(ports[1].written) if e[0] == "frame"]
    expect(rc == 0 and len(sent) == 1 and sent[0][2] == len(frames) + 1 and
           sent[0][3]["vision_sequence"] == len(results) + 1, "send: does not continue the state: %r" % sent)
    # --new-boot: next BOOT, numbers from 1
    out = io.StringIO()
    rc = pb.main(["send", "--port", "FAKE", "status", "--state", str(state), "--new-boot", "--listen", "0"], out)
    sent = [e for e in frames_written(ports[2].written) if e[0] == "frame"]
    expect(rc == 0 and len(sent) == 1 and sent[0][2] == 1 and sent[0][3]["status_sequence"] == 1 and
           sent[0][3]["pi_boot_id"] == pb.DEFAULT_BOOT + 1, "send --new-boot: %r" % sent)
    st = json.loads(state.read_text(encoding="utf-8"))
    expect(st == {"boot": pb.DEFAULT_BOOT + 1, "next_link": 2, "result_seq": 0, "status_seq": 1},
           "--new-boot state %r" % st)
    # repeat mode: one judgement, AGE grows
    out = io.StringIO()
    rc = pb.main(["run", "--port", "FAKE", "--duration", "0.3", "--result-every", "100", "--status-every", "0",
                  "--result-mode", "repeat", "--state", str(state)], out)
    res = [e for e in frames_written(ports[3].written) if e[0] == "frame"]
    ages = [e[3]["vision_age_ms"] for e in res]
    expect(rc == 0 and len(res) >= 2 and len({e[3]["vision_sequence"] for e in res}) == 1 and
           ages == sorted(ages) and ages[-1] >= ages[0] + 150, "repeat mode: %r" % [(e[2], e[3]) for e in res])
    for f in work.iterdir():
        f.unlink()
    work.rmdir()


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
