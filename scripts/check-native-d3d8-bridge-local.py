#!/usr/bin/env python3
"""Compile/link the real typed8/9 adapter/device fixtures; no PE execution."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


parser = argparse.ArgumentParser()
parser.add_argument("--llvm", type=Path, required=True)
parser.add_argument("--sdk-shared", type=Path, required=True)
parser.add_argument("--sdk-um", type=Path, required=True)
parser.add_argument("--wdk-shared", type=Path, required=True)
parser.add_argument("--wdk-um", type=Path, required=True)
parser.add_argument("--out", type=Path, required=True)
parser.add_argument("--renderer", action="store_true", help="also compile the actual private parent/backend and SM1 fixture")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
args.out.mkdir(parents=True, exist_ok=False)
commands = []
outputs = []
dependencies = {}
sources = ["tests/umd-d3d9-adapter.cpp", "tests/umd-d3d9-device.cpp",
           "src/umd/umd_d3d9_adapter.cpp", "src/umd/umd_d3d9_device.cpp",
           "src/umd/umd_runtime_query.cpp", "src/umd/umd_runtime_gpu.cpp",
           "src/umd/umd_allocation.cpp", "src/umd/umd_d3d8_compat.cpp"]
renderer_sources = ["src/umd/umd_d3d9_backend.cpp", "src/d3d9/d3d9_interface.cpp",
                    "tests/umd-d3d8-sm1.cpp"] if args.renderer else []
# Include the headers which carry immutable identity and bytecode contracts.
primary = sources + renderer_sources + [str(path.relative_to(root)) for path in sorted((root / "src/umd").glob("*.h"))]
primary += ["src/d3d9/d3d9_caps.h", "src/d3d9/d3d9_shader_code.h"]
before = {name: sha(root / name) for name in primary}


def run(name, command):
    command = [str(value) for value in command]
    result = subprocess.run(command, cwd=root, capture_output=True)
    (args.out / f"{name}.stdout.txt").write_bytes(result.stdout)
    (args.out / f"{name}.stderr.txt").write_bytes(result.stderr)
    commands.append({"name": name, "command": command, "exit": result.returncode})
    (args.out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    assert result.returncode == 0, f"{name}: {result.stderr.decode(errors='replace')}"
    return result.stdout.decode(errors="replace")


for target, machine in (("i686", "IMAGE_FILE_MACHINE_I386"),
                        ("x86_64", "IMAGE_FILE_MACHINE_AMD64"),
                        ("aarch64", "IMAGE_FILE_MACHINE_ARM64")):
    compiler = args.llvm / "bin" / f"{target}-w64-mingw32-clang++"
    common = [compiler, "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror", "-fms-extensions",
              "-isystem", args.wdk_um, "-idirafter", args.sdk_shared,
              "-idirafter", args.sdk_um, "-idirafter", args.wdk_shared]
    objects = {}
    for source in sources + renderer_sources:
        obj = args.out / f"{target}-{Path(source).stem}.obj"
        dep = obj.with_suffix(".d")
        extra = []
        if source in sources[:2]:
            extra += ["-Wshadow"]
        if source in renderer_sources:
            # Exact existing Meson header-warning policy for private DXVK core.
            # The typed adapter/device units above use unsuppressed Werror.
            extra += ["-Wno-missing-field-initializers", "-Wno-unused-parameter",
                     "-Wno-misleading-indentation", "-Wno-cast-function-type",
                     "-Wno-unused-private-field", "-Wno-microsoft-exception-spec",
                     "-Wno-extern-c-compat", "-Wno-unused-const-variable", "-Wno-missing-braces",
                     "-DNOMINMAX", "-DDXVK_WSI_WIN32", "-I", root / "include",
                     "-I", root / "include/vulkan/include", "-I", root / "include/spirv/include",
                     "-I", root / "subprojects/dxbc-spirv"]
        run(obj.stem, common + extra + ["-MMD", "-MF", dep, "-c", root / source, "-o", obj])
        objects[source] = obj
        # -MMD records project and non-system dependencies. SDK roots and
        # their decisive DDI headers are also pinned separately below.
        raw = dep.read_text().replace("\\\n", " ")
        for name in raw.split(":", 1)[1].split():
            path = Path(name).resolve()
            dependencies[str(path)] = sha(path)
        header = run(obj.stem + "-coff", ["llvm-readobj", "--file-headers", obj])
        assert machine in header
        outputs.append({"path": str(obj), "sha256": sha(obj), "bytes": obj.stat().st_size,
                        "machine": machine, "kind": "COFF"})
    for label, members in (
        ("adapter", [sources[0], sources[2], sources[4], sources[7]]),
        ("device", [sources[1], *sources[2:]]),
    ):
        exe = args.out / f"{target}-{label}.exe"
        run(f"{target}-{label}-link", [compiler, "-static", *[objects[name] for name in members],
                                        "-lgdi32", "-o", exe])
        header = run(f"{target}-{label}-pe", ["llvm-readobj", "--file-headers", "--coff-imports", exe])
        assert machine in header
        assert "viogpudxvk" not in header.lower() and "Name: d3d9.dll" not in header.lower()
        outputs.append({"path": str(exe), "sha256": sha(exe), "bytes": exe.stat().st_size,
                        "machine": machine, "kind": "PE"})

sdk_headers = {}
for base, names in ((args.wdk_um, ["d3dumddi.h", "d3d10umddi.h"]),
                    (args.wdk_shared, ["d3dkmddi.h"]),
                    (args.sdk_shared, ["d3d9.h", "d3d9caps.h", "d3d9types.h", "d3dkmthk.h"]),
                    (args.sdk_um, ["dxmini.h"])):
    for name in names:
        path = base / name
        sdk_headers[str(path)] = sha(path)
after = {name: sha(root / name) for name in primary}
assert before == after
assert all(sha(Path(path)) == digest for path, digest in dependencies.items())
proof = {"scope": "strict MinGW cross compilation/link; not native MSVC fixture or GPU execution",
         "source_before": before, "source_after": after, "dependencies": dependencies,
         "sdk_headers": sdk_headers, "outputs": outputs, "cross_objects": 3*(8+len(renderer_sources)), "cross_exes": 6,
         "pe_executions": 0, "target_operations": 0,
         "compiler_version": run("cross-compiler-version", [args.llvm / "bin/i686-w64-mingw32-clang++", "--version"])}
(args.out / "verified.json").write_text(json.dumps(proof, indent=2) + "\n")
print(json.dumps({"proof": str(args.out / "verified.json"), "cross_objects": proof["cross_objects"], "cross_exes": 6,
                  "target_operations": 0, "status": "PASS"}))
