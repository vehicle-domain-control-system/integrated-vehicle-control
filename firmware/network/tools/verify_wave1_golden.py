#!/usr/bin/env python3
"""Read-only verification of the immutable Wave 1 golden evidence.

Default and --check compare the authenticated fixture with an independent
stdlib struct/binascii reference and all nine fixed C literal vectors. They
never write inputs, reports, caches, or normalized JSON. Generation is not
supported. Use --network after moving this standalone project.

The fixture content is the one archived in SOURCE_WAVE1_RESULT_20261017_v1.0.zip,
whose bytes end their lines with CRLF. Git stores it with LF, so a checkout has
either line end. The check reads it with CRLF turned into LF and compares the
SHA-256 of that content written with CRLF line ends (FIXTURE_SHA256, the
archived bytes): either checkout passes, any other change (a bare CR included)
is rejected, and the file is never modified. The C source SHAs are taken the
same way, with LF line ends. No production codec is imported or invoked.
"""
from __future__ import annotations

import sys

sys.dont_write_bytecode = True

import argparse
import binascii
import hashlib
import json
from pathlib import Path
import re
import struct


HERE = Path(__file__).resolve().parent
FIXTURE_SHA256 = "4c979747887b76a0aed9a295309cb050650ea83da145b9d321569d8f475bd07e"
FIXTURE = HERE / "fixtures" / "wave1_golden.json"


def lf_bytes(raw: bytes) -> bytes:
    # The same content in a CRLF (Windows) or LF checkout: CRLF read as LF.
    return raw.replace(b"\r\n", b"\n")


def require(condition: bool, message: str):
    # Unconditional checks remain active when Python is invoked with -O.
    if not condition:
        raise ValueError(message)

CAN_TEST = "tests/host/test_wire_can.c"
DTO_TEST = "tests/host/test_wire_uart_dto.c"
STREAM_TEST = "tests/host/test_wire_stream.c"
CAN_DOC = "docs/network/interface/03_COMMUNICATION/10_CAN_FD.md"
UART_C_DOC = "docs/network/interface/03_COMMUNICATION/30_UART_C.md"
UART_M_DOC = "docs/network/interface/03_COMMUNICATION/20_UART_M.md"


def literal(network: Path, source: str, name: str) -> bytes:
    text = (network / source).read_text(encoding="utf-8")
    match = re.search(
        r"static\s+const\s+uint8_t\s+" + re.escape(name)
        + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\}\s*;", text, re.S
    )
    if match is None:
        raise ValueError(f"Missing fixed C array: {source}:{name}")
    body = re.sub(r"/\*.*?\*/|//[^\n]*", "", match.group(1), flags=re.S)
    return bytes(int(token.rstrip("uUlL"), 0) for token in
                 re.findall(r"0[xX][0-9a-fA-F]+[uUlL]*|\b[0-9]+[uUlL]*", body))


def packed_fields(fields: list[tuple[str, str, object]], base: int):
    data = bytearray()
    descriptions = []
    for name, fmt, value in fields:
        raw = struct.pack("<" + fmt, value)
        descriptions.append({
            "field": name, "payload_offset": len(data),
            "frame_offset": base + len(data), "width_bytes": len(raw),
            "struct_format": "<" + fmt,
            "input_value": value.hex() if isinstance(value, bytes) else value,
            "packed_hex": raw.hex(),
        })
        data.extend(raw)
    return bytes(data), descriptions


def uart_frame(version: int, type_id: int, sequence: int, payload: bytes):
    protected = struct.pack("<BBHH", version, type_id, len(payload), sequence) + payload
    crc = binascii.crc_hqx(protected, 0xffff)
    frame = b"\xa5\x5a" + protected + struct.pack("<H", crc)
    return frame, {
        "algorithm": "CRC-16/CCITT-FALSE", "polynomial": "0x1021",
        "initial": "0xffff", "refin": False, "refout": False, "xorout": 0,
        "reference": "Python stdlib binascii.crc_hqx(protected_bytes, 0xffff)",
        "window_frame_start_inclusive": 2,
        "window_frame_end_inclusive": len(payload) + 7,
        "protected_length": len(protected), "protected_hex": protected.hex(),
        "numeric_crc": f"0x{crc:04x}", "wire_crc_le_hex": struct.pack("<H", crc).hex(),
        "wire_crc_offset": len(frame) - 2,
        "sync_excluded": True,
    }


