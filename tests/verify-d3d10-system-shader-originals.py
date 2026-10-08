#!/usr/bin/env python3
"""Check retained SM4/4.1 programs and every integer/depth readback word."""
import argparse
import hashlib
import json
import re
import struct
import tempfile
from pathlib import Path

SOURCE_SHA256 = "f1cf6d5a6f3177e437d4ec3426f1242753f4abed8db0a32eb8a14cebea6976a2"


def shader_cases():
    index = 0
    for model in (40, 41):
        for geometry in (0, 1):
            for entry, stage in (("vs", 1), ("gs", 2), ("ps", 0)):
                index += 1
                yield f"system-fxc-{index:02d}-{entry}-{entry}_4_{model % 10}", stage, model


def shader_chunk(binary, stage, model):
    if len(binary) < 36 or binary[:4] != b"DXBC" or struct.unpack_from("<I", binary, 24)[0] != len(binary):
        raise AssertionError("Original DXBC extent")
    count = struct.unpack_from("<I", binary, 28)[0]
    if not 1 <= count <= 32 or 32 + 4 * count > len(binary):
        raise AssertionError("Original DXBC chunk table")
    ranges, found = [], []
    for offset in struct.unpack_from("<" + "I" * count, binary, 32):
        if offset < 32 + 4 * count or offset + 8 > len(binary):
            raise AssertionError("Original DXBC chunk offset")
        length = struct.unpack_from("<I", binary, offset + 4)[0]
        end = offset + 8 + length
        if end > len(binary) or any(offset < right and left < end for left, right in ranges):
            raise AssertionError("Original DXBC chunk extent or overlap")
        ranges.append((offset, end))
        if binary[offset:offset + 4] in (b"SHDR", b"SHEX"):
            code = binary[offset + 8:end]
            if length < 12 or length % 4:
                raise AssertionError("Original token extent")
            version, words = struct.unpack_from("<2I", code)
            if version != (stage << 16) | (0x40 if model == 40 else 0x41) or 4 * words != len(code):
                raise AssertionError("Original SM4 stage/version/word count")
            found.append(code)
    if len(found) != 1:
        raise AssertionError("Exactly one original code chunk is required")
    return found[0]


