#!/usr/bin/env python3
"""Join original texture UAV outputs to an independent absolute-coordinate oracle."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def float_word(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def cases():
    index = 0
    for dimension in [1, 2]:
        for floating in [False, True]:
            for slices in [1, 3]:
                for mip in range(3):
                    ranges = [(0, 1)] if slices == 1 else [(0, 1), (0, 3), (1, 1), (1, 2), (2, 1)]
                    for first, count in ranges:
                        view_dimension = (2 if dimension == 1 else 4) + (slices > 1)
                        metadata = [dimension, 41 if floating else 42, 8, 1 if dimension == 1 else 4,
                                    3, slices, mip, first, count, view_dimension, view_dimension]
                        stages = {}
                        for cleared in [False, True]:
                            expected = []
                            for absolute_slice in range(slices):
                                for absolute_mip in range(3):
                                    width = 8 // (2 ** absolute_mip)
                                    height = 1 if dimension == 1 else 4 // (2 ** absolute_mip)
                                    for y in range(height):
                                        for x in range(width):
                                            value = ((0x3F000000 if floating else 0x11000000)
                                                     | (absolute_slice << 16) | (absolute_mip << 12)
                                                     | (y << 6) | x)
                                            if absolute_mip == mip and first <= absolute_slice < first + count:
                                                z = absolute_slice - first
                                                if cleared:
                                                    value = float_word(0.375) if floating else 0x2468ACE1
                                                elif floating:
                                                    value = float_word(1024 + x + 16 * y + 256 * z)
                                                else:
                                                    value = 0x50000000 | x | (y << 8) | (z << 16)
                                            expected.append(value)
                            stages["clear" if cleared else "compute"] = expected
                        yield index, metadata, stages
                        index += 1
    if index != 72:
        raise AssertionError("Independent UAV case enumeration")


def shader_cases():
    for dimension in [1, 2]:
        for floating in [False, True]:
            for array in [False, True]:
                index = (dimension - 1) * 4 + int(floating) * 2 + int(array)
                texture = f"RWTexture{dimension}D" + ("Array" if array else "")
                coordinate = ("uint2(id.x,id.z)" if array else "id.x") if dimension == 1 else ("id" if array else "id.xy")
                expression = "float(1024+id.x+16*id.y+256*id.z)" if floating else "0x50000000u|id.x|(id.y<<8)|(id.z<<16)"
                source = (texture + "<" + ("float" if floating else "uint") + "> image:register(u0);\n"
                          "[numthreads(1,1,1)]void main(uint3 id:SV_DispatchThreadID){image["
                          + coordinate + "]=" + expression + ";}\n")
                yield index, source.encode()


def compute_chunk(binary):
    if len(binary) < 36 or binary[:4] != b"DXBC" or struct.unpack_from("<I", binary, 24)[0] != len(binary):
        raise AssertionError("Original DXBC extent")
    count = struct.unpack_from("<I", binary, 28)[0]
    if not 1 <= count <= 32 or 32 + 4 * count > len(binary):
        raise AssertionError("Original DXBC chunk table")
    found = []
    for offset in struct.unpack_from("<" + "I" * count, binary, 32):
        if offset < 32 + 4 * count or offset + 8 > len(binary):
            raise AssertionError("Original DXBC chunk offset")
        length = struct.unpack_from("<I", binary, offset + 4)[0]
        if offset + 8 + length > len(binary):
            raise AssertionError("Original DXBC chunk extent")
        if binary[offset:offset + 4] == b"SHEX":
            code = binary[offset + 8:offset + 8 + length]
            if length < 8 or length % 4:
                raise AssertionError("Original compute token extent")
            version, words = struct.unpack_from("<2I", code)
            if version != 0x50050 or 4 * words != len(code):
                raise AssertionError("Original compute version/word count")
            found.append(code)
    if len(found) != 1:
        raise AssertionError("Exactly one original SHEX is required")
    return found[0]


def verify(directory):
    directory = Path(directory)
    wanted, originals = set(), []
    checks = words = observations = 0

    def load(name):
        nonlocal checks
        path = directory / name
        if not path.is_file() or path.is_symlink():
            raise AssertionError("Missing or nonregular original: " + name)
        raw = path.read_bytes()
        wanted.add(name); checks += 1
        originals.append(dict(path=str(path.resolve()), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        return raw

    for index, metadata, stages in cases():
        prefix = f"texture-uav-{index:03d}"
        if load(prefix + ".metadata") != struct.pack("<11I", *metadata):
            raise AssertionError("Original resource/view dimension/range differs: " + prefix)
        checks += 1
        for stage, expected in stages.items():
            words += len(expected)
            for role in ["native", "public"]:
                name = prefix + "." + stage + "." + role + ".words"
                raw = load(name)
                if len(raw) != 4 * len(expected):
                    raise AssertionError("Original readback extent: " + name)
                for word, (actual, oracle) in enumerate(zip(struct.unpack("<" + "I" * len(expected), raw), expected)):
                    checks += 1; observations += 1
                    if actual != oracle:
                        raise AssertionError(f"{name} word{word}: {actual:08x} != {oracle:08x}")
    for index, source in shader_cases():
        prefix = f"texture-uav-{index:03d}"
        if load(prefix + ".hlsl") != source:
            raise AssertionError("Original HLSL shape/type differs: " + prefix)
        binary, code = load(prefix + ".dxbc"), load(prefix + ".shex")
        if compute_chunk(binary) != code:
            raise AssertionError("Original SHEX does not join its DXBC: " + prefix)
        checks += 1
    if {p.name for p in directory.glob("texture-uav-*")} != wanted:
        raise AssertionError("Exact original output closure")
    if words != 10752 or observations != 21504 or len(originals) != 384:
        raise AssertionError("Independent UAV word/file counts")
    return dict(passed=True, scope="original texture UAV descriptors/compute/clear absolute-coordinate oracle",
                checks=checks, views=72, words=words, observations=observations, original_files=originals,
                hardware_admission=False, registration=False,
                compiler_provenance_scope="caller must separately join actual source/compiler/process originals")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = verify(args.directory)
    if args.output:
        with args.output.open("x") as output:
            json.dump(result, output, indent=2)
    print(json.dumps({key: value for key, value in result.items() if key != "original_files"}))


if __name__ == "__main__":
    main()
