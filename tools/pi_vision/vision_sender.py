#!/usr/bin/env python3
"""Counts the people in the Raspberry Pi camera picture and sends the result to the CIS.

    python3 vision_sender.py --port auto                  # camera + detector
    python3 vision_sender.py --port auto --simulate 0,1,2,3,4,5,6,u,x   # no camera
    python3 vision_sender.py --dry-run --simulate 0,2     # no serial port either

Protocol: UART-C v1, see pi_protocol.py. A P_VISION_RESULT frame is sent every 100 ms and a
P_VISION_STATUS frame every second. The people are judged a few times per second; the frames
between two judgements repeat the last one with a growing age and the same VISION_SEQUENCE, so
the CIS can tell a new judgement from a repeat.
"""

import argparse
import collections
import random
import statistics
import sys
import threading
import time

import pi_protocol as pp

SEND_PERIOD_S = 0.1
SLOW_DETECTION_MS = 600.0        # above this the judgements cannot stay fresh for the CIS
LOG_PERIOD_S = 2.0


# --------------------------------------------------------------------------
# Latest judgement, shared between the detection thread and the sender
# --------------------------------------------------------------------------

class Judgement:
    """Latest judgement, shared between the detection thread and the sender.

    Every update is a NEW judgement (VISION_SEQUENCE and STATUS_SEQUENCE increase). The Pi is
    the producer of the original quality: presence and count each carry their own validity and
    reason.
    """

    def __init__(self, boot_id: int) -> None:
        self.boot_id = boot_id
        self._lock = threading.Lock()
        self._vision_seq = 0
        self._status_seq = 0
        self._t = time.monotonic()
        self._last_fault = pp.FAULT_NONE
        self._set(pp.UNKNOWN, pp.INVALID, pp.REASON_NOT_READY,
                  pp.UNKNOWN, pp.INVALID, pp.REASON_NOT_READY,
                  pp.STATE_NOT_READY, pp.FAULT_NONE, count_sequences=False)

    def _set(self, presence, p_valid, p_reason, count, c_valid, c_reason, status, fault,
             count_sequences=True) -> None:
        with self._lock:
            if count_sequences:
                self._vision_seq = (self._vision_seq + 1) & 0xFFFFFFFF
                self._status_seq = (self._status_seq + 1) & 0xFFFFFFFF
            self._presence, self._p_valid, self._p_reason = presence, p_valid, p_reason
            self._count, self._c_valid, self._c_reason = count, c_valid, c_reason
            self._status, self._fault = status, fault
            if fault != pp.FAULT_NONE:
                self._last_fault = fault
            self._t = time.monotonic()

    # --- the kinds of judgement ---
    def ok(self, n: int) -> None:
        """n people (0..5)."""
        self._set(pp.PRESENT if n > 0 else pp.ABSENT, pp.VALID, pp.REASON_NONE,
                  n, pp.VALID, pp.REASON_NONE, pp.STATE_ACTIVE, pp.FAULT_NONE)

    def not_ready(self) -> None:
        """Not enough pictures yet: unknown, never "nobody"."""
        self._set(pp.UNKNOWN, pp.INVALID, pp.REASON_NOT_READY,
                  pp.UNKNOWN, pp.INVALID, pp.REASON_NOT_READY, pp.STATE_NOT_READY, pp.FAULT_NONE)

    def no_data(self) -> None:
        """The function runs but cannot tell."""
        self._set(pp.UNKNOWN, pp.INVALID, pp.REASON_NO_DATA,
                  pp.UNKNOWN, pp.INVALID, pp.REASON_NO_DATA, pp.STATE_ACTIVE, pp.FAULT_NONE)

    def more_than_five(self) -> None:
        """People are certainly present, but the CIS only counts 0..5."""
        self._set(pp.PRESENT, pp.VALID, pp.REASON_NONE,
                  pp.UNKNOWN, pp.INVALID, pp.REASON_OUT_OF_RANGE, pp.STATE_ACTIVE, pp.FAULT_NONE)

    def fault(self, code: int) -> None:
        self._set(pp.UNKNOWN, pp.INVALID, pp.REASON_VISION_FAULT,
                  pp.UNKNOWN, pp.INVALID, pp.REASON_VISION_FAULT, pp.STATE_FAULT, code)

    # --- frames ---
    def result_frame(self) -> pp.VisionResult:
        with self._lock:
            age = int((time.monotonic() - self._t) * 1000)
            return pp.VisionResult(self.boot_id, self._vision_seq, age,
                                   self._presence, self._p_valid, self._p_reason,
                                   self._count, self._c_valid, self._c_reason,
                                   self._status, self._fault)

    def status_frame(self) -> pp.VisionStatus:
        with self._lock:
            age = int((time.monotonic() - self._t) * 1000)
            affected = 0
            if self._status != pp.STATE_ACTIVE:
                affected = pp.AFFECTS_PRESENCE | pp.AFFECTS_COUNT
            return pp.VisionStatus(self.boot_id, self._status_seq, self._status,
                                   self._fault, self._last_fault, age, affected)

    def state(self):
        with self._lock:
            return self._vision_seq, self._presence, self._count, self._status, self._fault