def expected_image(image, geometry, front):
    expected = []
    for y in range(16):
        for x in range(16):
            if image == "depth":
                expected.append(0x3F800000 if x < 8 else 0x3E800000)
            elif x < 8:
                expected.extend((0, 0, 0, 0))
            elif image == "uint":
                expected.extend((0x11223340 + x // 4, 44 if geometry else 7, front, 0x7FC01234))
            else:
                expected.extend(((-(0x11223340 + x // 4)) & 0xFFFFFFFF, 0xFFFFFFF9, 99, 0xFFFFFFFF))
    return expected


def verify(directory):
    directory = Path(directory)
    wanted, originals, front_faces = set(), [], {}
    checks = observations = 0

    def load(name):
        nonlocal checks
        path = directory / name
        if path.is_symlink() or not path.is_file():
            raise AssertionError("Missing or nonregular original: " + name)
        raw = path.read_bytes()
        wanted.add(name)
        checks += 1
        originals.append(dict(path=str(path.resolve()), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        return raw

    if hashlib.sha256(load("system-shaders-original.hlsl")).hexdigest() != SOURCE_SHA256:
        raise AssertionError("Original source differs from the fixed shader oracle")
    checks += 1
    for prefix, stage, model in shader_cases():
        if shader_chunk(load(prefix + ".dxbc"), stage, model) != load(prefix + ".tokens"):
            raise AssertionError("Retained tokens differ from original FXC container: " + prefix)
        checks += 1
    for model in (40, 41):
        for geometry in (0, 1):
            images = {}
            for image in ("uint", "sint", "depth"):
                for role in ("native", "public"):
                    name = f"system-model{model}-gs{geometry}-{image}-{role}.bin"
                    raw = load(name)
                    words = 256 * (1 if image == "depth" else 4)
                    if len(raw) != words * 4:
                        raise AssertionError("Original readback extent: " + name)
                    images[image, role] = struct.unpack("<" + "I" * words, raw)
            front = images["uint", "native"][8 * 4 + 2]
            if front not in (0, 1):
                raise AssertionError("Front-face output must be boolean")
            front_faces[model, geometry] = front
            if model == 41 and front == front_faces[40, geometry]:
                raise AssertionError("FrontCounterClockwise toggle did not invert IsFrontFace")
            for image in ("uint", "sint", "depth"):
                expected = expected_image(image, geometry, front)
                for role in ("native", "public"):
                    name = f"system-model{model}-gs{geometry}-{image}-{role}.bin"
                    for word, (actual, oracle) in enumerate(zip(images[image, role], expected)):
                        checks += 1
                        observations += 1
                        if actual != oracle:
                            raise AssertionError(f"{name} word{word}: {actual:08x} != {oracle:08x}")
    if {p.name for p in directory.glob("system-*")} != wanted:
        raise AssertionError("Exact original output closure")
    if len(originals) != 49 or observations != 18432:
        raise AssertionError("Independent frame/program counts")
    return dict(passed=True, scope="original SM4/4.1 tokens and integer/depth/clip/instance/primitive/front-face readbacks",
                checks=checks, scenes=4, pixels=1024, original_frames=24, fxc_programs=12,
                words_per_role=9216, observations=observations, original_files=originals,
                hardware_admission=False, registration=False,
                compiler_provenance_scope="caller must separately join actual source/compiler/process originals")


def selftest(root):
    """Synthetic reader controls only; this never executes or attests a shader."""
    controls = 0
    with tempfile.TemporaryDirectory(prefix="shader10-reader-", dir=root) as temporary:
        directory = Path(temporary)
        fixture = Path(__file__).with_name("umd-d3d10-system-shaders.cpp").read_text()
        source = re.search(r'static constexpr char source\[\]=R"\((.*?)\)";', fixture, re.S).group(1).encode()
        (directory / "system-shaders-original.hlsl").write_bytes(source)
        for prefix, stage, model in shader_cases():
            code = struct.pack("<3I", (stage << 16) | (0x40 if model == 40 else 0x41), 3, 62 | (1 << 24))
            binary = b"DXBC" + bytes(16) + struct.pack("<4I", 1, 44 + len(code), 1, 36) + b"SHDR" + struct.pack("<I", len(code)) + code
            (directory / (prefix + ".dxbc")).write_bytes(binary)
            (directory / (prefix + ".tokens")).write_bytes(code)
        for model in (40, 41):
            for geometry in (0, 1):
                for image in ("uint", "sint", "depth"):
                    expected = expected_image(image, geometry, int(model == 41))
                    raw = struct.pack("<" + "I" * len(expected), *expected)
                    for role in ("native", "public"):
                        (directory / f"system-model{model}-gs{geometry}-{image}-{role}.bin").write_bytes(raw)
        verify(directory)
        controls += 1
        mutations = [
            ("system-model40-gs0-uint-native.bin", 0, 1),  # clipped region
            ("system-model40-gs0-uint-public.bin", 8 * 16, 0x11223343),  # instance
            ("system-model40-gs1-uint-native.bin", 8 * 16 + 4, 7),  # generated GS primitive
            ("system-model40-gs0-uint-native.bin", 8 * 16 + 12, 0x7FC00000),  # raw integer NaN-like bits
            ("system-model40-gs0-sint-native.bin", 8 * 16, 0x11223342),  # sign
            ("system-model41-gs0-uint-public.bin", 8 * 16 + 8, 0),  # face/public mismatch
            ("system-model40-gs0-depth-native.bin", 8 * 4, 0x3F800000),  # written depth
            ("system-fxc-01-vs-vs_4_0.tokens", 0, 0x10050),  # retained token mismatch
            ("system-fxc-01-vs-vs_4_0.dxbc", 44, 0x10050),  # SM5 container
            ("system-fxc-01-vs-vs_4_0.dxbc", 48, 4),  # token count
        ]
        for name, offset, value in mutations:
            path = directory / name
            original = path.read_bytes()
            changed = bytearray(original)
            struct.pack_into("<I", changed, offset, value)
            path.write_bytes(changed)
            try:
                verify(directory)
            except AssertionError:
                controls += 1
            else:
                raise AssertionError("Mutation accepted: " + name)
            finally:
                path.write_bytes(original)
        for name, action in (("system-extra.bin", "extra"), ("system-model40-gs0-depth-native.bin", "truncate"),
                             ("system-model40-gs0-depth-native.bin", "symlink")):
            path = directory / name
            original = path.read_bytes() if path.exists() else None
            if action == "extra":
                path.write_bytes(b"extra")
            elif action == "truncate":
                path.write_bytes(original[:-4])
            else:
                path.unlink()
                path.symlink_to(directory / "system-model40-gs0-depth-public.bin")
            try:
                verify(directory)
            except AssertionError:
                controls += 1
            else:
                raise AssertionError("Closure mutation accepted: " + action)
            finally:
                path.unlink()
                if original is not None:
                    path.write_bytes(original)
        verify(directory)
        controls += 1
    return dict(passed=True, scope="synthetic reader logic selftest", controls=controls,
                native_execution=False, compiler_execution=False, hardware_admission=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--directory", type=Path)
    mode.add_argument("--selftest-root", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = verify(args.directory) if args.directory else selftest(args.selftest_root)
    if args.output:
        with args.output.open("x") as output:
            json.dump(result, output, indent=2)
    print(json.dumps({key: value for key, value in result.items() if key != "original_files"}))


if __name__ == "__main__":
    main()
