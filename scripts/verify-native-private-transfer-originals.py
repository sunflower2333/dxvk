#!/usr/bin/env python3
"""Verify private rotation/Present transfer originals using a fixed byte recipe."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

MARKER = b'DXGI private transfer PASS profiles=3 snapshots=24 pixels=840 hardware_admission=0'
SEEDS = (4, 6, 2, 4, 0, 0, 4, 8)
BACKING_CASES = (3, 4, 6, 7)


def require(value, message):
    if not value:
        raise ValueError(message)


def original(path, size=None):
    require(path.is_file() and not path.is_symlink(), f'Not a regular original: {path}')
    raw = path.read_bytes()
    require(size is None or len(raw) == size, f'{path.name}: original byte count differs')
    return raw, dict(path=str(path), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def word(case, x, y):
    if case == 4:
        return 0xff0000ff  # Four identical red samples resolve into RGBA.
    if case == 5:
        return 0xff281008  # Issued non-hint predicate suppresses app Copy/Update.
    seed = SEEDS[case]
    return (((64 + seed * 11 + x * 3 + y * 5) & 255) << 24
            | ((seed * 13 + x * 17 + y * 7) & 255)
            | (((seed * 19 + x * 5 + y * 23) & 255) << 8)
            | (((seed * 29 + x * 11 + y * 13) & 255) << 16))


def verify(directory, stdout):
    directory, stdout = Path(directory), Path(stdout)
    require(directory.is_dir() and not directory.is_symlink(), 'Original directory is absent or linked')
    raw_stdout, stdout_identity = original(stdout)
    markers = [line.removesuffix(b'\r') for line in raw_stdout.split(b'\n')
               if line.startswith(b'DXGI private transfer PASS')]
    require(markers == [MARKER], 'Private transfer marker is absent, changed or ambiguous')
    names = {f'private-transfer-{profile}-{case}.{role}'
             for profile in range(3) for case in range(8)
             for role in ('actual.bin', 'metadata.u32.bin')}
    observed = {path.name for path in directory.iterdir() if path.name.startswith('private-transfer-')}
    require(observed == names, 'Private transfer original file closure differs')
    images = []
    for profile in range(3):
        for case in range(8):
            stem = f'private-transfer-{profile}-{case}'
            backing = case in BACKING_CASES
            actual, actual_identity = original(directory / f'{stem}.actual.bin', 172 if backing else 140)
            metadata, metadata_identity = original(directory / f'{stem}.metadata.u32.bin', 32)
            width, height, seen_case, seed, pitch, span, format_value, seen_profile = struct.unpack('<8I', metadata)
            require((width, height, seen_case, seed, format_value, seen_profile)
                    == (7, 5, case, SEEDS[case], 28, profile), f'{stem}: fixed metadata differs')
            require(pitch >= 28, f'{stem}: map pitch is short')
            if backing:
                require((pitch, span) == (28, 140), f'{stem}: owned backing geometry differs')
                require(actual[:16] == bytes([0xa5]) * 16 and actual[156:] == bytes([0xa5]) * 16,
                        f'{stem}: owned backing guards changed')
                payload = actual[16:156]
            else:
                # DepthPitch is observation only for a Texture2D map; the
                # runtime is free to leave it zero. Packed raw words are fixed.
                payload = actual
            words = struct.unpack('<35I', payload)
            for y in range(5):
                for x in range(7):
                    observed_word, expected_word = words[y * 7 + x], word(case, x, y)
                    require(observed_word == expected_word,
                            f'{stem}: pixel ({x},{y}) actual={observed_word:08x} expected={expected_word:08x}')
            images.append(dict(profile=profile, case=case, seed=seed, pixels=35,
                               width=7, height=5, row_pitch=pitch, observed_span=span,
                               kernel_backing=backing, actual=actual_identity, metadata=metadata_identity))
    require(len(images) == 24 and len(names) == 48, 'Private transfer workload totals differ')
    return dict(schema='native-dxgi-private-transfer-originals-v1', verified=True,
                profiles=3, snapshots=24, pixels=840, original_files=48, images=images,
                stdout=stdout_identity, backend='WARP production-DDI reference',
                hardware_admission=False, ordinary_runtime_admission=False,
                primary_scanout_admission=False,
                oracle='fixed complete RGBA words; backing guards; no alpha masking')


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
                     ('verified', 'profiles', 'snapshots', 'pixels', 'original_files', 'hardware_admission')}))


if __name__ == '__main__':
    main()
