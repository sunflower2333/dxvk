#!/usr/bin/env python3
"""Verify shared-transfer predicate originals with an independent byte recipe."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


MARKER = (b'DXGI shared transfer predicate PASS profiles=3 snapshots=21 '
          b'pixels=735 hardware_admission=0')
SEEDS = (17, 3, 19, 19, 23, 3, 17)
MAPPED_CASES = (0, 1, 5, 6)
WIDTH, HEIGHT, FORMAT = 7, 5, 28


def require(value, message):
    if not value:
        raise ValueError(message)


def original(path, size=None):
    require(path.is_file() and not path.is_symlink(), f'Not a regular original: {path}')
    raw = path.read_bytes()
    require(size is None or len(raw) == size, f'{path.name}: original byte count differs')
    return raw, dict(path=str(path), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def pixel(seed, x, y):
    # Literal physical RGBA storage bytes, independent of fixture helpers.
    return bytes((seed + 3 * x, seed + 7 * y, 11 * (x + y), 255))


def verify(directory, stdout):
    directory, stdout = Path(directory), Path(stdout)
    require(directory.is_dir() and not directory.is_symlink(), 'Original directory is absent or linked')
    raw_stdout, stdout_identity = original(stdout)
    marker_lines = [line.removesuffix(b'\r') for line in raw_stdout.split(b'\n')
                    if line.startswith(b'DXGI shared transfer predicate PASS')]
    require(marker_lines == [MARKER], 'Shared-transfer predicate marker is absent, changed or ambiguous')
    names = {
        f'shared-transfer-predicate-{profile}-{case}.metadata.u32.bin'
        for profile in range(3) for case in range(7)
    } | {
        f'shared-transfer-predicate-{profile}-{case}.actual.'
        + ('u32.bin' if case in MAPPED_CASES else 'bin')
        for profile in range(3) for case in range(7)
    }
    observed_names = {path.name for path in directory.iterdir()
                      if path.name.startswith('shared-transfer-predicate-')}
    require(observed_names == names, 'Shared-transfer predicate original file closure differs')
    images = []
    mapped_pixels, backing_pixels = 0, 0
    for profile in range(3):
        for case, seed in enumerate(SEEDS):
            stem = f'shared-transfer-predicate-{profile}-{case}'
            metadata, metadata_identity = original(directory / f'{stem}.metadata.u32.bin', 32)
            seen = struct.unpack('<8I', metadata)
            require(seen[:4] == (WIDTH, HEIGHT, case, seed)
                    and seen[6:] == (FORMAT, profile), f'{stem}: fixed metadata differs')
            if case in MAPPED_CASES:
                row_pitch, depth_pitch = seen[4:6]
                require(row_pitch >= WIDTH * 4, f'{stem}: mapped row pitch is short')
                # Texture2D DepthPitch is retained, without inventing a fixed
                # value or volume rule for this observed native map result.
                actual, actual_identity = original(directory / f'{stem}.actual.u32.bin', WIDTH * HEIGHT * 4)
                for y in range(HEIGHT):
                    for x in range(WIDTH):
                        at = (y * WIDTH + x) * 4
                        require(actual[at:at + 4] == pixel(seed, x, y),
                                f'{stem}: mapped pixel ({x},{y}) differs')
                mapped_pixels += WIDTH * HEIGHT
                observation = dict(kind='mapped', row_pitch=row_pitch, depth_pitch=depth_pitch)
            else:
                require(seen[4:6] == (40, 200), f'{stem}: borrowed backing pitch or size differs')
                actual, actual_identity = original(directory / f'{stem}.actual.bin', 232)
                require(actual[:16] == b'\xa5' * 16 and actual[-16:] == b'\xa5' * 16,
                        f'{stem}: borrowed backing boundary guards differ')
                for y in range(HEIGHT):
                    row = actual[16 + y * 40:16 + (y + 1) * 40]
                    require(row[28:] == b'\xa5' * 12, f'{stem}: borrowed row {y} padding differs')
                    for x in range(WIDTH):
                        require(row[x * 4:x * 4 + 4] == pixel(seed, x, y),
                                f'{stem}: borrowed pixel ({x},{y}) differs')
                backing_pixels += WIDTH * HEIGHT
                observation = dict(kind='borrowed-backing', pitch=40, size=200)
            images.append(dict(profile=profile, case=case, seed=seed, width=WIDTH,
                               height=HEIGHT, format=FORMAT, pixels=WIDTH * HEIGHT,
                               actual=actual_identity, metadata=metadata_identity, **observation))
    require(len(images) == 21 and mapped_pixels == 420 and backing_pixels == 315
            and len(names) == 42, 'Shared-transfer predicate workload totals differ')
    return dict(schema='native-shared-transfer-predicate-originals-v1', verified=True,
                profiles=3, snapshots=21, pixels=mapped_pixels + backing_pixels,
                mapped_snapshots=12, mapped_pixels=mapped_pixels,
                borrowed_backing_snapshots=9, borrowed_backing_pixels=backing_pixels,
                original_files=len(names), images=images, stdout=stdout_identity,
                backend='WARP production-DDI reference', hardware_admission=False,
                ordinary_runtime_admission=False, primary_scanout_admission=False,
                oracle='fixed physical RGBA bytes; literal borrowed guards and row padding')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    proof = verify(args.directory, args.stdout)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps({key: proof[key] for key in
                     ('verified', 'profiles', 'snapshots', 'pixels', 'mapped_pixels',
                      'borrowed_backing_pixels', 'original_files', 'hardware_admission')}))


if __name__ == '__main__':
    main()
