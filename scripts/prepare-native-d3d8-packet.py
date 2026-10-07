#!/usr/bin/env python3
"""Freeze exact committed DX8 sources and verbatim legacy headers; no target I/O."""
import argparse
import gzip
import hashlib
import io
import json
import subprocess
import tarfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--source", required=True)
parser.add_argument("--legacy", type=Path, required=True)
parser.add_argument("--out", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", args.source]).decode().strip()
args.out.mkdir(parents=True, exist_ok=False)
files = ["src/umd/umd_d3d8_compat.h", "src/umd/umd_d3d8_compat.cpp", "src/umd/umd_runtime_imports.h",
         "tests/umd-d3d8-api.h", "tests/umd-d3d8-compat.cpp", "tests/umd-d3d8-sdk.cpp",
         "tests/umd-d3d8-runtime-probe.cpp", "tests/umd-runtime-imports.cpp",
         "scripts/build-native-d3d8-cpu.ps1", "scripts/verify-native-d3d8-offscreen.py"]
members = {}
rows = []
for name in files:
    data = subprocess.check_output(["git", "-C", str(root), "show", f"{commit}:{name}"])
    assert (root / name).read_bytes() == data, f"worktree drift {name}"
    members[name] = data
    rows.append({"path": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(), "source_commit": commit})
for name in ("d3d8.h", "d3d8caps.h", "d3d8types.h"):
    source = args.legacy / name
    data = source.read_bytes()
    target = "dependencies/legacy-d3d8/" + name
    members[target] = data
    rows.append({"path": target, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                 "source_path": str(source), "provenance": "verbatim Wine/MinGW legacy header, original license preserved"})
stream = io.BytesIO()
with tarfile.open(fileobj=stream, mode="w") as archive:
    for name, data in sorted(members.items()):
        info = tarfile.TarInfo(name); info.size = len(data); info.mode = 0o644; info.mtime = 0
        archive.addfile(info, io.BytesIO(data))
packet = gzip.compress(stream.getvalue(), mtime=0)
(args.out / "native-d3d8-x86-cpu-source-01.tar.gz").write_bytes(packet)
manifest = {"source_commit": commit, "archive_sha256": hashlib.sha256(packet).hexdigest(),
            "primary_git_inputs": len(files), "legacy_dependencies": 3, "inputs": rows,
            "target_execution": "deferred", "target_arch": "x86", "fixture_objects": 5, "fixture_exes": 3,
            "hardware_payload_requirement": "matching x86 core plus Vulkan loader and Mesa ICD; ARM64 candidates cannot be loaded by system8"}
(args.out / "native-d3d8-x86-cpu-source-01.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps({"packet": str(args.out), "sha256": manifest["archive_sha256"], "inputs": len(rows), "target_execution": "deferred"}))
