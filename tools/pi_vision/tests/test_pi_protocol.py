import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import pi_protocol as pp  # noqa: E402


class CrcTest(unittest.TestCase):
    def test_check_value(self):
        self.assertEqual(pp.crc16_ccitt_false(b"123456789"), 0x29B1)


class BuildTest(unittest.TestCase):
    def test_vision_layout(self):
        f = pp.build_vision(session=7, seq=0x1234, age_ms=321, count=3)
        self.assertEqual(len(f), 14)
        self.assertEqual(f[:4], bytes([0xA5, 0x5A, 0x01, 8]))
        self.assertEqual(f[4:12], bytes([7, 0x34, 0x12, 321 & 0xFF, 321 >> 8, 3, 0, 0]))
        crc = pp.crc16_ccitt_false(f[2:12])
        self.assertEqual(f[12:], bytes([crc & 0xFF, crc >> 8]))

    def test_age_saturates(self):
        f = pp.build_vision(1, 1, 999999, 0)
        self.assertEqual(f[7] | (f[8] << 8), 65535)

    def test_permission_layout(self):
        f = pp.build_permission(9, 0x0102, pp.PERMISSION_ALLOWED, 1500)
        self.assertEqual(len(f), 12)
        self.assertEqual(f[:4], bytes([0xA5, 0x5A, 0x02, 6]))


class ParserTest(unittest.TestCase):
    def test_roundtrip(self):
        p = pp.FrameParser()
        out = p.feed(pp.build_vision(7, 0x1234, 321, 3, pp.REASON_NONE, pp.FAULT_NONE))
        self.assertEqual(out, [pp.Vision(7, 0x1234, 321, 3, 0, 0)])

    def test_permission_roundtrip(self):
        p = pp.FrameParser()
        out = p.feed(pp.build_permission(9, 5, pp.PERMISSION_UNKNOWN, 65535))
        self.assertEqual(out, [pp.Permission(9, 5, 255, 65535)])

    def test_garbage_before_frame(self):
        p = pp.FrameParser()
        out = p.feed(bytes([0x00, 0xA5, 0xA5, 0x11, 0x5A, 0xFF]) + pp.build_vision(1, 1, 0, 2))
        self.assertEqual(len(out), 1)
        self.assertEqual(out[0].count, 2)

    def test_crc_error_is_skipped_and_next_frame_ok(self):
        p = pp.FrameParser()
        bad = bytearray(pp.build_vision(1, 1, 0, 2))
        bad[9] ^= 1
        out = p.feed(bytes(bad) + pp.build_vision(1, 2, 0, 4))
        self.assertEqual([f.count for f in out], [4])
        self.assertGreaterEqual(p.bad_crc, 1)

    def test_good_frame_after_truncated_one(self):
        p = pp.FrameParser()
        out = p.feed(bytes([0xA5, 0x5A, 0x01, 8, 1, 2]) + pp.build_vision(2, 5, 10, 1))
        self.assertEqual(len(out), 1)
        self.assertEqual(out[0].seq, 5)

    def test_wrong_length_rejected(self):
        p = pp.FrameParser()
        bad = bytearray(pp.build_vision(1, 1, 0, 2))
        bad[3] = 7
        self.assertEqual(p.feed(bytes(bad)), [])
        self.assertGreaterEqual(p.bad_format, 1)

    def test_byte_by_byte(self):
        p = pp.FrameParser()
        out = []
        for b in pp.build_vision(3, 9, 0, 5):
            out += p.feed(bytes([b]))
        self.assertEqual(len(out), 1)
        self.assertEqual(out[0].count, 5)

    def test_two_frames_in_one_read(self):
        p = pp.FrameParser()
        out = p.feed(pp.build_vision(1, 1, 0, 1) + pp.build_permission(2, 1, 1, 10))
        self.assertEqual(len(out), 2)


if __name__ == "__main__":
    unittest.main()
