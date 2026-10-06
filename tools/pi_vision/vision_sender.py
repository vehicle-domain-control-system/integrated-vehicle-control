#!/usr/bin/env python3
"""Counts the people in the Raspberry Pi camera picture and sends the result to the CIS.

    python3 vision_sender.py --port auto                  # camera + detector
    python3 vision_sender.py --port auto --simulate 0,1,2,3,4,5,6,u,x   # no camera
    python3 vision_sender.py --dry-run --simulate 0,2     # no serial port either

Protocol: see pi_protocol.py. A VISION frame is sent every 100 ms. The number of
people is judged a few times per second; the frames between two judgements repeat
the last one with a growing AGE and the same SEQ, so the CIS can tell a new
judgement from a repeat.
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
LOG_PERIOD_S = 2.0


# --------------------------------------------------------------------------
# Latest judgement, shared between the detection thread and the sender
# --------------------------------------------------------------------------

class Judgement:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._seq = 0
        self._count = pp.COUNT_UNKNOWN
        self._quality = pp.REASON_NOT_READY
        self._fault = pp.FAULT_NONE
        self._t = time.monotonic()

    def update(self, count: int, quality: int, fault: int) -> None:
        """Records a NEW judgement (the sequence number increases)."""
        with self._lock:
            self._seq = (self._seq + 1) & 0xFFFF
            self._count = count
            self._quality = quality
            self._fault = fault
            self._t = time.monotonic()

    def snapshot(self):
        with self._lock:
            age_ms = int((time.monotonic() - self._t) * 1000)
            return self._seq, self._count, self._quality, self._fault, age_ms


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

    def _fault(self, fault: int) -> None:
        self._window.clear()
        self._judgement.update(pp.COUNT_UNKNOWN, pp.REASON_VISION_FAULT, fault)

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
                n = self._detector.count(frame)
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
                self._judgement.update(pp.COUNT_UNKNOWN, pp.REASON_NOT_READY, pp.FAULT_NONE)
            else:
                count = statistics.median_low(self._window)
                if count > pp.MAX_COUNT:
                    self._judgement.update(pp.COUNT_UNKNOWN, pp.REASON_OUT_OF_RANGE, pp.FAULT_NONE)
                else:
                    self._judgement.update(count, pp.REASON_NONE, pp.FAULT_NONE)

            self.stop_event.wait(max(0.0, self._period - (time.monotonic() - t0)))

        if source is not None:
            source.close()


# --------------------------------------------------------------------------
# Simulation (no camera): cycles through a list of results
# --------------------------------------------------------------------------

class SimulationWorker(threading.Thread):
    """Tokens: 0..5 people, 6+ more than the CIS can count, u unknown (no data),
    n not ready, x camera fault."""

    def __init__(self, tokens, judgement: Judgement, period_s: float) -> None:
        super().__init__(daemon=True)
        self._tokens = tokens
        self._judgement = judgement
        self._period = period_s
        self.stop_event = threading.Event()

    def run(self) -> None:
        i = 0
        while not self.stop_event.is_set():
            tok = self._tokens[i % len(self._tokens)]
            i += 1
            if tok == "u":
                self._judgement.update(pp.COUNT_UNKNOWN, pp.REASON_NO_DATA, pp.FAULT_NONE)
            elif tok == "n":
                self._judgement.update(pp.COUNT_UNKNOWN, pp.REASON_NOT_READY, pp.FAULT_NONE)
            elif tok == "x":
                self._judgement.update(pp.COUNT_UNKNOWN, pp.REASON_VISION_FAULT, pp.FAULT_CAMERA)
            else:
                n = int(tok)
                if n > pp.MAX_COUNT:
                    self._judgement.update(pp.COUNT_UNKNOWN, pp.REASON_OUT_OF_RANGE, pp.FAULT_NONE)
                else:
                    self._judgement.update(n, pp.REASON_NONE, pp.FAULT_NONE)
            self.stop_event.wait(self._period)


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


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="auto", help="serial port, or 'auto' (default)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--dry-run", action="store_true", help="do not open a serial port; print the judgements")
    ap.add_argument("--simulate", metavar="LIST", help="no camera: cycle through e.g. 0,1,2,3,4,5,6,u,x")
    ap.add_argument("--sim-period", type=float, default=3.0, help="seconds per simulated result")
    ap.add_argument("--source", default="picamera", help="'picamera' (default), a camera index (0) or a video file")
    ap.add_argument("--detector", choices=("auto", "yolo", "hog"), default="auto")
    ap.add_argument("--width", type=int, default=640)
    ap.add_argument("--height", type=int, default=480)
    ap.add_argument("--fps", type=float, default=5.0, help="judgements per second")
    ap.add_argument("--window", type=int, default=5, help="pictures used for the median")
    ap.add_argument("--min-samples", type=int, default=3, help="pictures needed before a count is reported")
    args = ap.parse_args()

    judgement = Judgement()
    if args.simulate:
        worker = SimulationWorker(args.simulate.split(","), judgement, args.sim_period)
    else:
        import person_counter as pc
        detector = pc.make_detector(args.detector)
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

    session = random.randint(1, 255)          # changes whenever this program starts
    parser = pp.FrameParser()
    sent = 0
    last_perm = None
    last_seq_printed = None
    perm_seq = None

    worker.start()
    next_send = time.monotonic()
    next_log = next_send + LOG_PERIOD_S
    try:
        while True:
            now = time.monotonic()
            if now >= next_send:
                seq, count, quality, fault, age_ms = judgement.snapshot()
                frame = pp.build_vision(session, seq, age_ms, count, quality, fault)
                if ser is not None:
                    ser.write(frame)
                sent += 1
                next_send += SEND_PERIOD_S
                if next_send < now:                          # fell behind: do not burst
                    next_send = now + SEND_PERIOD_S
                if args.dry_run and seq != last_seq_printed:
                    last_seq_printed = seq
                    print(f"judgement #{seq}: count={count} quality={quality} fault={fault}")

            if ser is not None:
                data = ser.read(256)
                for f in parser.feed(data):
                    if isinstance(f, pp.Permission):
                        perm_seq = f.seq
                        state = (f.permission, f.session)
                        if state != last_perm:
                            last_perm = state
                            print(f"CIS permission: {PERMISSION_NAMES.get(f.permission, f.permission)} "
                                  f"(source age {f.source_age_ms} ms, CIS session {f.session})")

            if now >= next_log:
                next_log += LOG_PERIOD_S
                seq, count, quality, fault, age_ms = judgement.snapshot()
                print(f"seq={seq} count={count} quality={quality} fault={fault} age={age_ms}ms "
                      f"sent={sent} cis_frames={parser.frames_ok} crc_err={parser.bad_crc}"
                      + (f" cis_seq={perm_seq}" if perm_seq is not None else ""))
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
