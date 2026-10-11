"""UART-C v1: frames between the Raspberry Pi (camera) and the CIS.

Defined in docs/network/interface/03_COMMUNICATION/30_UART_C.md (pull request #68, status
PROPOSED). The C implementation is firmware/CIS/src/pi_protocol.h - keep both in sync.
All multi-byte numbers are little-endian.

    byte 0..1  SYNC            0xA5 0x5A
    byte 2     VERSION         1
    byte 3     MESSAGE_TYPE    0x60 P_VISION_RESULT (Pi -> CIS), 0x61 P_VISION_STATUS (Pi -> CIS),
                               0x62 P_PERMISSION (CIS -> Pi)
    byte 4..5  PAYLOAD_LENGTH  u16, exact for each type (19, 16, 15)
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

PAYLOAD_LEN = {TYPE_VISION_RESULT: 19, TYPE_VISION_STATUS: 16, TYPE_PERMISSION: 15}
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


Message = Union[VisionResult, VisionStatus, Permission]


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

    def __init__(self, accept=(TYPE_PERMISSION,)) -> None:
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
            else:
                msg = Permission(_u32_at(p, 0), _u32_at(p, 4), p[8], p[9], p[10], _u16_at(p, 11), _u16_at(p, 13))
            return Frame(link_seq, msg)
