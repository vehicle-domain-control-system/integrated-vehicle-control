#!/usr/bin/env python3
"""Generate or periodically send UART-M v2 M_DOMAIN_ALIVE frames."""

from __future__ import annotations

import argparse
import struct
import time
from pathlib import Path
from typing import Iterator


def bounded_int(bits: int):
    maximum = (1 << bits) - 1

    def parse(value: str) -> int:
        number = int(value, 0)
        if not 0 <= number <= maximum:
            raise argparse.ArgumentTypeError(
                f"value must be in 0..{maximum} (uint{bits})"
            )
        return number

    return parse


def positive_int(value: str) -> int:
    number = int(value, 0)
    if number <= 0:
        raise argparse.ArgumentTypeError("value must be greater than zero")
    return number


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def alive_frame(
    boot_id: int,
    update_sequence: int,
    source_age_ms: int,
    domain_state: int,
    quality: int,
    link_sequence: int,
    corrupt_crc: bool = False,
) -> bytes:
    if boot_id == 0:
        raise ValueError("DomainBootID 0 is invalid in the current codec")
    payload = struct.pack(
        "<IIHBB",
        boot_id,
        update_sequence,
        source_age_ms,
        domain_state,
        quality,
    )
    body = struct.pack("<BBHH", 2, 0x02, len(payload), link_sequence) + payload
    crc = crc16_ccitt_false(body)
    if corrupt_crc:
        crc ^= 0x0001
    return b"\xA5\x5A" + body + struct.pack("<H", crc)


def periodic_frames(
    boot_id: int,
    next_boot_id: int | None,
    update_sequence: int,
    source_age_ms: int,
    domain_state: int,
    quality: int,
    link_sequence: int,
    corrupt_crc: bool,
    count: int,
) -> Iterator[tuple[int, int, int, bytes]]:
    """Yield index, boot ID, link sequence and one finite test frame."""
    switch_index = (count + 1) // 2
    for index in range(count):
        current_boot = (
            next_boot_id
            if next_boot_id is not None and index >= switch_index
            else boot_id
        )
        current_link = (link_sequence + index) & 0xFFFF
        current_update = (update_sequence + index) & 0xFFFFFFFF
        yield (
            index,
            current_boot,
            current_link,
            alive_frame(
                current_boot,
                current_update,
                source_age_ms,
                domain_state,
                quality,
                current_link,
                corrupt_crc,
            ),
        )


def open_serial_port(port: str, baud_rate: int):
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError(
            "pyserial is required only for --port: python -m pip install pyserial"
        ) from error
    try:
        return serial.Serial(
            port=port,
            baudrate=baud_rate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0,
            write_timeout=1.0,
        )
    except (serial.SerialException, ValueError) as error:
        raise RuntimeError(f"cannot open {port}: {error}") from error


def send_periodic(args: argparse.Namespace) -> int:
    interval_s = args.period_ms / 1000.0
    frames = periodic_frames(
        args.boot_id,
        args.next_boot_id,
        args.update_sequence,
        args.source_age_ms,
        args.domain_state,
        args.quality,
        args.link_sequence,
        args.crc_error,
        args.count,
    )
    try:
        with open_serial_port(args.port, args.baud_rate) as serial_port:
            deadline = time.monotonic()
            for index, boot_id, link_sequence, frame in frames:
                written = serial_port.write(frame)
                serial_port.flush()
                if written != len(frame):
                    raise RuntimeError(
                        f"short write on {args.port}: {written}/{len(frame)} bytes"
                    )
                print(
                    f"sent[{index}] boot=0x{boot_id:08X} link={link_sequence} "
                    f"bytes={len(frame)} crc={'BAD' if args.crc_error else 'OK'}"
                )
                deadline += interval_s
                remaining = deadline - time.monotonic()
                if remaining > 0 and index + 1 < args.count:
                    time.sleep(remaining)
    except (OSError, RuntimeError) as error:
        print(f"error: {error}")
        return 2
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Create raw UART-M v2 M_DOMAIN_ALIVE frame(s)."
    )
    parser.add_argument("--boot-id", type=bounded_int(32), default=0x11223344)
    parser.add_argument("--next-boot-id", type=bounded_int(32))
    parser.add_argument("--update-sequence", type=bounded_int(32), default=1)
    parser.add_argument("--source-age-ms", type=bounded_int(16), default=0)
    parser.add_argument("--domain-state", type=bounded_int(8), choices=range(4), default=1)
    parser.add_argument("--quality", type=bounded_int(8), choices=range(4), default=0)
    parser.add_argument("--link-sequence", type=bounded_int(16), default=0)
    parser.add_argument("--crc-error", action="store_true")
    parser.add_argument("--output", type=Path, help="write concatenated raw bytes")
    parser.add_argument("--port", help="send finite frames to this serial port")
    parser.add_argument("--baud-rate", type=positive_int, choices=(115200,), default=115200)
    parser.add_argument("--period-ms", type=positive_int, default=250)
    parser.add_argument("--count", type=positive_int, default=20)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.port is not None:
        if args.output is not None:
            raise SystemExit("--output and --port cannot be used together")
        return send_periodic(args)
    frames = [
        alive_frame(
            args.boot_id,
            args.update_sequence,
            args.source_age_ms,
            args.domain_state,
            args.quality,
            args.link_sequence,
            args.crc_error,
        )
    ]
    if args.next_boot_id is not None:
        frames.append(
            alive_frame(
                args.next_boot_id,
                (args.update_sequence + 1) & 0xFFFFFFFF,
                args.source_age_ms,
                args.domain_state,
                args.quality,
                (args.link_sequence + 1) & 0xFFFF,
                False,
            )
        )
    for index, frame in enumerate(frames):
        print(f"frame[{index}] ({len(frame)}B): {frame.hex(' ').upper()}")
    if args.output is not None:
        args.output.write_bytes(b"".join(frames))
        print(f"wrote {sum(map(len, frames))} bytes to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
