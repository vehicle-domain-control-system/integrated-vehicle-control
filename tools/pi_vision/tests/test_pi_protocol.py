import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import pi_protocol as pp  # noqa: E402

RESULT = pp.VisionResult(0x11223344, 0x01020304, 0x0506, pp.PRESENT, pp.VALID, 0, 3, pp.VALID, 0,
                         pp.STATE_ACTIVE, 0)
STATUS = pp.VisionStatus(9, 77, pp.STATE_FAULT, 2, 3, 400, 0x03)
PERM = pp.Permission(1, 1, pp.PERMISSION_UNKNOWN, pp.QUALITY_NO_DATA, pp.REASON_NO_DATA, 1500, 0xFFFF)


class CrcTest(unittest.TestCase):
    def test_check_value(self):
        self.assertEqual(pp.crc16_ccitt_false(b"123456789"), 0x29B1)


class BuildTest(unittest.TestCase):
    def test_result_layout(self):
        f = pp.build_vision_result(0x0A0B, RESULT)
        self.assertEqual(len(f), 29)
        self.assertEqual(f[:8], bytes([0xA5, 0x5A, 1, 0x60, 19, 0, 0x0B, 0x0A]))
        self.assertEqual(f[8:12], bytes([0x44, 0x33, 0x22, 0x11]))              # little-endian
        self.assertEqual(f[12:16], bytes([0x04, 0x03, 0x02, 0x01]))
        self.assertEqual(f[16:18], bytes([0x06, 0x05]))
        crc = pp.crc16_ccitt_false(f[2:27])
        self.assertEqual(f[27:], bytes([crc & 0xFF, crc >> 8]))

    def test_status_and_permission_length(self):
        self.assertEqual(len(pp.build_vision_status(1, STATUS)), 26)
        p = pp.build_permission(0x0102, PERM)
        self.assertEqual(len(p), 25)
        self.assertEqual(p[3], 0x62)
        self.assertEqual(p[19:21], bytes([0xDC, 0x05]))                         # 1500


class ParserTest(unittest.TestCase):
    def cis_parser(self):
        return pp.FrameParser(accept=(pp.TYPE_VISION_RESULT, pp.TYPE_VISION_STATUS))

    def test_result_roundtrip(self):
        out = self.cis_parser().feed(pp.build_vision_result(5, RESULT))
        self.assertEqual(out, [pp.Frame(5, RESULT)])

    def test_status_roundtrip(self):
        out = self.cis_parser().feed(pp.build_vision_status(2, STATUS))
        self.assertEqual(out, [pp.Frame(2, STATUS)])

    def test_pi_side_takes_permission(self):
        out = pp.FrameParser().feed(pp.build_permission(3, PERM))
        self.assertEqual(out, [pp.Frame(3, PERM)])

    def test_direction_is_checked(self):
        p = pp.FrameParser()                                                    # accepts only 0x62
        self.assertEqual(p.feed(pp.build_vision_result(1, RESULT)), [])
        self.assertGreaterEqual(p.bad_format, 1)

    def test_garbage_before_frame(self):
        out = self.cis_parser().feed(bytes([0, 0xA5, 0xA5, 0x11, 0x5A, 0xFF]) + pp.build_vision_result(1, RESULT))
        self.assertEqual(len(out), 1)

    def test_crc_error_skipped_next_frame_ok(self):
        p = self.cis_parser()
        bad = bytearray(pp.build_vision_result(1, RESULT))
        bad[21] ^= 1
        out = p.feed(bytes(bad) + pp.build_vision_status(2, STATUS))
        self.assertEqual([type(f.message) for f in out], [pp.VisionStatus])
        self.assertGreaterEqual(p.bad_crc, 1)

    def test_wrong_version_and_length(self):
        p = self.cis_parser()
        a = bytearray(pp.build_vision_result(1, RESULT))
        a[2] = 2
        b = bytearray(pp.build_vision_result(1, RESULT))
        b[4] = 18
        self.assertEqual(p.feed(bytes(a) + bytes(b)), [])
        self.assertGreaterEqual(p.bad_format, 2)

    def test_byte_by_byte(self):
        p = self.cis_parser()
        out = []
        for i, b in enumerate(pp.build_vision_result(1, RESULT)):
            out += p.feed(bytes([b]), now=i * 0.0001)
        self.assertEqual(len(out), 1)

    def test_assembly_timeout(self):
        p = self.cis_parser()
        f = pp.build_vision_result(1, RESULT)
        p.feed(f[:10], now=1.000)
        self.assertEqual(p.feed(b"", now=1.040), [])
        self.assertEqual(p.assembly_timeouts, 0)
        p.feed(b"", now=1.051)
        self.assertEqual(p.assembly_timeouts, 1)
        self.assertEqual(p.feed(f[10:], now=1.052), [])                         # the late rest is no frame
        self.assertEqual(len(p.feed(f, now=1.100)), 1)

    def test_two_frames_in_one_read(self):
        out = self.cis_parser().feed(pp.build_vision_result(1, RESULT) + pp.build_vision_status(2, STATUS))
        self.assertEqual(len(out), 2)


if __name__ == "__main__":
    unittest.main()
