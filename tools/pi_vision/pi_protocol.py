"""Frame format of the UART link between the Raspberry Pi (camera) and the CIS.

This link is internal to the CIS module. The C implementation is in
firmware/CIS/src/pi_protocol.h (keep both in sync).

    byte 0   0xA5                    start of frame (2 bytes)
    byte 1   0x5A
    byte 2   TYPE                    0x01 VISION (Pi -> CIS), 0x02 PERMISSION (CIS -> Pi)
    byte 3   LEN                     payload length (fixed for each TYPE)
    byte 4.. payload
    last 2   CRC16, low byte first   CRC-16/CCITT-FALSE over TYPE, LEN and payload

Multi-byte numbers are little-endian. 115200 baud, 8N1, no flow control.

VISION payload (8 bytes, Pi -> CIS, every 100 ms):
    0     SESSION   random number chosen when this program starts
    1..2  SEQ       +1 for every NEW camera judgement; repeats keep the number
    3..4  AGE_MS    time since that judgement (saturates at 65535)
    5     COUNT     people 0..5, 255 = unknown
    6     QUALITY   0 = usable, otherwise a reason code (see REASON_*)
    7     FAULT     0 none, 1 initialization, 2 camera, 3 detector, 4 data

PERMISSION payload (6 bytes, CIS -> Pi, every 500 ms):
    0     SESSION   changes when the CIS restarts
    1..2  SEQ       +1 for every frame
    3     PERMISSION  0 denied, 1 allowed, 255 unknown
    4..5  SOURCE_AGE  age of the permission information in ms (65535 = unknown)
"""

from dataclasses import dataclass
from typing import List, Optional, Union

SOF0 = 0xA5
SOF1 = 0x5A

TYPE_VISION = 0x01
TYPE_PERMISSION = 0x02

PAYLOAD_LEN = {TYPE_VISION: 8, TYPE_PERMISSION: 6}
OVERHEAD = 6                      # SOF(2) + TYPE + LEN + CRC(2)

COUNT_UNKNOWN = 255
MAX_COUNT = 5

# QUALITY reason codes (same numbers as the CIS QUALITY_REASON)
REASON_NONE = 0
REASON_NOT_READY = 1
REASON_OUT_OF_RANGE = 2
REASON_SENSOR_FAULT = 3
REASON_VISION_FAULT = 4
REASON_STALE = 5
REASON_NO_DATA = 6

# FAULT codes
FAULT_NONE = 0
FAULT_INIT = 1
FAULT_CAMERA = 2
FAULT_DETECTOR = 3
FAULT_DATA = 4

PERMISSION_DENIED = 0
PERMISSION_ALLOWED = 1
PERMISSION_UNKNOWN = 255


def crc16_ccitt_false(data: bytes) -> int:
    """CRC-16/CCITT-FALSE. crc16_ccitt_false(b"123456789") == 0x29B1."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _frame(frame_type: int, payload: bytes) -> bytes:
    body = bytes([frame_type, len(payload)]) + payload
    crc = crc16_ccitt_false(body)
    return bytes([SOF0, SOF1]) + body + bytes([crc & 0xFF, crc >> 8])


def build_vision(session: int, seq: int, age_ms: int, count: int,
                 quality: int = REASON_NONE, fault: int = FAULT_NONE) -> bytes:
    age_ms = max(0, min(65535, int(age_ms)))
    payload = bytes([session & 0xFF, seq & 0xFF, (seq >> 8) & 0xFF,
                     age_ms & 0xFF, age_ms >> 8,
                     count & 0xFF, quality & 0xFF, fault & 0xFF])
    return _frame(TYPE_VISION, payload)


def build_permission(session: int, seq: int, permission: int, source_age_ms: int) -> bytes:
    source_age_ms = max(0, min(65535, int(source_age_ms)))
    payload = bytes([session & 0xFF, seq & 0xFF, (seq >> 8) & 0xFF, permission & 0xFF,
                     source_age_ms & 0xFF, source_age_ms >> 8])
    return _frame(TYPE_PERMISSION, payload)


@dataclass
class Vision:
    session: int
    seq: int
    age_ms: int
    count: int
    quality: int
    fault: int


@dataclass
class Permission:
    session: int
    seq: int
    permission: int
    source_age_ms: int


Frame = Union[Vision, Permission]


class FrameParser:
    """Streaming parser. Feed it received bytes; it returns complete valid frames.

    A damaged frame is skipped (the next frame start is searched inside it too),
    so one bad byte never blocks the following frames.
    """

    def __init__(self) -> None:
        self._buf = bytearray()
        self.frames_ok = 0
        self.bad_crc = 0
        self.bad_format = 0
        self.skipped_bytes = 0

    def feed(self, data: bytes) -> List[Frame]:
        self._buf.extend(data)
        out: List[Frame] = []
        while True:
            frame = self._step()
            if frame is None:
                return out
            out.append(frame)

    def _step(self) -> Optional[Frame]:
        buf = self._buf
        while True:
            if len(buf) >= 1 and buf[0] != SOF0:
                del buf[0]
                self.skipped_bytes += 1
                continue
            if len(buf) >= 2 and buf[1] != SOF1:
                del buf[0]
                self.skipped_bytes += 1
                continue
            if len(buf) < 4:
                return None
            length = PAYLOAD_LEN.get(buf[2], 0)
            if length == 0 or buf[3] != length:
                self.bad_format += 1
                del buf[0]
                continue
            total = OVERHEAD + length
            if len(buf) < total:
                return None
            crc_calc = crc16_ccitt_false(bytes(buf[2:4 + length]))
            crc_rx = buf[total - 2] | (buf[total - 1] << 8)
            if crc_calc != crc_rx:
                self.bad_crc += 1
                del buf[0]
                continue
            ftype = buf[2]
            p = bytes(buf[4:4 + length])
            del buf[:total]
            self.frames_ok += 1
            if ftype == TYPE_VISION:
                return Vision(p[0], p[1] | (p[2] << 8), p[3] | (p[4] << 8), p[5], p[6], p[7])
            return Permission(p[0], p[1] | (p[2] << 8), p[3], p[4] | (p[5] << 8))
