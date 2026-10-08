#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reconstruct all face/mip bytes and typed view descriptors from raw originals."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def require(value, message):
    if not value:
        raise ValueError(message)


def ranges(profile):
    return ((0, 5, 1), (1, 1, 3), (2, 0, 6), (3, 5, 1)) if profile == 0 else (
        (0, 5, 3), (1, 11, 2), (2, 17, 1), (3, 0, 18))


def expected(profile, depth, step):
    faces = 6 if profile == 0 else 18
    words = []
    for face in range(faces):
        for mip in range(4):
            edge = 8 >> mip
            for word in range(edge * edge):
                value = ((0x40 + face) << 24) | (0x400000 + 32 * face + mip) if depth else (
                    0xff000000 | (face << 19) | (mip << 16) | (word + 1))
                for operation, (selected_mip, first, count) in enumerate(ranges(profile)):
                    if operation > step or (depth and profile == 2 and operation):
                        continue
                    if mip == selected_mip and first <= face < first + count:
                        value = ((0x29 + operation) << 24) | 0xffffff if depth else (
                            0xffff0000 | (0xff00 if operation & 2 else 0) | (0xff if operation & 1 else 0))
                words.append(value)
    return struct.pack('<' + str(len(words)) + 'I', *words)


def descriptor(profile, depth, step):
    mip, first, count = ranges(profile)[step]
    fields = (45, 4, step if profile == 2 else 0, mip, first, count) if depth else (
        28, 5, mip, first, count)
    return struct.pack('<' + str(len(fields)) + 'I', *fields)


def verify(originals, stdout):
    text = stdout.read_text(encoding='utf-8')
    rows = []
    wanted_names = set()
    total = 0

    def join(name, wanted):
        nonlocal total
        path = originals / name
        raw = path.read_bytes()
        require(raw == wanted, 'raw byte oracle mismatch: ' + name)
        wanted_names.add(name)
        rows.append(dict(name=name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))

    expected_reads = []
    for profile in range(3):
        faces = 6 if profile == 0 else 18
        for depth in range(2):
            kind = 'depth' if depth else 'color'
            for step in range(5):
                wanted = expected(profile, depth, step)
                prefix = f'cube-target-{profile}-{kind}-{step}'
                for suffix in ('.actual.u32', '.public.u32', '.expected.u32'):
                    join(prefix + suffix, wanted)
                words = len(wanted) // 4
                total += words
                expected_reads.append((profile, depth, step, faces, words))
                if step < 4:
                    desc = descriptor(profile, depth, step)
                    name = f'cube-target-desc-{profile}-{kind}-{step}'
                    join(name + '.u32', desc)
                    join(name + '.public-u32', desc)
    observed_reads = [tuple(map(int, row)) for row in re.findall(
        r'^CUBE_TARGET_READBACK profile=(\d+) depth=(\d+) step=(\d+) faces=(\d+) words=(\d+) mismatches=0$', text, re.M)]
    require(observed_reads == expected_reads and len(re.findall(r'^CUBE_TARGET_READBACK', text, re.M)) == 30,
        'raw readback/stdout ordering or counts')
    expected_negatives = []
    count = 0
    for profile in range(3):
        for depth in range(2):
            operations = 8 if profile == 0 else (9 if profile == 2 and depth else 7)
            for _ in range(operations):
                count += 1
                expected_negatives.append((profile, depth, count))
    observed_negatives = [tuple(map(int, row)) for row in re.findall(
        r'^CUBE_TARGET_NEGATIVE profile=(\d+) depth=(\d+) callbacks=(\d+) hr=80070057 atomic=1 binding_retained=1$', text, re.M)]
    require(observed_negatives == expected_negatives and len(re.findall(r'^CUBE_TARGET_NEGATIVE', text, re.M)) == 46,
        'negative callback trace/count/order')
    matches = re.findall(r'^typed cube-array targets verified checks=(\d+) views=24 snapshots=30 words=35700 callbacks=46 public_reference=1 hardware_admission=0$', text, re.M)
    require(len(matches) == 1 and int(matches[0]) > 35700
        and len(re.findall(r'^typed cube-array targets verified', text, re.M)) == 1, 'original native completion marker')
    observed_names = {path.name for path in originals.glob('cube-target-*') if path.is_file()}
    require(observed_names == wanted_names and len(rows) == 138 and total == 35700, 'exact raw output set/size')
    return dict(passed=True, native_words=total, public_words=total, expected_words=total,
        snapshots=30, views=24, callbacks=46, originals=rows,
        original_stdout=dict(path=str(stdout), bytes=stdout.stat().st_size,
            sha256=hashlib.sha256(stdout.read_bytes()).hexdigest()),
        actual_process_and_compiler_provenance_verified=False,
        hardware_acceptance=False, ordinary_runtime_admission=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--originals', required=True, type=Path)
    parser.add_argument('--stdout', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    require(not args.output.exists(), 'refusing existing output')
    result = verify(args.originals, args.stdout)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('cube-array target originals verified snapshots=30 views=24 native_words=35700 public_words=35700 callbacks=46 hardware=0')


if __name__ == '__main__':
    main()
