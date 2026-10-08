#!/usr/bin/env python3
"""Read original fixture outputs without executing a compiler or a target."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

SENTINEL = (1 << 32) - 1


def expected_cases():
    """Independent absolute-coordinate oracle, without production helpers."""
    for profile in range(3):
        index = 0
        for levels, slices, samples in [(4, 1, 1), (4, 3, 1), (1, 1, 4), (1, 3, 4)]:
            for first_slice in range(slices):
                for first_mip in range(levels):
                    mip_requests = [1, levels - first_mip, SENTINEL] if samples == 1 else [1, SENTINEL]
                    for requested_mips in mip_requests:
                        for requested_slices in [1, slices - first_slice, SENTINEL]:
                            absolute_mips = list(range(first_mip, levels if requested_mips == SENTINEL
                                                       else first_mip + requested_mips))
                            absolute_slices = list(range(first_slice, slices if requested_slices == SENTINEL
                                                         else first_slice + requested_slices))
                            metadata = [8, 4, levels, slices, samples, first_mip, requested_mips,
                                        first_slice, requested_slices, len(absolute_mips), len(absolute_slices)]
                            words = []
                            if samples == 1:
                                for array_index in absolute_slices:
                                    for mip in absolute_mips:
                                        width, height = max(1, 8 // (1 << mip)), max(1, 4 // (1 << mip))
                                        for corner in range(4):
                                            x = (width - 1) * (corner % 2)
                                            y = (height - 1) * (corner // 2)
                                            words.append(0x11000000 + array_index * 65536 + mip * 4096 + y * 64 + x)
                            yield profile, index, metadata, words
                            index += 1
        if index != 168:
            raise AssertionError("Independent case count")


def pin(path, raw):
    return dict(path=str(path.resolve()), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def verify(directory):
    directory = Path(directory)
    checks, sampled, multisample = 0, 0, 0
    originals, wanted = [], set()

    def load(name):
        nonlocal checks
        path = directory / name
        if not path.is_file() or path.is_symlink():
            raise AssertionError("Missing or nonregular original: " + name)
        raw = path.read_bytes()
        checks += 1; wanted.add(name); originals.append(pin(path, raw))
        return raw

    for profile, index, metadata, expected in expected_cases():
        prefix = f"tex2d-srv-{profile}-{index:03d}"
        raw = load(prefix + ".metadata")
        if raw != struct.pack("<11I", *metadata):
            raise AssertionError("Original requested/resolved range differs: " + prefix)
        checks += 1
        if metadata[4] > 1:
            multisample += 1
            continue
        for role in ["native", "public"]:
            raw = load(prefix + "." + role + ".words")
            if len(raw) != 4 * len(expected):
                raise AssertionError("Original word count: " + prefix + "." + role)
            observed = struct.unpack("<" + "I" * len(expected), raw)
            for word, (actual, oracle) in enumerate(zip(observed, expected)):
                checks += 1
                if actual != oracle:
                    raise AssertionError(f"{prefix}.{role} word{word}: {actual:08x} != {oracle:08x}")
        sampled += len(expected)
    for profile in range(3):
        for shader in range(2):
            prefix = f"tex2d-srv-{profile}-{shader:03d}"
            source = load(prefix + ".hlsl")
            if b"source.GetDimensions" not in source or b"source.Load" not in source:
                raise AssertionError("Original sampling HLSL missing")
            dimension = b"Texture2D<uint>" if shader == 0 else b"Texture2DArray<uint>"
            if dimension not in source:
                raise AssertionError("Original HLSL dimension")
            binary = load(prefix + ".dxbc")
            if len(binary) < 36 or binary[:4] != b"DXBC" or struct.unpack_from("<I", binary, 24)[0] != len(binary):
                raise AssertionError("Original DXBC container")
            count = struct.unpack_from("<I", binary, 28)[0]
            if not 1 <= count <= 32 or 32 + 4 * count > len(binary):
                raise AssertionError("Original DXBC chunk table")
            compute = False
            for offset in struct.unpack_from("<" + "I" * count, binary, 32):
                if offset < 32 + 4 * count or offset + 8 > len(binary):
                    raise AssertionError("Original DXBC chunk offset")
                length = struct.unpack_from("<I", binary, offset + 4)[0]
                if offset + 8 + length > len(binary):
                    raise AssertionError("Original DXBC chunk extent")
                if binary[offset:offset + 4] == b"SHEX":
                    if length < 8 or length % 4:
                        raise AssertionError("Original SHEX extent")
                    version, words = struct.unpack_from("<2I", binary, offset + 8)
                    if version != 0x50050 or 4 * words != length:
                        raise AssertionError("Original cs_5_0 extent/version")
                    compute = True
            if not compute:
                raise AssertionError("Original compute code missing")
            checks += 1
    actual = {path.name for path in directory.glob("tex2d-srv-*")}
    if actual != wanted or sampled != 5184 or multisample != 72 or len(originals) != 1380:
        raise AssertionError("Exact original fixture case/file closure")
    return dict(passed=True, scope="original Texture2D SRV requested-range and independent value oracle",
                checks=checks, profiles=3, views=504, multisample_views=multisample, words=sampled,
                original_files=originals, hardware_admission=False, registration=False,
                compiler_provenance_scope="caller must separately join actual source/compiler/process originals")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = verify(args.directory)
    if args.output:
        with args.output.open("x") as out:
            json.dump(result, out, indent=2)
    print(json.dumps({key: value for key, value in result.items() if key != "original_files"}))


if __name__ == "__main__":
    main()