# --------------------------------------------------------------------------
# Detection thread (real camera)
# --------------------------------------------------------------------------

class DetectionWorker(threading.Thread):
    def __init__(self, make_source, detector, judgement: Judgement, fps: float,
                 window: int, min_samples: int) -> None:
        super().__init__(daemon=True)
        self._make_source = make_source
        self._detector = detector
        self._judgement = judgement
        self._period = 1.0 / fps
        self._window = collections.deque(maxlen=window)
        self._min_samples = min_samples
        self.stop_event = threading.Event()
        self.detect_ms = 0.0                  # time of the last detection, for the log
        self._slow_warned = False

    def _fault(self, fault: int) -> None:
        self._window.clear()
        self._judgement.fault(fault)

    def run(self) -> None:
        import person_counter as pc

        source = None
        while not self.stop_event.is_set():
            if source is None:
                try:
                    source = self._make_source()
                except pc.CameraError as exc:
                    print(f"[camera] {exc}", file=sys.stderr)
                    self._fault(pp.FAULT_INIT)
                    self.stop_event.wait(2.0)             # try again later
                    continue

            t0 = time.monotonic()
            try:
                frame = source.read()
            except pc.CameraError as exc:
                print(f"[camera] {exc}", file=sys.stderr)
                self._fault(pp.FAULT_CAMERA)
                source.close()
                source = None
                self.stop_event.wait(2.0)
                continue

            try:
                t_detect = time.monotonic()
                n = self._detector.count(frame)
                self.detect_ms = (time.monotonic() - t_detect) * 1000.0
                if self.detect_ms > SLOW_DETECTION_MS and not self._slow_warned:
                    self._slow_warned = True
                    print(f"[detector] {self.detect_ms:.0f} ms per picture is slow: the CIS does not use a "
                          f"judgement older than 1 s, so the result would keep turning invalid. "
                          f"Use a smaller --imgsz, --detector hog, or an NCNN model (see README).",
                          file=sys.stderr)
            except Exception as exc:                       # detector problem
                print(f"[detector] {exc}", file=sys.stderr)
                self._fault(pp.FAULT_DETECTOR)
                self.stop_event.wait(1.0)
                continue

            # Median of the last few pictures: one missed or false detection does not
            # change the answer. Until enough pictures are collected the result is
            # "not ready" - it is never reported as "nobody".
            self._window.append(n)
            if len(self._window) < self._min_samples:
                self._judgement.not_ready()
            else:
                count = statistics.median_low(self._window)
                if count > pp.MAX_COUNT:
                    self._judgement.more_than_five()
                else:
                    self._judgement.ok(count)

            self.stop_event.wait(max(0.0, self._period - (time.monotonic() - t0)))

        if source is not None:
            source.close()