def verify(network: Path):
    # Values are transcribed from the fixed input fixtures, not their wire arrays.
    specifications = [
        ("CIS_REAR", "CAN", 0x0a0, 0, CAN_TEST, "rear_golden", CAN_DOC, [
            ("BOOT_ID", "I", 0x01020304), ("TX_SEQUENCE", "I", 0x11223344),
            ("UPDATE_SEQUENCE", "I", 0xa1b2c3d4), ("SOURCE_AGE", "H", 0x1234),
            ("REAR_DISTANCE", "H", 123), ("VALIDITY", "B", 1),
            ("QUALITY_REASON", "B", 0), ("PROXIMITY_STATUS", "B", 0),
        ]),
        ("DOMAIN_PERMISSION", "CAN", 0x080, 0, CAN_TEST, "permission_golden", CAN_DOC, [
            ("BOOT_ID", "I", 0x10203040), ("TX_SEQUENCE", "I", 0x50607080),
            ("VEHICLE_POWER_PERMISSION", "B", 1), ("POWER_PERMISSION_QUALITY", "B", 0),
            ("POWER_OPERATION_STATE", "B", 255), ("OPERATION_ALLOWED", "B", 0),
            ("OPERATION_QUALITY", "B", 3), ("UPDATE_SEQUENCE", "I", 0x89abcdef),
            ("SOURCE_AGE", "H", 0xfedc),
        ]),
        ("P_VISION_RESULT", "UART-C", 0x60, 1, DTO_TEST, "golden_vision", UART_C_DOC, [
            ("PI_BOOT_ID", "I", 0x12345678), ("VISION_SEQUENCE", "I", 0x90abcdef),
            ("VISION_AGE", "H", 0x1234), ("OCCUPANT_PRESENCE", "B", 1),
            ("PRESENCE_VALIDITY", "B", 1), ("PRESENCE_REASON", "B", 0),
            ("OCCUPANT_COUNT", "B", 3), ("COUNT_VALIDITY", "B", 1),
            ("COUNT_REASON", "B", 0), ("VISION_STATUS", "B", 2),
            ("VISION_FAULT_CODE", "H", 0),
        ]),
        ("P_PERMISSION", "UART-C", 0x62, 1, DTO_TEST, "golden_permission", UART_C_DOC, [
            ("CIS_BOOT_ID", "I", 0x12345678), ("UPDATE_SEQUENCE", "I", 0x90abcdef),
            ("VEHICLE_POWER_PERMISSION", "B", 1), ("VALUE_QUALITY", "B", 0),
            ("QUALITY_REASON", "B", 0), ("USE_LIMIT", "H", 1500),
            ("SOURCE_AGE", "H", 499),
        ]),
        ("M_CONTEXT", "UART-M", 0x01, 2, DTO_TEST, "golden_context", UART_M_DOC, [
            ("DEVICE_CONTEXT_ID", "I", 0x12345678), ("SESSION_ID", "Q", 0x0123456789abcdef),
            ("DEVICE_REGISTRATION_STATE", "B", 1), ("BT_CONNECTION_STATE", "B", 1),
            ("APP_ACTIVE_STATE", "B", 0), ("APP_QUALITY", "B", 0),
            ("APP_UPDATE_SEQUENCE", "I", 0x10203040), ("APP_AGE", "H", 0x1234),
            ("PROXIMITY_STATE", "B", 1), ("PROXIMITY_VALIDITY", "B", 1),
            ("PROXIMITY_REASON", "B", 0), ("PROXIMITY_UPDATE_SEQUENCE", "I", 0x50607080),
            ("PROXIMITY_AGE", "H", 0x5678), ("REGISTRATION_CONNECTION_QUALITY", "B", 0),
            ("REGISTRATION_CONNECTION_UPDATE_SEQUENCE", "I", 0x90abcdef),
            ("REGISTRATION_CONNECTION_AGE", "H", 0x9abc),
        ]),
        ("M_REQUEST", "UART-M", 0x10, 2, DTO_TEST, "golden_request", UART_M_DOC, [
            ("DEVICE_CONTEXT_ID", "I", 0x12345678), ("SESSION_ID", "Q", 0x0123456789abcdef),
            ("REQUEST_ID", "I", 0x90abcdef), ("REQUEST_KIND", "B", 2),
            ("OPERATION", "B", 1), ("ARG_0..1_SIGNED_CENTI_C", "h", -1234),
            ("ARG_2..5_RESERVED", "4s", bytes(4)),
        ]),
        ("M_RESULT", "UART-M", 0x20, 2, DTO_TEST, "golden_result", UART_M_DOC, [
            ("DEVICE_CONTEXT_ID", "I", 0x12345678), ("SESSION_ID", "Q", 12),
            ("DOMAIN_BOOT_ID", "I", 0x10203040), ("QUERY_ID", "I", 0x50607080),
            ("REQUEST_SESSION", "Q", 7), ("REQUEST_ID", "I", 0x90abcdef),
            ("RESULT", "B", 2), ("RESULT_REASON", "H", 9),
            ("RESULT_CONFIRMATION", "B", 1), ("COMMAND_TARGET", "B", 1),
            ("COMMAND_ID", "I", 0x11223344), ("ORIGIN", "B", 1),
            ("RESULT_AGE", "H", 0x1234),
        ]),
    ]
    expected_lengths = [19, 19, 19, 15, 38, 24, 44]
    results = []
    for spec, payload_length in zip(specifications, expected_lengths):
        name, link, type_id, version, source, symbol, document, fields = spec
        payload, descriptions = packed_fields(fields, 0 if link == "CAN" else 8)
        require(len(payload) == payload_length, name + ": unexpected reference payload length")
        record = {
            "message": name, "link": link, "id_or_type": f"0x{type_id:03x}" if link == "CAN" else f"0x{type_id:02x}",
            "source_document": "FINAL_98/" + document,
            "test_source": source, "literal_symbol": symbol,
            "classification": "NEW_HOST_TYPED_CANDIDATE_VECTOR_NOT_I1_NOT_PRODUCT_APPROVAL",
            "payload_used_bytes": len(payload), "fields": descriptions,
        }
        if link == "CAN":
            frame = payload + b"\0"
            record.update({"dlc": 11, "standard_11bit": True, "fdf": 1, "brs": 1, "rtr": 0,
                           "padding": {"offset": 19, "length": 1, "hex": "00"},
                           "crc": "NO_APPLICATION_CRC; CAN FD controller CRC is external"})
        else:
            frame, crc_record = uart_frame(version, type_id, 0x5aa5, payload)
            record.update({"version": version, "link_sequence": 0x5aa5,
                           "header_hex": frame[:8].hex(), "crc": crc_record})
        existing = literal(network, source, symbol)
        require(frame == existing, f"Golden mismatch {name}: expected={frame.hex()} literal={existing.hex()}")
        record.update({"frame_bytes": len(frame), "frame_hex": frame.hex(),
                       "frame_sha256": hashlib.sha256(frame).hexdigest(),
                       "matches_fixed_c_literal": True})
        results.append(record)


    structural = []
    for name, version, type_id, payload in [
        ("golden_c", 1, 0x60, b"\xa5\x5a" + bytes(range(2, 19))),
        ("golden_m", 2, 0x01, bytes(range(38))),
    ]:
        frame, crc = uart_frame(version, type_id, 0x1234, payload)
        require(frame == literal(network, STREAM_TEST, name), "Golden mismatch " + name)
        structural.append({
            "literal_symbol": name, "test_source": STREAM_TEST,
            "classification": "EXTRA_STRUCTURAL_FRAMING_ONLY_NOT_TYPED_DTO_VECTOR",
            "payload_generation": "A5 5A then 02..12" if name == "golden_c" else "00..25",
            "payload_hex": payload.hex(), "frame_hex": frame.hex(), "frame_bytes": len(frame),
            "link_sequence": 0x1234, "crc": crc,
            "matches_fixed_c_literal": True,
        })


    require(binascii.crc_hqx(b"123456789", 0xffff) == 0x29b1, "CRC check mismatch")
    require(binascii.crc_hqx(b"", 0xffff) == 0xffff, "Empty CRC check mismatch")
    sources = {source: hashlib.sha256(lf_bytes((network / source).read_bytes())).hexdigest()
               for source in (CAN_TEST, DTO_TEST, STREAM_TEST)}
    return {
        "status": "PASS_INDEPENDENT_HOST_GOLDEN_REFERENCE", "client_date": "2026-10-17",
        "typed_vector_count": 7, "extra_structural_vector_count": 2,
        "historical_I1_vectors": "NOT_USED; original files and SHA were not authenticated",
        "candidate_archive_sha256": "f9d803bfdaf2eab1eb1c56581e550f8046b48a34c43e551ee8060878f8ba4b5c",
        "method": "Handwritten fields -> Python struct.pack little endian -> binascii.crc_hqx; compare fixed C literals. No production codec calls.",
        "uart_header_offsets": {"SYNC": [0, 2], "VERSION": [2, 1], "TYPE": [3, 1],
                                "P_U16LE": [4, 2], "LINK_SEQUENCE_U16LE": [6, 2], "PAYLOAD": [8, "P"]},
        "crc_check_vectors": [{"ascii": "123456789", "crc": "0x29b1", "crc_le_hex": "b129"},
                              {"ascii": "", "crc": "0xffff", "crc_le_hex": "ffff"}],
        "source_sha256": sources, "typed_vectors": results,
        "extra_structural_vectors": structural,
        "deterministic_random_tests": [
            {"test_source": CAN_TEST, "case": "can_randomized", "seed": "0x4319a277",
             "iterations": 2000, "message_round_trips": 4000, "scope": "CAN2 typed candidate"},
            {"test_source": STREAM_TEST, "case": "deterministic_stream_replay", "seed": "0x5eeda731",
             "rounds": 3000, "accepted_frames": 3000, "scope": "candidate structural UART stream"},
            {"test_source": "tests/host/test_uart_random_stress.c", "seed": "0xB5E1",
             "rounds": 500, "frames": 12628, "scope": "preserved v1.1 synthetic fixture, not actual wire CRC"},
        ],
        "limits": ["Candidate structure only; product adoption and owner integration remain HOLD.",
                   "No HAL, physical CAN controller CRC, board, runtime permission or TTL acceptance is proven.",
                   "Random test counts describe C tests; this script verifies literal reference bytes and CRC."],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Explicit read-only check (also the default)")
    parser.add_argument("--network", type=Path, default=HERE.parent,
                        help="Path to the firmware/network project")
    parser.add_argument("--fixture", type=Path, default=FIXTURE,
                        help="Authentic byte-for-byte Wave 1 JSON fixture")
    args = parser.parse_args()
    try:
        raw = lf_bytes(args.fixture.read_bytes())
        actual_sha = hashlib.sha256(raw.replace(b"\n", b"\r\n")).hexdigest()
        require(actual_sha == FIXTURE_SHA256,
                f"Fixture SHA mismatch: expected={FIXTURE_SHA256} actual={actual_sha}")
        frozen = json.loads(raw.decode("utf-8"))
        reference = verify(args.network.resolve())
        require(reference == frozen, "Fixture fields or original C source SHA do not match the independent reference")
    except (OSError, ValueError, UnicodeError, struct.error) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"PASS immutable fixture SHA256 {actual_sha}")
    print("PASS independent fields/lengths/CRC and 7 typed + 2 structural C golden vectors")
    print("PASS original C source SHA; read-only check complete (no writes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
