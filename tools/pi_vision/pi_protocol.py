"""UART-C v1: frames between the Raspberry Pi (camera) and the CIS.

Defined in docs/network/interface/03_COMMUNICATION/30_UART_C.md (pull request #68, status
PROPOSED). The C implementation is firmware/CIS/src/pi_protocol.h - keep both in sync.
All multi-byte numbers are little-endian.

    byte 0..1  SYNC            0xA5 0x5A
    byte 2     VERSION         1
    byte 3     MESSAGE_TYPE    0x60 P_VISION_RESULT (Pi -> CIS), 0x61 P_VISION_STATUS (Pi -> CIS),
                               0x62 P_PERMISSION (CIS -> Pi)
               0x70 P_BENCH_SENSORS (CIS -> Pi, bench only, not part of UART-C v1)
    byte 4..5  PAYLOAD_LENGTH  u16, exact for each type (19, 16, 15, 52)
    byte 6..7  LINK_SEQUENCE   u16, +1 for every frame sent in one direction
    byte 8..   PAYLOAD
    last 2     CRC16           CRC-16/CCITT-FALSE over VERSION .. end of payload (SYNC excluded)

A frame has to be complete within 50 ms of its first byte; a damaged or late frame is
dropped and the search for the next SYNC continues after its first byte.
"""

import time
from dataclasses import dataclass
from typing import List, Optional, Union

SOF0 = 0xA5
SOF1 = 0x5A
VERSION = 1

TYPE_VISION_RESULT = 0x60
TYPE_VISION_STATUS = 0x61
TYPE_PERMISSION = 0x62
# NOT part of UART-C v1: bench-only report of the CIS sensor values (CIS -> Pi). The Network stack
# drops unknown types, so the CIS sends it only in the bench build (CIS_BENCH_REPORT).
TYPE_BENCH_SENSORS = 0x70

PAYLOAD_LEN = {TYPE_VISION_RESULT: 19, TYPE_VISION_STATUS: 16, TYPE_PERMISSION: 15,
               TYPE_BENCH_SENSORS: 52}
OVERHEAD = 10                      # SYNC(2) VERSION TYPE LEN(2) LINK_SEQ(2) CRC(2)
ASSEMBLY_TIMEOUT_S = 0.050

# tri-state / validity
UNKNOWN = 255
ABSENT = 0
PRESENT = 1
INVALID = 0
VALID = 1
MAX_COUNT = 5

# Quality reasons (QUALITY_REASON codes)
REASON_NONE = 0
REASON_NOT_READY = 1
REASON_OUT_OF_RANGE = 2
REASON_SENSOR_FAULT = 3
REASON_VISION_FAULT = 4
REASON_STALE = 5
REASON_NO_DATA = 6
REASON_COMMUNICATION_FAULT = 7
REASON_RECOVERING = 8
REASON_DATA_INVALID = 9

# Function state
STATE_NOT_READY = 0
STATE_READY = 1
STATE_ACTIVE = 2
STATE_FAULT = 3
STATE_RECOVERING = 4

# Vision fault codes
FAULT_NONE = 0
FAULT_INIT = 1
FAULT_CAMERA = 2
FAULT_DETECTOR = 3
FAULT_DATA = 4

AFFECTS_PRESENCE = 0x01
AFFECTS_COUNT = 0x02

PERMISSION_DENIED = 0
PERMISSION_ALLOWED = 1
PERMISSION_UNKNOWN = 255

# NetValueQuality (P_PERMISSION)
QUALITY_OK = 0
QUALITY_STALE = 1
QUALITY_INVALID = 2
QUALITY_NO_DATA = 3


