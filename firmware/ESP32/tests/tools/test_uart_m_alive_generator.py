#!/usr/bin/env python3
"""Independent Host checks for the UART-M Alive generator."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

import uart_m_alive_generator as generator  # noqa: E402


class AliveGeneratorTest(unittest.TestCase):
    def test_crc_known_vector(self) -> None:
        self.assertEqual(generator.crc16_ccitt_false(b"123456789"), 0x29B1)

    def test_frame_matches_independent_c_golden_vector(self) -> None:
        # Independently fixed in tests/uart_transport/test_uart_m_transport.c.
        expected = bytes(
            (
                0xA5, 0x5A, 0x02, 0x02, 0x0C, 0x00, 0xEF, 0xBE,
                0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
                0x99, 0x00, 0x01, 0x00, 0x1D, 0xE1,
            )
        )
        actual = generator.alive_frame(
            boot_id=0x11223344,
            update_sequence=0x55667788,
            source_age_ms=0x0099,
            domain_state=1,
            quality=0,
            link_sequence=0xBEEF,
        )
        self.assertEqual(actual, expected)

    def test_periodic_sequence_wrap_and_single_boot_switch(self) -> None:
        frames = list(
            generator.periodic_frames(
                boot_id=1,
                next_boot_id=2,
                update_sequence=0xFFFFFFFE,
                source_age_ms=0,
                domain_state=1,
                quality=0,
                link_sequence=0xFFFE,
                corrupt_crc=False,
                count=4,
            )
        )

        self.assertEqual([item[1] for item in frames], [1, 1, 2, 2])
        self.assertEqual([item[2] for item in frames], [0xFFFE, 0xFFFF, 0, 1])
        self.assertEqual(len(frames), 4)
        self.assertTrue(all(len(item[3]) == 22 for item in frames))

    def test_crc_error_changes_only_crc_bytes(self) -> None:
        good = generator.alive_frame(1, 2, 3, 0, 3, 4)
        bad = generator.alive_frame(1, 2, 3, 0, 3, 4, corrupt_crc=True)

        self.assertEqual(good[:-2], bad[:-2])
        self.assertNotEqual(good[-2:], bad[-2:])

    def test_periodic_sender_writes_complete_frames(self) -> None:
        class FakeSerial:
            def __init__(self) -> None:
                self.frames: list[bytes] = []
                self.flush_count = 0

            def __enter__(self) -> "FakeSerial":
                return self

            def __exit__(self, *_args: object) -> None:
                return None

            def write(self, frame: bytes) -> int:
                self.frames.append(frame)
                return len(frame)

            def flush(self) -> None:
                self.flush_count += 1

        fake_serial = FakeSerial()
        args = SimpleNamespace(
            port="MOCK",
            baud_rate=115200,
            period_ms=250,
            count=3,
            boot_id=1,
            next_boot_id=None,
            update_sequence=7,
            source_age_ms=0,
            domain_state=1,
            quality=0,
            link_sequence=0xFFFE,
            crc_error=False,
        )
        with (
            mock.patch.object(generator, "open_serial_port", return_value=fake_serial),
            mock.patch.object(generator.time, "monotonic", return_value=0.0),
            mock.patch.object(generator.time, "sleep") as sleep_mock,
            mock.patch("builtins.print"),
        ):
            self.assertEqual(generator.send_periodic(args), 0)

        self.assertEqual(len(fake_serial.frames), 3)
        self.assertEqual(fake_serial.flush_count, 3)
        self.assertEqual(
            [int.from_bytes(frame[6:8], "little") for frame in fake_serial.frames],
            [0xFFFE, 0xFFFF, 0],
        )
        self.assertEqual(sleep_mock.call_count, 2)


if __name__ == "__main__":
    unittest.main()
