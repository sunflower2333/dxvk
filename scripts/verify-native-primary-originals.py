#!/usr/bin/env python3
"""Read literal primary pixels independently of fixture/production helpers."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def verify(directory, stdout):
    marker = re.fullmatch(
        r'DXGI primary/display PASS checks=([1-9][0-9]*) profiles=6 snapshots=24 pixels=768 hardware_admission=0\r?\n?',
        stdout.read_text(encoding='utf-8'))
    if not marker:
        raise ValueError('primary fixture marker')
    expected_names = {
        f'primary-{profile}-{image}.{role}.bin'
        for profile in range(6) for image in range(4)
        for role in ('actual', 'expected', 'metadata')
    }
    actual_names = {p.name for p in directory.glob('primary-*')}
    if actual_names != expected_names:
        raise ValueError('primary original file closure')
    # Literal stored byte order, independently specified from clear colors.
    colors = (bytes((255, 0, 0, 255)), bytes((0, 255, 0, 255)),
              bytes((255, 0, 0, 255)), bytes((0, 255, 255, 255)))
    formats = (3, 1, 2, 3)
    rows = []
    for profile in range(6):
        for image in range(4):
            primary = image < 3
            metadata = (profile, image, 8, 4, formats[image], 1 if primary else 2,
                        60000 if primary else 0, 1001 if primary else 0)
            for role in ('actual', 'expected', 'metadata'):
                path = directory / f'primary-{profile}-{image}.{role}.bin'
                raw = path.read_bytes()
                reference = struct.pack('<8I', *metadata) if role == 'metadata' else colors[image] * 32
                if raw != reference:
                    raise ValueError(f'primary {profile}/{image}/{role} literal mismatch')
                rows.append(dict(path=str(path), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
    return dict(verified=True, profiles=6, snapshots=24, pixels=768,
                primary_images=18, optional_copy_images=6, originals=rows,
                checks=int(marker[1]), hardware_admission=False, gamma_capability_admission=False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    parser.add_argument('--stdout', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = verify(args.directory, args.stdout)
    with args.output.open('x', encoding='utf-8') as target:
        json.dump(result, target, indent=2)
        target.write('\n')
    print('DXGI primary original pixels PASS profiles=6 snapshots=24 pixels=768 files=72')


if __name__ == '__main__':
    main()
