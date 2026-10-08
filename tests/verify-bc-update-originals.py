#!/usr/bin/env python3
"""Reopen actual native/public BC update rows; derive the byte oracle locally."""
import argparse
import hashlib
import json
from pathlib import Path


def pattern(fmt, profile, cube, sub, step, y, x):
    return (17 * fmt + 41 * profile + 13 * cube + 29 * sub + 53 * step + 7 * y + x) & 255


def bytes_per_block(fmt):
    assert 70 <= fmt <= 84
    return 8 if fmt <= 72 or 79 <= fmt <= 81 else 16


def shapes(cube, sub):
    mip = sub % 5
    return max(1, (16 if cube else 24) >> mip), max(1, 16 >> mip)


def expected_rows(profile, fmt, cube, sub):
    width, height = shapes(cube, sub)
    block_bytes = bytes_per_block(fmt)
    stride = ((width + 3) // 4) * block_bytes
    rows = (height + 3) // 4
    expected = bytearray(pattern(fmt, profile, cube, sub, 1, y, x)
                         for y in range(rows) for x in range(stride))
    last_layer = 5 if cube else 1
    # Independently reconstruct the four update locations used by the fixture.
    patches = {5 * last_layer: (4, 4, 12, 12, 2),
               5 * last_layer + 3: (0, 0, 2 if cube else 3, 2, 4),
               5 * last_layer + 4: (0, 0, 1, 1, 5)}
    if not cube:
        patches[5 * last_layer + 2] = (4, 0, 6, 4, 3)
    if sub in patches:
        left, top, right, bottom, step = patches[sub]
        patch_row = ((right - left + 3) // 4) * block_bytes
        for y in range((bottom - top + 3) // 4):
            start = (top // 4 + y) * stride + left // 4 * block_bytes
            expected[start:start + patch_row] = bytes(pattern(fmt, profile, cube, sub, step, y, x)
                                                      for x in range(patch_row))
    return width, height, block_bytes, stride, rows, bytes(expected)


def verify(directory):
    observed_names = set()
    pins = []
    snapshots = observations = 0
    cases = [(profile, fmt, 0) for profile in (10, 11) for fmt in range(70, 85)]
    cases += [(profile, fmt, 1) for profile in (10, 11) for fmt in (71, 84)]
    for profile, fmt, cube in cases:
        for phase in (0, 1):
            for sub in range(30 if cube else 10):
                name = f'bc-update-{profile}-{fmt}-{cube}-{phase}-{sub}'
                width, height, block_bytes, row_bytes, rows, expected = expected_rows(profile, fmt, cube, sub)
                layout_path = directory / (name + '.layout.json')
                layout_bytes = layout_path.read_bytes()
                layout = json.loads(layout_bytes)
                fixed = dict(profile=profile, format=fmt, cube=cube, phase=phase, subresource=sub,
                             width=width, height=height, blockBytes=block_bytes, rowBytes=row_bytes, rows=rows)
                assert set(layout) == set(fixed) | {'nativePitch', 'publicPitch'}, name
                assert all(type(layout[key]) is int and layout[key] == value for key, value in fixed.items()), name
                observed_names.add(layout_path.name)
                pins.append(dict(name=layout_path.name, bytes=len(layout_bytes), sha256=hashlib.sha256(layout_bytes).hexdigest()))
                for suffix, pitch_key in (('native', 'nativePitch'), ('public', 'publicPitch')):
                    pitch = layout[pitch_key]
                    assert type(pitch) is int and row_bytes <= pitch <= (1 << 32) - 1, name
                    path = directory / (name + '.' + suffix + '.bin')
                    data = path.read_bytes()
                    assert len(data) == (rows - 1) * pitch + row_bytes, name
                    for y in range(rows):
                        assert data[y * pitch:y * pitch + row_bytes] == expected[y * row_bytes:(y + 1) * row_bytes], (name, suffix, y)
                    observed_names.add(path.name)
                    pins.append(dict(name=path.name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
                    observations += len(expected)
                snapshots += 1
    assert snapshots == 840 and observations == 130944
    assert {path.name for path in directory.iterdir()} == observed_names
    assert len(pins) == 2520
    return dict(schema=1, cases=len(cases), snapshots=snapshots,
                compressed_byte_observations=observations, original_files=len(pins), originals=pins,
                scope='same-format BC1-5 D3D10/11 native/public CPU WARP updates; no hardware admission')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.directory)
    with args.output.open('x') as output:
        json.dump(result, output, indent=2)
        output.write('\n')
    print('PASS BC update originals: cases=34 snapshots=840 bytes=130944 files=2520')


if __name__ == '__main__':
    main()
