#!/usr/bin/env python3
"""Verify identity-Blt originals against a fixed, independent byte recipe."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


MARKER = (b'DXGI identity Blt PASS profiles=3 formats=4 snapshots=120 '
          b'pixels=15624 hardware_admission=0')
FORMATS = (28, 29, 87, 88)
SEED = 4
SENTINEL = 0xff281008


def require(value, message):
    if not value:
        raise ValueError(message)


def original(path, size=None):
    require(path.is_file() and not path.is_symlink(), f'Not a regular original: {path}')
    raw = path.read_bytes()
    require(size is None or len(raw) == size, f'{path.name}: original byte count differs')
    return raw, dict(path=str(path), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def recipe():
    # Dimensions and destinations are fixed by the workload, never obtained
    # from the observed metadata or a production Blt eligibility helper.
    yield 0, 0, 11, 8, 2, 1
    for subresource in range(4):
        width, height = (22, 16) if subresource % 2 == 0 else (11, 8)
        yield 1, subresource, width, height, 2, 1
    yield 2, 0, 7, 5, 0, 0
    yield 3, 0, 11, 8, 0, 0
    yield 4, 0, 7, 5, 0, 0
    # Native-helper supplement: one backend image, source mip/array subresource
    # 1 copied to subresource 3. Both full 11x8 snapshots preserve exact words.
    yield 5, 1, 11, 8, 0, 0
    yield 5, 3, 11, 8, 0, 0


def expected_word(case, subresource, x, y, left, top):
    copied = case in (0, 2, 5) or (case == 1 and subresource == 3)
    if not copied or (case != 5 and not (left <= x < left + 7 and top <= y < top + 5)):
        return SENTINEL
    x -= left
    y -= top
    # This is physical storage order for all four actual formats. In
    # particular X8's fourth byte is compared exactly, without alpha masking.
    return (((64 + SEED * 11 + x * 3 + y * 5) & 255) << 24
            | ((SEED * 13 + x * 17 + y * 7) & 255)
            | (((SEED * 19 + x * 5 + y * 23) & 255) << 8)
            | (((SEED * 29 + x * 11 + y * 13) & 255) << 16))


def verify(directory, stdout):
    directory, stdout = Path(directory), Path(stdout)
    require(directory.is_dir() and not directory.is_symlink(), 'Original directory is absent or linked')
    raw_stdout, stdout_identity = original(stdout)
    marker_lines = [line.removesuffix(b'\r') for line in raw_stdout.split(b'\n')
                    if line.startswith(b'DXGI identity Blt PASS')]
    require(marker_lines == [MARKER], 'Identity Blt marker is absent, changed or ambiguous')
    names = {
        f'identity-blt-{profile}-{format_index}-{case}-{subresource}.{role}.u32.bin'
        for profile in range(3) for format_index in range(4)
        for case, subresource, *_ in recipe() for role in ('actual', 'metadata')
    }
    observed_names = {path.name for path in directory.iterdir()
                      if path.name.startswith('identity-blt-')}
    require(observed_names == names, 'Identity Blt original file closure differs')
    images = []
    total = 0
    for profile in range(3):
        for format_index, format_value in enumerate(FORMATS):
            format_pixels = 0
            for case, subresource, width, height, left, top in recipe():
                stem = f'identity-blt-{profile}-{format_index}-{case}-{subresource}'
                actual, actual_identity = original(directory / f'{stem}.actual.u32.bin', width * height * 4)
                metadata, metadata_identity = original(directory / f'{stem}.metadata.u32.bin', 32)
                seen = struct.unpack('<8I', metadata)
                require(seen[:5] == (width, height, case, subresource, format_value)
                        and seen[6:] == (SEED, profile), f'{stem}: fixed metadata differs')
                row_pitch = seen[5]
                require(row_pitch >= width * 4, f'{stem}: map pitch is short')
                words = struct.unpack(f'<{width * height}I', actual)
                for y in range(height):
                    for x in range(width):
                        expected = expected_word(case, subresource, x, y, left, top)
                        observed = words[y * width + x]
                        require(observed == expected,
                                f'{stem}: pixel ({x},{y}) actual={observed:08x} expected={expected:08x}')
                count = width * height
                format_pixels += count
                images.append(dict(profile=profile, format_index=format_index, format=format_value,
                                   case=case, subresource=subresource, width=width, height=height,
                                   pixels=count, row_pitch=row_pitch,
                                   actual=actual_identity, metadata=metadata_identity))
            require(format_pixels == 1302, 'Identity Blt per-format pixel count differs')
            total += format_pixels
    require(len(images) == 120 and total == 15624 and len(names) == 240,
            'Identity Blt workload totals differ')
    return dict(schema='native-dxgi-identity-blt-originals-v1', verified=True,
                profiles=3, formats=4, snapshots=120, pixels=total, original_files=len(names),
                seed=SEED, images=images, stdout=stdout_identity,
                backend='WARP production-DDI reference', hardware_admission=False,
                ordinary_runtime_admission=False, primary_scanout_admission=False,
                native_helper_alias_snapshots=24,
                oracle='fixed physical 32-bit words including X8 fourth byte; no alpha masking')


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
                     ('verified', 'profiles', 'formats', 'snapshots', 'pixels',
                      'original_files', 'hardware_admission')}))


if __name__ == '__main__':
    main()