def crc16_ccitt_false(data: bytes) -> int:
    """CRC-16/CCITT-FALSE. crc16_ccitt_false(b"123456789") == 0x29B1."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _u16(v: int) -> bytes:
    v = max(0, min(65535, int(v)))
    return bytes([v & 0xFF, v >> 8])


def _u32(v: int) -> bytes:
    v &= 0xFFFFFFFF
    return bytes([v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, v >> 24])


def _frame(frame_type: int, link_seq: int, payload: bytes) -> bytes:
    assert len(payload) == PAYLOAD_LEN[frame_type]
    body = bytes([VERSION, frame_type]) + _u16(len(payload)) + _u16(link_seq & 0xFFFF) + payload
    return bytes([SOF0, SOF1]) + body + _u16(crc16_ccitt_false(body))


@dataclass
class VisionResult:
    """P_VISION_RESULT"""
    pi_boot_id: int
    vision_sequence: int
    vision_age_ms: int
    presence: int
    presence_validity: int
    presence_reason: int
    count: int
    count_validity: int
    count_reason: int
    vision_status: int
    fault_code: int


@dataclass
class VisionStatus:
    """P_VISION_STATUS"""
    pi_boot_id: int
    status_sequence: int
    vision_status: int
    active_fault: int
    last_fault: int
    status_age_ms: int
    affected_function: int


@dataclass
class Permission:
    """P_PERMISSION (CIS -> Pi)"""
    cis_boot_id: int
    update_sequence: int
    permission: int
    value_quality: int
    quality_reason: int
    use_limit_ms: int
    source_age_ms: int


@dataclass
class BenchValue:
    """One sensor value of P_BENCH_SENSORS: number, validity, reason, new-measurement number, age."""
    value: int
    valid: int
    reason: int
    sequence: int
    age_ms: int


@dataclass
class BenchSensors:
    """P_BENCH_SENSORS (CIS -> Pi), bench only. Layout (52 bytes, little-endian):
         0 reported_status u8 (PROXIMITY_STATUS after the power gate, 5 = INACTIVE)
         1 reported_valid u8   2 reported_reason u8   3 sensor_status u8 (before the power gate)
         4 distance_mm u16    6 rear_sequence u32   10 rear_age_ms u16
        12 temperature i16 (0.01 degC)  14 valid  15 reason  16 sequence u32  20 age u16
        22 humidity u16 (0.01 %RH)      24 valid  25 reason  26 sequence u32  30 age u16
        32 illuminance u32 (lx)         36 valid  37 reason  38 sequence u32  42 age u16
        44 pi_link_up u8  45 occupant_count u8  46 occupant_presence u8  47 vision_sequence u32
        51 cis_state u8
    """
    reported_status: int
    reported_valid: int
    reported_reason: int
    sensor_status: int
    distance_mm: BenchValue
    temperature: BenchValue
    humidity: BenchValue
    illuminance: BenchValue
    pi_link_up: int
    occupant_count: int
    occupant_presence: int
    vision_sequence: int
    cis_state: int


Message = Union[VisionResult, VisionStatus, Permission, BenchSensors]


@dataclass
class Frame:
    link_seq: int
    message: Message


def build_vision_result(link_seq: int, m: VisionResult) -> bytes:
    p = (_u32(m.pi_boot_id) + _u32(m.vision_sequence) + _u16(m.vision_age_ms)
         + bytes([m.presence & 0xFF, m.presence_validity & 0xFF, m.presence_reason & 0xFF,
                  m.count & 0xFF, m.count_validity & 0xFF, m.count_reason & 0xFF,
                  m.vision_status & 0xFF]) + _u16(m.fault_code))
    return _frame(TYPE_VISION_RESULT, link_seq, p)


def build_vision_status(link_seq: int, m: VisionStatus) -> bytes:
    p = (_u32(m.pi_boot_id) + _u32(m.status_sequence) + bytes([m.vision_status & 0xFF])
         + _u16(m.active_fault) + _u16(m.last_fault) + _u16(m.status_age_ms)
         + bytes([m.affected_function & 0xFF]))
    return _frame(TYPE_VISION_STATUS, link_seq, p)


def build_permission(link_seq: int, m: Permission) -> bytes:
    p = (_u32(m.cis_boot_id) + _u32(m.update_sequence)
         + bytes([m.permission & 0xFF, m.value_quality & 0xFF, m.quality_reason & 0xFF])
         + _u16(m.use_limit_ms) + _u16(m.source_age_ms))
    return _frame(TYPE_PERMISSION, link_seq, p)


def build_bench_sensors(link_seq: int, m: BenchSensors) -> bytes:
    """Used by the tests; the CIS firmware builds this frame itself."""
    def value(v: BenchValue, signed: bool = False, wide: bool = False) -> bytes:
        num = _u32(v.value) if wide else _u16(v.value & 0xFFFF)
        return num + bytes([v.valid & 0xFF, v.reason & 0xFF]) + _u32(v.sequence) + _u16(v.age_ms)
    p = (bytes([m.reported_status & 0xFF, m.reported_valid & 0xFF, m.reported_reason & 0xFF,
                m.sensor_status & 0xFF])
         + _u16(m.distance_mm.value) + _u32(m.distance_mm.sequence) + _u16(m.distance_mm.age_ms)
         + value(m.temperature) + value(m.humidity) + value(m.illuminance, wide=True)
         + bytes([m.pi_link_up & 0xFF, m.occupant_count & 0xFF, m.occupant_presence & 0xFF])
         + _u32(m.vision_sequence) + bytes([m.cis_state & 0xFF]))
    return _frame(TYPE_BENCH_SENSORS, link_seq, p)


def _u16_at(p: bytes, i: int) -> int:
    return p[i] | (p[i + 1] << 8)


def _u32_at(p: bytes, i: int) -> int:
    return p[i] | (p[i + 1] << 8) | (p[i + 2] << 16) | (p[i + 3] << 24)


class FrameParser:
    """Streaming parser. Feed it received bytes; it returns complete valid frames.

    `accept` is the set of message types this side takes: the Pi receives only
    P_PERMISSION (the default), the CIS side receives P_VISION_RESULT / P_VISION_STATUS.
    A damaged frame is skipped (the next frame start is searched inside it too).
    """

    def __init__(self, accept=(TYPE_PERMISSION, TYPE_BENCH_SENSORS)) -> None:
        self._accept = set(accept)
        self._buf = bytearray()
        self._t_start = 0.0
        self.frames_ok = 0
        self.bad_crc = 0
        self.bad_format = 0
        self.assembly_timeouts = 0
        self.skipped_bytes = 0

    def feed(self, data: bytes, now: Optional[float] = None) -> List[Frame]:
        now = time.monotonic() if now is None else now
        self.expire(now)
        if data and not self._buf:
            self._t_start = now
        self._buf.extend(data)
        out: List[Frame] = []
        while True:
            frame = self._step(now)
            if frame is None:
                return out
            out.append(frame)

    def expire(self, now: float) -> None:
        """Drops a frame that is not complete 50 ms after its first byte."""
        if self._buf and now - self._t_start > ASSEMBLY_TIMEOUT_S:
            self.assembly_timeouts += 1
            del self._buf[0]
            self._t_start = now

    def _drop(self, now: float) -> None:
        del self._buf[0]
        self._t_start = now

    def _step(self, now: float) -> Optional[Frame]:
        buf = self._buf
        while True:
            if len(buf) >= 1 and buf[0] != SOF0:
                self._drop(now)
                self.skipped_bytes += 1
                continue
            if len(buf) >= 2 and buf[1] != SOF1:
                self._drop(now)
                self.skipped_bytes += 1
                continue
            if len(buf) >= 3 and buf[2] != VERSION:
                self.bad_format += 1
                self._drop(now)
                continue
            if len(buf) >= 4 and (buf[3] not in self._accept or buf[3] not in PAYLOAD_LEN):
                self.bad_format += 1
                self._drop(now)
                continue
            if len(buf) < 6:
                return None
            length = _u16_at(buf, 4)
            if length != PAYLOAD_LEN[buf[3]]:
                self.bad_format += 1
                self._drop(now)
                continue
            total = OVERHEAD + length
            if len(buf) < total:
                return None
            crc_calc = crc16_ccitt_false(bytes(buf[2:8 + length]))
            if crc_calc != _u16_at(buf, 8 + length):
                self.bad_crc += 1
                self._drop(now)
                continue
            ftype = buf[3]
            link_seq = _u16_at(buf, 6)
            p = bytes(buf[8:8 + length])
            del buf[:total]
            self._t_start = now
            self.frames_ok += 1
            if ftype == TYPE_VISION_RESULT:
                msg: Message = VisionResult(_u32_at(p, 0), _u32_at(p, 4), _u16_at(p, 8), p[10], p[11], p[12],
                                            p[13], p[14], p[15], p[16], _u16_at(p, 17))
            elif ftype == TYPE_VISION_STATUS:
                msg = VisionStatus(_u32_at(p, 0), _u32_at(p, 4), p[8], _u16_at(p, 9), _u16_at(p, 11),
                                   _u16_at(p, 13), p[15])
            elif ftype == TYPE_BENCH_SENSORS:
                temp = _u16_at(p, 12)
                if temp >= 0x8000:
                    temp -= 0x10000                      # signed 0.01 degC
                msg = BenchSensors(
                    p[0], p[1], p[2], p[3],
                    BenchValue(_u16_at(p, 4), p[1], p[2], _u32_at(p, 6), _u16_at(p, 10)),
                    BenchValue(temp, p[14], p[15], _u32_at(p, 16), _u16_at(p, 20)),
                    BenchValue(_u16_at(p, 22), p[24], p[25], _u32_at(p, 26), _u16_at(p, 30)),
                    BenchValue(_u32_at(p, 32), p[36], p[37], _u32_at(p, 38), _u16_at(p, 42)),
                    p[44], p[45], p[46], _u32_at(p, 47), p[51])
            else:
                msg = Permission(_u32_at(p, 0), _u32_at(p, 4), p[8], p[9], p[10], _u16_at(p, 11), _u16_at(p, 13))
            return Frame(link_seq, msg)
