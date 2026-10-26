#!/usr/bin/env python3
"""Regression for the read-only Wave 1 reference verifier (stdlib only).

All deliberately damaged inputs are disposable copies under firmware/network.
The check tool must leave every input, directory, and existing result untouched.
No previous result ZIP or review directory is needed to run this suite.
"""
from __future__ import annotations

import sys

sys.dont_write_bytecode = True

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import unittest


SOURCES = (
    "tests/host/test_wire_can.c",
    "tests/host/test_wire_uart_dto.c",
    "tests/host/test_wire_stream.c",
)
NETWORK = Path(__file__).resolve().parents[2]
WORK = None
FIXTURE_SHA256 = "4c979747887b76a0aed9a295309cb050650ea83da145b9d321569d8f475bd07e"
# Removing the work folder at the end: on Windows a file of it can still be
# held for a moment after the verifier subprocesses ended (WinError 32,
# another process uses the file). The removal is retried; a folder still held
# after that is reported and left in place (.build* is ignored by Git), and
# the test result stays the result of the eight cases.
CLEANUP_ATTEMPTS = 6
CLEANUP_DELAY_S = 0.25


def sha(data):
    return hashlib.sha256(data).hexdigest()


def tree_snapshot(root):
    return {
        str(path.relative_to(root)): sha(path.read_bytes()) if path.is_file() else "DIRECTORY"
        for path in root.rglob("*")
    }


class ReadOnlyVerifierTests(unittest.TestCase):
    def setUp(self):
        self.case = WORK / self._testMethodName
        self.case.mkdir()
        self.project = self.case / "project"
        self.project.mkdir()
        for name in SOURCES:
            destination = self.project / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes((NETWORK / name).read_bytes())
        self.fixture = self.case / "golden.json"
        self.fixture.write_bytes((NETWORK / "tools/fixtures/wave1_golden.json").read_bytes())
        # A result-like sibling catches the original overwrite defect too.
        (self.case / "04_GOLDEN_VECTORS.json").write_bytes(b"Existing result: do not overwrite.\r\n")

    def invoke(self, extra=(), expected=0, optimize=False):
        before = tree_snapshot(self.case)
        protected = [NETWORK / name for name in SOURCES] + [
            NETWORK / "tools/verify_wave1_golden.py",
            NETWORK / "tools/fixtures/wave1_golden.json",
            Path(__file__).resolve(),
        ]
        original_sha = {str(path): sha(path.read_bytes()) for path in protected}
        command = [sys.executable, "-B"]
        if optimize:
            command.append("-O")
        command += [str(NETWORK / "tools/verify_wave1_golden.py"), *map(str, extra)]
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", PYTHONIOENCODING="utf-8")
        result = subprocess.run(command, cwd=self.case, env=env, capture_output=True,
                                text=True, encoding="utf-8", timeout=20)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        self.assertEqual(tree_snapshot(self.case), before, "Verifier changed a test input or result")
        self.assertEqual({str(path): sha(path.read_bytes()) for path in protected}, original_sha,
                         "Verifier changed the original tool, fixture, test, or C source")
        if expected == 0:
            self.assertIn("7 typed + 2 structural", result.stdout)
            self.assertIn("no writes", result.stdout)
        return result

    def copied_inputs(self):
        return ["--network", self.project, "--fixture", self.fixture]

    def test_default_twice(self):
        self.invoke()
        self.invoke()

    def test_explicit_check_twice(self):
        self.invoke(["--check", *self.copied_inputs()])
        self.invoke(["--check", *self.copied_inputs()])

    def test_newline_variants_preserved(self):
        original = self.fixture.read_bytes()
        self.assertEqual(sha(original), FIXTURE_SHA256)
        self.assertIn(b"\r\n", original)
        self.assertEqual(original.count(b"\n"), original.count(b"\r\n"))
        self.invoke(["--check", *self.copied_inputs()])
        normalized = original.replace(b"\r\n", b"\n")
        self.assertEqual(json.loads(normalized), json.loads(original))
        self.fixture.write_bytes(normalized)
        result = self.invoke(["--check", *self.copied_inputs()], expected=1)
        self.assertIn("Fixture SHA mismatch", result.stderr)
        self.assertEqual(self.fixture.read_bytes(), normalized)

    def test_invalid_fixtures_preserved(self):
        original = self.fixture.read_bytes()
        for damaged in (b"{invalid JSON\n", b"", original.replace(b'"typed_vector_count": 7',
                                                                   b'"typed_vector_count": 8', 1)):
            self.assertNotEqual(damaged, original)
            self.fixture.write_bytes(damaged)
            result = self.invoke(["--check", *self.copied_inputs()], expected=1)
            self.assertIn("Fixture SHA mismatch", result.stderr)

    def test_missing_fixture_does_not_create_file(self):
        missing = self.case / "missing.json"
        self.invoke(["--check", "--network", self.project, "--fixture", missing], expected=1)
        self.assertFalse(missing.exists())

    def test_literal_corruption_rejected_in_normal_and_optimized_python(self):
        source = self.project / SOURCES[0]
        raw = source.read_bytes()
        match = re.search(rb"rear_golden\s*\[[^\]]*\]\s*=\s*\{.*?(0x[0-9a-fA-F]+)", raw, re.S)
        self.assertIsNotNone(match)
        damaged_byte = f"0x{int(match.group(1), 16) ^ 1:02x}".encode("ascii")
        source.write_bytes(raw[:match.start(1)] + damaged_byte + raw[match.end(1):])
        for optimize in (False, True):
            result = self.invoke(["--check", *self.copied_inputs()], expected=1, optimize=optimize)
            self.assertIn("Golden mismatch CIS_REAR", result.stderr)

    def test_source_sha_detects_nonliteral_change(self):
        source = self.project / SOURCES[0]
        source.write_bytes(source.read_bytes() + b"\n/* Change outside the fixed literal. */\n")
        result = self.invoke(["--check", *self.copied_inputs()], expected=1)
        self.assertIn("original C source SHA", result.stderr)

    def test_generation_and_output_options_rejected_without_writes(self):
        destination = self.case / "must_not_be_created.json"
        self.invoke(["--generate", "--out", destination], expected=2)
        self.invoke(["--check", "--out", self.fixture], expected=2)
        self.assertFalse(destination.exists())


