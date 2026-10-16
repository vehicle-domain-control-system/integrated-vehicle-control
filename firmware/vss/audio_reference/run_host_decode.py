"""Reproduce the isolated real-asset decode check; no target SDK is used."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
TEST = HERE.parent / "tests" / "test_audio_decode.c"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=HERE / "host_evidence")
    parser.add_argument("--compiler", default="gcc")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which(args.compiler)
    if compiler is None:
        raise SystemExit("Requested host compiler unavailable: " + args.compiler)
    manifest = json.loads((HERE / "PROVENANCE.json").read_text(encoding="utf-8"))
    for entry in manifest["files"]:
        data = (HERE / entry["path"]).read_bytes()
        if len(data) != entry["current_size"] or hashlib.sha256(data).hexdigest() != entry["current_sha256"]:
            raise SystemExit("Reference manifest mismatch: " + entry["path"])
    (out / "provenance_verification.json").write_text(json.dumps({
        "verified_files": len(manifest["files"]), "status": "PASS",
        "original_zip_sha256": manifest["original_zip_sha256"],
        "exact_copies": manifest["exact_copies"], "modified_copies": manifest["modified_copies"]
    }, indent=2) + "\n", encoding="utf-8")
    records = []

    def run(label, command, required=True):
        result = subprocess.run(command, text=True, capture_output=True, timeout=120)
        record = dict(label=label, command=command, returncode=result.returncode,
                      stdout=result.stdout, stderr=result.stderr, required=required)
        records.append(record)
        (out / (label + ".json")).write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        (out / (label + ".txt")).write_text(result.stdout + result.stderr, encoding="utf-8")
        print(label, result.returncode)
        if required and result.returncode:
            (out / "results.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
            raise SystemExit(result.returncode)
        return result

    run("compiler", [compiler, "--version"])
    flags = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-Wpedantic",
             "-Wconversion", "-Wshadow", "-Wstrict-prototypes", "-Wmissing-prototypes",
             "-DVSS_AUDIO_REFERENCE_HOST", "-I" + str(HERE / "host_shim"), "-I" + str(HERE)]
    # Retain evidence of why the pinned vendor implementation alone needs an exception.
    run("vendor_strict_probe", [compiler, *flags, "-O0", "-c", str(HERE / "Minimp3Port.c"),
                               "-o", str(out / "vendor_strict_probe.o")], required=False)
    for level in ("O0", "O2"):
        objects = []
        sources = [HERE / "AudioAsset.c", HERE / "Mp3PcmProvider.c", HERE / "Minimp3Port.c", TEST]
        for source in sources:
            obj = out / (source.stem + "_" + level + ".o")
            local_flags = flags.copy()
            if source.name == "Minimp3Port.c":
                local_flags += ["-Wno-conversion", "-Wno-sign-conversion"]
            run("build_" + source.stem + "_" + level,
                [compiler, *local_flags, "-" + level, "-c", str(source), "-o", str(obj)])
            objects.append(str(obj))
        exe = out / ("test_audio_decode_" + level + (".exe" if sys.platform == "win32" else ""))
        run("link_" + level, [compiler, *objects, "-o", str(exe)])
        result = run("run_" + level, [str(exe)])
        print(result.stdout, end="")
    (out / "results.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
