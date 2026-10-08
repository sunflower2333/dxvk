#!/usr/bin/env python3
"""Verify typed shared-handoff readbacks using independent pixel arithmetic."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct


def require(value, message):
    if not value:
        raise ValueError(message)


def original(path, size):
    require(path.is_file() and not path.is_symlink(), f'Original is not a regular file: {path}')
    data = path.read_bytes()
    require(len(data) == size, f'{path.name}: expected {size} original bytes, got {len(data)}')
    return data, dict(path=str(path), bytes=size, sha256=hashlib.sha256(data).hexdigest())


def rgba(seed, x, y):
    # Little-endian RGBA: the red and green channels encode different axes;
    # blue encodes both. The five fixed seeds identify each ownership boundary.
    return bytes((seed + 3 * x, seed + 7 * y, 11 * (x + y), 255))


def verify_legacy(directory, stdout):
    require(directory.is_dir(), 'Original readback directory is absent')
    require(stdout.is_file() and not stdout.is_symlink(), 'Original stdout is absent')
    raw = stdout.read_bytes()
    text = raw.decode('utf-8')
    markers = re.findall(r'(?m)^DXGI shared resolve PASS checks=(\d+) profiles=3 '
                         r'snapshots=15 pixels=525 hardware_admission=0\r?$', text)
    require(len(markers) == 1 and int(markers[0]) > 525, 'Actual shared-resolve marker is absent or ambiguous')
    names, images, padding = set(), [], []
    seeds = (1, 3, 5, 5, 11)
    for profile in range(3):
        for sample, seed in enumerate(seeds):
            stem = f'resolve-{profile}-{sample}'
            paths = [directory / (stem + suffix) for suffix in ('.actual.u32.bin', '.metadata.u32.bin')]
            actual, actual_identity = original(paths[0], 7 * 5 * 4)
            metadata, metadata_identity = original(paths[1], 20)
            width, height, seen_seed, row_pitch, depth_pitch = struct.unpack('<5I', metadata)
            require((width, height, seen_seed) == (7, 5, seed), f'{stem}: ownership seed or dimensions differ')
            require(row_pitch >= 28, f'{stem}: original map pitch does not cover a row')
            for y in range(5):
                for x in range(7):
                    offset = (y * 7 + x) * 4
                    require(actual[offset:offset + 4] == rgba(seed, x, y),
                            f'{stem}: xy={x}/{y} RGBA={actual[offset:offset + 4].hex()} differs')
            names.update(path.name for path in paths)
            images.append(dict(profile=profile, sample=sample, seed=seed, pixels=35,
                               row_pitch=row_pitch, depth_pitch=depth_pitch,
                               actual=actual_identity, metadata=metadata_identity))
        stem = f'resolve-padded-{profile}'
        paths = [directory / (stem + suffix) for suffix in ('.actual.bin', '.metadata.u32.bin')]
        actual, actual_identity = original(paths[0], 232)
        metadata, metadata_identity = original(paths[1], 16)
        require(struct.unpack('<4I', metadata) == (7, 5, 40, 200), f'{stem}: original allocation shape differs')
        require(actual[:16] == actual[-16:] == bytes([0xa5]) * 16, f'{stem}: allocation boundary canary changed')
        for y in range(5):
            for x in range(7):
                offset = 16 + y * 40 + x * 4
                require(actual[offset:offset + 4] == rgba(13, x, y), f'{stem}: uploaded xy={x}/{y} differs')
            require(actual[16 + y * 40 + 28:16 + (y + 1) * 40] == bytes([0xa5]) * 12,
                    f'{stem}: uploaded row {y} overwrote allocation padding')
        names.update(path.name for path in paths)
        padding.append(dict(profile=profile, pixels=35, row_padding_bytes=60,
                            actual=actual_identity, metadata=metadata_identity))
    require({path.name for path in directory.glob('resolve-*')} == names,
            'Original shared-resolve files are missing, extra or ambiguous')
    require(len(images) == 15 and len(padding) == 3 and len(names) == 36, 'Original readback totals differ')
    return dict(schema='native-shared-resolve-originals-v1', verified=True,
                profiles=3, snapshots=15, readback_pixels=525, uploaded_pixels=105,
                original_files=36, actual_checks=int(markers[0]), images=images,
                padded_allocations=padding, stdout=dict(path=str(stdout), bytes=len(raw),
                                                       sha256=hashlib.sha256(raw).hexdigest()),
                backend='WARP production-DDI reference', hardware_admission=False,
                keyed_mutex_admission=False, gdi_admission=False,
                oracle='literal RGBA axis arithmetic plus original allocation padding and canaries')


def verify(directory, stdout):
    proof = verify_legacy(directory, stdout)
    path = Path(__file__).with_name('verify-native-shared-transfer-predicate-originals.py')
    spec = importlib.util.spec_from_file_location('native_shared_transfer_predicate_originals', path)
    require(spec is not None and spec.loader is not None, 'Shared transfer predicate reader is absent')
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    proof['shared_transfer_predicate'] = reader.verify(directory, stdout)
    return proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    proof = verify(args.directory, args.stdout)
    with args.output.open('x') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps({key: proof[key] for key in ('verified', 'profiles', 'snapshots',
                                                'readback_pixels', 'uploaded_pixels', 'original_files',
                                                'hardware_admission')}))


if __name__ == '__main__':
    main()
