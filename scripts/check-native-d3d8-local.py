#!/usr/bin/env python3
"""Strict local cross builds. Never executes a Windows PE or operates a VM."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--llvm", type=Path, required=True)
parser.add_argument("--sdk-shared", type=Path, required=True)
parser.add_argument("--out", type=Path, required=True)
parser.add_argument("--original-mapped", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
args.out.mkdir(parents=True, exist_ok=False)
commands = []


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(name, command):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=root, capture_output=True)
    (args.out / f"{name}.stdout.txt").write_bytes(result.stdout)
    (args.out / f"{name}.stderr.txt").write_bytes(result.stderr)
    commands.append({"name": name, "command": command, "exit": result.returncode})
    (args.out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    assert result.returncode == 0, f"{name}: {result.stderr.decode(errors='replace')}"
    return result.stdout.decode(errors="replace")


sdk_copy = args.out / "sdk-headers"
sdk_copy.mkdir()
headers = []
for name in ("d3d9.h", "d3d9caps.h", "d3d9types.h"):
    source = args.sdk_shared / name
    (sdk_copy / name).write_bytes(source.read_bytes())
    headers.append({"path": str(source), "copy": str(sdk_copy / name), "sha256": sha(source)})
legacy = args.llvm / "generic-w64-mingw32/include"
for name in ("d3d8.h", "d3d8caps.h", "d3d8types.h"):
    headers.append({"path": str(legacy / name), "sha256": sha(legacy / name), "provenance": "Wine/MinGW legacy header"})
headers.append({"path": str(args.sdk_shared / "d3dkmthk.h"), "sha256": sha(args.sdk_shared / "d3dkmthk.h")})
sources = ["tests/umd-d3d8-runtime-probe.cpp", "tests/umd-runtime-imports.cpp",
           "tests/umd-d3d8-compat.cpp", "tests/umd-d3d8-sdk.cpp", "src/umd/umd_d3d8_compat.cpp",
           "tests/umd-d3d8-api.h", "src/umd/umd_d3d8_compat.h", "src/umd/umd_runtime_imports.h"]
before = {name: sha(root / name) for name in sources}
outputs = []
for target, machine in (("i686", "IMAGE_FILE_MACHINE_I386"), ("x86_64", "IMAGE_FILE_MACHINE_AMD64"),
                        ("aarch64", "IMAGE_FILE_MACHINE_ARM64")):
    compiler = args.llvm / "bin" / f"{target}-w64-mingw32-clang++"
    common = [compiler, "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror", "-fms-extensions",
              "-idirafter", args.sdk_shared]
    objects = {}
    for source in sources[:5]:
        obj = args.out / f"{target}-{Path(source).stem}.obj"
        # The original SDK header contains nested comments. Treat unmodified
        # vendor headers as system includes; source warnings remain fatal.
        extra = ["-isystem", sdk_copy] if source in ("tests/umd-d3d8-sdk.cpp", "src/umd/umd_d3d8_compat.cpp") else []
        run(obj.stem, common + extra + ["-c", root / source, "-o", obj])
        objects[source] = obj
    for label, members in (("caps", sources[2:5]), ("imports", [sources[1]]), ("runtime", [sources[0]])):
        exe = args.out / f"{target}-{label}.exe"
        flags = ["-municode", "-luser32"] if label == "runtime" else []
        run(f"{target}-{label}-link", [compiler, "-static", *[objects[name] for name in members], *flags, "-o", exe])
        header = run(f"{target}-{label}-pe", ["llvm-readobj", "--file-headers", "--coff-imports", exe])
        assert machine in header
        if label == "runtime":
            assert "Direct3DCreate8" not in header and "Name: d3d8.dll" not in header.lower()
        outputs.append({"path": str(exe), "bytes": exe.stat().st_size, "sha256": sha(exe), "machine": machine})
    outputs += [{"path": str(obj), "bytes": obj.stat().st_size, "sha256": sha(obj), "machine": machine}
                for obj in objects.values()]
for compiler in ("g++", "clang++"):
    for label, source in (("caps", sources[2]), ("imports", sources[1])):
        exe = args.out / f"{compiler}-{label}"
        run(f"{compiler}-{label}-compile", [compiler, "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror",
          "-fsanitize=address,undefined", "-fno-omit-frame-pointer", root / source, "-o", exe])
        command = [exe] + (["--mapped-image", args.original_mapped] if label == "imports" and args.original_mapped else [])
        run(f"{compiler}-{label}-execute", command)
    # Restore the bug in a disposable, owned copy to prove masks and shader bounds matter.
    bad_header = (root / "src/umd/umd_d3d8_compat.h").read_text().replace(
        "if (local[51]) local[51] = std::min(local[51], 0xffff0104u);", "// negative: preserve SM2 pixel cap")
    bad = args.out / f"{compiler}-negative"; (bad / "src/umd").mkdir(parents=True)
    (bad / "tests").mkdir()
    (bad / "src/umd/umd_d3d8_compat.h").write_text(bad_header)
    (bad / "tests/umd-d3d8-compat.cpp").write_bytes((root / sources[2]).read_bytes())
    run(f"{compiler}-negative-compile", [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                                        bad / "tests/umd-d3d8-compat.cpp", "-o", bad / "negative"])
    result = subprocess.run([bad / "negative"], capture_output=True)
    (bad / "stdout.txt").write_bytes(result.stdout); (bad / "stderr.txt").write_bytes(result.stderr)
    assert result.returncode == 1 and result.stderr.count(b"CHECK failed") == 1
    assert b"guarded[52] == 0xffff0104" in result.stderr
after = {name: sha(root / name) for name in sources}
assert before == after
proof = {"scope": "local GCC/Clang sanitizer and MinGW cross compile/link, not native EWDK or hardware acceptance",
         "source_before": before, "source_after": after, "headers": headers, "outputs": outputs,
         "pe_executions": 0, "target_operations": 0, "cross_objects": 15, "cross_exes": 9,
         "semantic_negative_controls": 2, "compiler_versions": {
             "cross": run("cross-compiler-version", [args.llvm / "bin/i686-w64-mingw32-clang++", "--version"])}}
(args.out / "verified.json").write_text(json.dumps(proof, indent=2) + "\n")
print(json.dumps({"proof": str(args.out / "verified.json"), "cross_objects": 15, "cross_exes": 9,
                  "target_operations": 0, "status": "PASS"}))