# --------------------------------------------------------------------------
# Simulation (no camera): cycles through a list of results
# --------------------------------------------------------------------------

class SimulationWorker(threading.Thread):
    """Tokens: 0..5 people, 6+ more than the CIS can count (presence is still reported),
    u unknown (no data), n not ready, x camera fault.

    A value is kept for `period_s`. Like the real detector, a new judgement
    (new SEQ) is made every JUDGE_PERIOD_S, otherwise the CIS would treat the
    result as too old after 1 s."""

    JUDGE_PERIOD_S = 0.3

    def __init__(self, tokens, judgement: Judgement, period_s: float) -> None:
        super().__init__(daemon=True)
        self._tokens = tokens
        self._judgement = judgement
        self._period = period_s
        self.stop_event = threading.Event()

    def _judge(self, tok: str) -> None:
        if tok == "u":
            self._judgement.no_data()
        elif tok == "n":
            self._judgement.not_ready()
        elif tok == "x":
            self._judgement.fault(pp.FAULT_CAMERA)
        else:
            n = int(tok)
            if n > pp.MAX_COUNT:
                self._judgement.more_than_five()
            else:
                self._judgement.ok(n)

    def run(self) -> None:
        i = 0
        while not self.stop_event.is_set():
            tok = self._tokens[i % len(self._tokens)]
            i += 1
            t_end = time.monotonic() + self._period
            while not self.stop_event.is_set() and time.monotonic() < t_end:
                self._judge(tok)
                self.stop_event.wait(min(self.JUDGE_PERIOD_S, max(0.0, t_end - time.monotonic())))


# --------------------------------------------------------------------------
# Serial port
# --------------------------------------------------------------------------

def find_port() -> str:
    from serial.tools import list_ports
    ports = list(list_ports.comports())
    keys = ("opensda", "p&e", "pemicro", "cmsis", "mbed")
    named = [p for p in ports if any(k in f"{p.description} {p.manufacturer}".lower() for k in keys)]
    acm = [p for p in ports if "ttyACM" in p.device]
    candidates = named or acm
    if len(candidates) == 1:
        return candidates[0].device
    listing = "\n".join(f"  {p.device}  {p.description}" for p in ports) or "  (no serial ports)"
    raise SystemExit("Cannot choose the serial port automatically. Use --port.\n" + listing)


PERMISSION_NAMES = {pp.PERMISSION_DENIED: "DENIED", pp.PERMISSION_ALLOWED: "ALLOWED",
                    pp.PERMISSION_UNKNOWN: "UNKNOWN"}
