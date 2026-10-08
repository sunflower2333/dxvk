#!/usr/bin/env python3
"""Read original typed/public/kernel outputs; this does not admit hardware."""
import argparse
import hashlib
import json
import pathlib
import struct


def verify(directory):
    expected = set()
    originals = []
    observations = 0
    handles = set()
    for profile in range(3):
        for fmt in (1, 3):
            handle = None
            for snapshot, seed in enumerate((4, 4, 6, 7)):
                base = f"optional-shared-{profile}-{fmt}-{snapshot}"
                frames = {}
                for suffix, length in (("native.u32", 140), ("public.u32", 140),
                                       ("kernel.u32", 140), ("metadata.u32", 44),
                                       ("allocation", 80)):
                    name = f"{base}.{suffix}.bin"
                    expected.add(name)
                    path = directory / name
                    assert path.is_file() and not path.is_symlink(), name
                    raw = path.read_bytes()
                    assert len(raw) == length, name
                    frames[suffix] = raw
                    originals.append(dict(path=str(path.resolve()), bytes=length,
                                          sha256=hashlib.sha256(raw).hexdigest()))
                metadata = struct.unpack("<11I", frames["metadata.u32"])
                assert metadata[:4] == (7, 5, fmt, seed), base
                assert metadata[4] >= 28 and metadata[5] >= 28, base
                assert metadata[6:8] == (28, 140), base
                assert metadata[8] and metadata[9:] == (2, 1), base
                if handle is None:
                    handle = metadata[8]
                    assert handle not in handles, base
                    handles.add(handle)
                assert metadata[8] == handle, base
                allocation = struct.unpack("<4I4Q8I", frames["allocation"])
                assert allocation == (0x504D5644, 0, 80, 0, 140, 4096, 0, 0,
                                      2, fmt, 7, 5, 28, 0, 0, 0), base
                for role in ("native.u32", "public.u32", "kernel.u32"):
                    words = struct.unpack("<35I", frames[role])
                    for y in range(5):
                        for x in range(7):
                            literal = (0xFF000000 | ((17 * seed + 3 * x + 5 * y) & 255)
                                       | (((29 * seed + 7 * x + 11 * y) & 255) << 8)
                                       | (((43 * seed + 13 * x + 19 * y) & 255) << 16))
                            assert words[y * 7 + x] == literal, (base, role, x, y)
                            observations += 1
    actual = set()
    for path in directory.iterdir():
        assert path.is_file() and not path.is_symlink(), str(path)
        actual.add(path.name)
    assert actual == expected, (sorted(actual - expected), sorted(expected - actual))
    return dict(passed=True, profiles=3, formats=2, images=24, pixels=840,
                original_file_count=120, observations=observations,
                hardware_admission=False, registration=False, originals=originals)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    directory = args.directory.resolve()
    output = args.output.resolve()
    assert output.parent != directory and directory not in output.parents, "Output must be outside original directory"
    result = verify(directory)
    output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
