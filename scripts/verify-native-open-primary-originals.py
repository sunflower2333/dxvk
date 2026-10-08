#!/usr/bin/env python3
"""Check opened-primary pixels and borrowed padding without fixture helpers."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def verify(directory, stdout):
    marker = re.fullmatch(
        r'DXGI opened primary PASS checks=([1-9][0-9]*) profiles=3 formats=3 '
        r'images=36 pixels=1260 failures=78 callbacks=78 runtime_terminal_releases=4 '
        r'runtime_terminal_maps=2 hardware_admission=0\r?\n?',
        stdout.read_text(encoding='utf-8'))
    if not marker:
        raise ValueError('opened-primary fixture marker')
    names = {
        f'open-primary-{profile}-{format_}-{snapshot}.{role}.u32.bin'
        for profile in range(3) for format_ in range(1, 4) for snapshot in range(4)
        for role in ('actual', 'metadata')
    } | {
        f'open-primary-padded-{profile}-{format_}.{role}.bin'
        for profile in range(3) for format_ in range(1, 4)
        for role in ('actual', 'metadata.u32')
    }
    if {p.name for p in directory.glob('open-primary-*')} != names:
        raise ValueError('opened-primary original file closure')
    rows = []

    def original(name):
        path = directory / name
        if not path.is_file() or path.is_symlink():
            raise ValueError(f'{name}: nonregular original')
        raw = path.read_bytes()
        rows.append(dict(path=str(path), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        return raw

    for profile in range(3):
        for format_ in range(1, 4):
            for snapshot in range(4):
                stem = f'open-primary-{profile}-{format_}-{snapshot}'
                metadata = original(stem + '.metadata.u32.bin')
                if len(metadata) != 32:
                    raise ValueError(f'{stem}: metadata byte count')
                w, h, stored_format, seed, row_pitch, depth_pitch, shared_pitch, shared_size = struct.unpack('<8I', metadata)
                if (w, h, stored_format, seed, shared_pitch, shared_size) != (7, 5, format_, snapshot + 1, 40, 200):
                    raise ValueError(f'{stem}: metadata values')
                # The renderer owns its mapped pitch. Texture2D DepthPitch is
                # retained as an observation, without inventing a volume rule.
                if row_pitch < 28 or row_pitch & 3:
                    raise ValueError(f'{stem}: mapped row pitch')
                actual = original(stem + '.actual.u32.bin')
                if len(actual) != 140:
                    raise ValueError(f'{stem}: pixel byte count')
                words = struct.unpack('<35I', actual)
                for y in range(5):
                    for x in range(7):
                        low = (17 * seed + 3 * x + 5 * y) % 256
                        middle = (29 * seed + 7 * x + 11 * y) % 256
                        high = (43 * seed + 13 * x + 19 * y) % 256
                        expected = low + 256 * middle + 65536 * high + 0xff000000
                        if words[y * 7 + x] != expected:
                            raise ValueError(f'{stem}: pixel ({x},{y}) actual={words[y * 7 + x]:08x} expected={expected:08x}')
            stem = f'open-primary-padded-{profile}-{format_}'
            metadata = original(stem + '.metadata.u32.bin')
            if metadata != struct.pack('<6I', 7, 5, format_, 4, 40, 200):
                raise ValueError(f'{stem}: borrowed metadata')
            observed = original(stem + '.actual.bin')
            if len(observed) != 232 or observed[:16] != b'\xa5' * 16 or observed[-16:] != b'\xa5' * 16:
                raise ValueError(f'{stem}: borrowed bounds')
            for y in range(5):
                row = observed[16 + 40 * y:16 + 40 * (y + 1)]
                if row[28:] != b'\xa5' * 12:
                    raise ValueError(f'{stem}: borrowed row {y} padding')
                for x in range(7):
                    expected_bytes = bytes(((68 + 3 * x + 5 * y) % 256,
                        (116 + 7 * x + 11 * y) % 256,
                        (172 + 13 * x + 19 * y) % 256, 255))
                    if row[4 * x:4 * x + 4] != expected_bytes:
                        raise ValueError(f'{stem}: borrowed pixel ({x},{y})')
    return dict(verified=True, profiles=3, formats=3, images=36, pixels=1260,
        borrowed_padding_images=9, failure_frames=78, error_callbacks=78,
        runtime_terminal_releases=4, runtime_terminal_mapping_closures=2,
        originals=rows, checks=int(marker[1]), hardware_admission=False,
        ordinary_runtime_admission=False)


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
    print('DXGI opened primary original pixels PASS profiles=3 images=36 pixels=1260 files=90')


if __name__ == '__main__':
    main()