STATUS_PERIOD_S = 1.0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="auto", help="serial port, or 'auto' (default)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--dry-run", action="store_true", help="do not open a serial port; print the judgements")
    ap.add_argument("--simulate", metavar="LIST", help="no camera: cycle through e.g. 0,1,2,3,4,5,6,u,x")
    ap.add_argument("--sim-period", type=float, default=3.0, help="seconds per simulated result")
    ap.add_argument("--source", default="picamera", help="'picamera' (default), a camera index (0) or a video file")
    ap.add_argument("--detector", choices=("auto", "yolo", "hog"), default="auto")
    ap.add_argument("--model", default="yolov8n.pt",
                    help="YOLO weights (a .pt file, or an exported folder such as yolov8n_ncnn_model)")
    ap.add_argument("--imgsz", type=int, default=320, help="YOLO input size: smaller = faster (default 320)")
    ap.add_argument("--conf", type=float, default=0.4, help="YOLO confidence threshold")
    ap.add_argument("--width", type=int, default=640)
    ap.add_argument("--height", type=int, default=480)
    ap.add_argument("--fps", type=float, default=3.0, help="judgements per second (the CIS needs at least 2)")
    ap.add_argument("--window", type=int, default=3, help="pictures used for the median")
    ap.add_argument("--min-samples", type=int, default=2, help="pictures needed before a count is reported")
    args = ap.parse_args()

    boot_id = random.getrandbits(32) or 1        # Pi start number, changes whenever this program starts; 0 is reserved
    judgement = Judgement(boot_id)
    if args.simulate:
        worker = SimulationWorker(args.simulate.split(","), judgement, args.sim_period)
    else:
        import person_counter as pc
        detector = pc.make_detector(args.detector, args.model, args.imgsz, args.conf)
        print(f"detector: {detector.name}")
        if args.source == "picamera":
            def make_source():
                return pc.PiCameraSource(args.width, args.height)
        else:
            def make_source():
                return pc.CvSource(args.source)
        worker = DetectionWorker(make_source, detector, judgement, args.fps,
                                 args.window, args.min_samples)

    ser = None
    if not args.dry_run:
        import serial
        port = find_port() if args.port == "auto" else args.port
        ser = serial.Serial(port, args.baud, timeout=0)
        print(f"serial port: {port} @ {args.baud}")

    parser = pp.FrameParser()                     # the Pi receives P_PERMISSION from the CIS
    link_seq = 0                                   # LINK_SEQUENCE of the frames sent to the CIS
    sent = 0
    last_perm = None
    last_printed = None
    last_status_key = None
    cis_update_seq = None

    def send(frame: bytes) -> None:
        nonlocal sent
        if ser is not None:
            ser.write(frame)
        sent += 1

    worker.start()
    next_send = time.monotonic()
    next_status = next_send
    next_log = next_send + LOG_PERIOD_S
    try:
        while True:
            now = time.monotonic()
            if now >= next_send:
                result = judgement.result_frame()
                send(pp.build_vision_result(link_seq, result))
                link_seq = (link_seq + 1) & 0xFFFF
                next_send += SEND_PERIOD_S
                if next_send < now:                          # fell behind: do not burst
                    next_send = now + SEND_PERIOD_S
                key = (result.presence, result.count, result.vision_status, result.fault_code)
                if args.dry_run and key != last_printed:
                    last_printed = key
                    print(f"judgement #{result.vision_sequence}: presence={result.presence} "
                          f"count={result.count} (count reason {result.count_reason}) "
                          f"state={result.vision_status} fault={result.fault_code}")

            status = judgement.status_frame()
            status_key = (status.vision_status, status.active_fault)
            if now >= next_status or status_key != last_status_key:
                last_status_key = status_key
                send(pp.build_vision_status(link_seq, status))
                link_seq = (link_seq + 1) & 0xFFFF
                next_status = now + STATUS_PERIOD_S

            if ser is not None:
                for f in parser.feed(ser.read(256)):
                    if isinstance(f.message, pp.Permission):
                        m = f.message
                        cis_update_seq = m.update_sequence
                        state = (m.permission, m.value_quality, m.cis_boot_id)
                        if state != last_perm:
                            last_perm = state
                            print(f"CIS permission: {PERMISSION_NAMES.get(m.permission, m.permission)} "
                                  f"(quality {m.value_quality}, source age {m.source_age_ms} ms, "
                                  f"CIS boot {m.cis_boot_id})")

            if now >= next_log:
                next_log += LOG_PERIOD_S
                seq, presence, count, state, fault = judgement.state()
                det = getattr(worker, "detect_ms", 0.0)
                print(f"seq={seq} presence={presence} count={count} state={state} fault={fault} "
                      + (f"detect={det:.0f}ms " if det else "")
                      + f"sent={sent} cis_frames={parser.frames_ok} crc_err={parser.bad_crc}"
                      + (f" cis_seq={cis_update_seq}" if cis_update_seq is not None else ""))
            time.sleep(0.005)
    except KeyboardInterrupt:
        print("stopped")
    finally:
        worker.stop_event.set()
        if ser is not None:
            ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
