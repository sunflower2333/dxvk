#!/usr/bin/env python3
"""Reopen actual typed/public/KMD shared-Present pixels; native execution required."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def require(value, reason):
    if not value:
        raise ValueError(reason)


def words(path, count):
    raw = path.read_bytes()
    require(len(raw) == count * 4, f'{path.name}: word count')
    return struct.unpack(f'<{count}I', raw)


def expected(seed, x, y):
    return (0xff000000 | ((17 * seed + 3 * x + 5 * y) & 255)
            | (((29 * seed + 7 * x + 11 * y) & 255) << 8)
            | (((43 * seed + 13 * x + 19 * y) & 255) << 16))


def verify(directory):
    directory = Path(directory)
    inventory = {}
    observations = []
    wanted = set()
    for profile in range(3):
        cases = [(0, fmt, snap, seed) for fmt in (1, 2, 3)
                 for snap, seed in enumerate((1, 2, 3, 7))]
        cases += [(1, fmt, snap, snap + 4) for fmt in (1, 3) for snap in range(2)]
        for kind, fmt, snap, seed in cases:
            stem = f'shared-present-{profile}-{kind}-{fmt}-{snap}'
            names = {suffix: f'{stem}.{suffix}.bin' for suffix in
                     ('native.u32', 'public.u32', 'kernel.u32', 'metadata.u32', 'allocation')}
            wanted.update(names.values())
            meta = words(directory / names['metadata.u32'], 10)
            require(meta[:4] == (7, 5, fmt, seed), f'{stem}: shape/seed/format')
            require(meta[4] >= 28 and meta[5] >= 28, f'{stem}: native/public pitches')
            require(meta[6] == (40 if kind == 0 else 28), f'{stem}: backing pitch')
            require(meta[7] == meta[6] * 5 and meta[8] != 0, f'{stem}: size/handle')
            require(meta[9] == (1 if kind == 0 else 2), f'{stem}: allocation ownership')
            raw = (directory / names['allocation']).read_bytes()
            require(len(raw) == 80, f'{stem}: allocation bytes')
            info = struct.unpack('<IIIIQQQQIIIIIIII', raw)
            require(info[:4] == (0x504d5644, 0, 80, 0), f'{stem}: wire header')
            require(info[4:8] == (meta[7], 4096, 0, 0), f'{stem}: wire size/alignment/generation')
            require(info[8:13] == (meta[9], fmt, 7, 5, meta[6]), f'{stem}: wire image')
            require(info[13:] == ((60000, 1001, 0) if kind == 0 else (0, 0, 0)), f'{stem}: wire refresh/context')
            observed = [words(directory / names[suffix], 35) for suffix in
                        ('native.u32', 'public.u32', 'kernel.u32')]
            require(observed[0] == observed[1] == observed[2], f'{stem}: native/public/KMD equality')
            oracle = tuple(expected(seed, x, y) for y in range(5) for x in range(7))
            require(observed[0] == oracle, f'{stem}: literal independent pixel oracle')
            for name in names.values():
                data = (directory / name).read_bytes()
                inventory[name] = dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
            observations.append(dict(profile=profile, kind=kind, format=fmt,
                                     snapshot=snap, seed=seed, actual_handle=meta[8], pixels=35))
    actual = {p.name for p in directory.glob('shared-present-*.bin')}
    require(actual == wanted, f'exact raw inventory: missing={sorted(wanted-actual)}, extra={sorted(actual-wanted)}')
    return dict(raw_files=len(inventory), images=len(observations), pixels=1680,
                native_public_kernel_words=5040, observations=observations,
                originals=inventory, ordinary_runtime_admission=False, hardware_admission=False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.directory)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('shared Present original reader PASS raw_files=240 images=48 pixels=1680 observations=5040')


if __name__ == '__main__':
    main()