def remove_work(temporary):
    """Remove the work folder of this run, retrying while a file is held.

    The first attempt is the normal TemporaryDirectory cleanup; the retries
    remove the same folder (the target checked in main()) with shutil.rmtree,
    which also works where cleanup() does not run twice (Python 3.8, 3.9).
    Returns True when the folder is gone. Never raises OSError: the folder is
    not a test input, so a held file only gives a warning on stderr."""
    path = temporary.name
    error = None
    for attempt in range(CLEANUP_ATTEMPTS):
        if attempt:
            time.sleep(CLEANUP_DELAY_S * attempt)
        try:
            if attempt == 0:
                temporary.cleanup()
            elif os.path.exists(path):
                shutil.rmtree(path)
        except OSError as exc:
            error = exc
        if not os.path.exists(path):
            return True
    print("WARNING: work folder not removed after %d attempts (%s); the test result is "
          "unchanged; delete it by hand: %s" % (CLEANUP_ATTEMPTS, error, path), file=sys.stderr)
    return False


def main():
    global NETWORK, WORK
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--network", type=Path, default=NETWORK)
    args = parser.parse_args()
    NETWORK = args.network.resolve()
    # Verify the resolved deletion target stays a direct child of this project
    # before TemporaryDirectory's automatic recursive cleanup is permitted.
    temporary = tempfile.TemporaryDirectory(prefix=".build-wave1-verifier-", dir=NETWORK)
    WORK = Path(temporary.name).resolve()
    if WORK.parent != NETWORK or not WORK.name.startswith(".build-wave1-verifier-"):
        raise RuntimeError("Unexpected temporary cleanup target")
    try:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(ReadOnlyVerifierTests)
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        return 0 if result.wasSuccessful() else 1
    finally:
        remove_work(temporary)


if __name__ == "__main__":
    raise SystemExit(main())
