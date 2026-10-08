#!/usr/bin/env python3
"""Verify typed Blt originals with independent rational pixel-center sampling."""
import argparse
from fractions import Fraction
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct


def require(value, message):
    if not value:
        raise ValueError(message)


def original(path, size=None):
    require(path.is_file() and not path.is_symlink(), f'Not a regular original: {path}')
    data = path.read_bytes()
    require(size is None or len(data) == size, f'{path.name}: original byte count differs')
    return data, dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def case_shape(case):
    # Fixed workload dimensions, independently of fixture output metadata.
    widths = (8, 6, 8, 6, 5, 14, 8, 8, 8, 4)
    heights = (6, 8, 6, 8, 4, 10, 6, 6, 6, 4)
    rw = (6, 4, 6, 4, 3, 12, 6, 6, 4, 4)[case]
    rh = (4, 6, 4, 6, 2, 8, 4, 4, 4, 4)[case]
    return widths[case], heights[case], 2 if case == 8 else 0 if case == 9 else 1, 0 if case == 9 else 1, rw, rh


def pixel(case, x, y):
    _, _, left, top, width, height = case_shape(case)
    if x < left or y < top or x >= left + width or y >= top + height:
        return bytes((8, 16, 40, 255))
    if case == 8:
        return bytes((0, 0, 255, 255))  # Red sample average, BGRA destination.
    u = Fraction(2 * (x - left) + 1, 2 * width)
    v = Fraction(2 * (y - top) + 1, 2 * height)
    if case == 1:
        u, v = 1 - v, u
    elif case == 2:
        u, v = 1 - u, 1 - v
    elif case == 3:
        u, v = v, 1 - u
    # Bilinear interpolation of independent affine channel ramps, clamped at
    # the source edge. Every chosen sample is an exact integer byte, so no
    # rounding tolerance or implementation-dependent expected image is needed.
    sx = min(Fraction(5), max(Fraction(0), u * 6 - Fraction(1, 2)))
    sy = min(Fraction(3), max(Fraction(0), v * 4 - Fraction(1, 2)))
    rgba = (8 + 8 * sx, 16 + 16 * sy, 32 + 8 * (sx + sy), Fraction(255))
    require(all(channel.denominator == 1 for channel in rgba), 'Workload sample is not an exact byte')
    encoded = bytes(int(channel) for channel in rgba)
    return bytes((encoded[2], encoded[1], encoded[0], encoded[3])) if case == 6 else encoded


def program(directory, stage):
    data, identity = original(directory / f'blt-internal-{stage}.dxbc')
    require(len(data) >= 44 and data[:4] == b'DXBC', 'Internal program is not DXBC')
    word = lambda at: struct.unpack_from('<I', data, at)[0]
    require(word(20) == 1 and word(24) == len(data) and word(28) == 3, 'Internal container header differs')
    chunks = {}
    end = 44
    for i in range(3):
        at = word(32 + i * 4)
        require(at >= end and at <= len(data) - 8, 'Internal container offset differs')
        size = word(at + 4); require(size <= len(data) - at - 8, 'Internal container size differs')
        tag = data[at:at + 4]; require(tag not in chunks, 'Duplicate internal chunk')
        chunks[tag] = data[at + 8:at + 8 + size]; end = at + 8 + size
    require(end == len(data) and set(chunks) == {b'ISGN', b'OSGN', b'SHDR'}, 'Internal chunks differ')
    vs = (0x10040, 26, 0x0300005f, 0x001010f2, 0, 0x0300005f, 0x00101032, 1,
          0x04000067, 0x001020f2, 0, 1, 0x03000065, 0x00102032, 1,
          0x05000036, 0x001020f2, 0, 0x00101e46, 0,
          0x05000036, 0x00102032, 1, 0x00101046, 1, 0x0100003e)
    ps = (0x40, 27, 0x04001858, 0x00107000, 0, 0x5555, 0x0300005a, 0x00106000, 0,
          0x03001062, 0x00101032, 1, 0x03000065, 0x001020f2, 0,
          0x0b000048, 0x001020f2, 0, 0x00101046, 1, 0x00107e46, 0,
          0x00106000, 0, 0x4001, 0, 0x0100003e)
    expected = vs if stage == 'vs' else ps
    require(chunks[b'SHDR'] == struct.pack(f'<{len(expected)}I', *expected), 'Internal shader instructions differ')
    return identity


def verify_legacy(directory, stdout):
    require(directory.is_dir(), 'Original directory is absent')
    raw, stdout_identity = original(stdout)
    markers = re.findall(rb'(?m)^DXGI Blt PASS checks=(\d+) profiles=3 snapshots=30 '
                         rb'pixels=1536 hardware_admission=0\r?$', raw)
    require(len(markers) == 1 and int(markers[0]) > 1536, 'Blt marker is absent or ambiguous')
    programs = [program(directory, stage) for stage in ('vs', 'ps')]
    names = {'blt-internal-vs.dxbc', 'blt-internal-ps.dxbc'}
    images, uploaded = [], []
    total = 0
    for profile in range(3):
        for case in range(10):
            width, height, *_ = case_shape(case)
            stem = f'blt-{profile}-{case}'
            actual, actual_identity = original(directory / f'{stem}.actual.u32.bin', width * height * 4)
            metadata, metadata_identity = original(directory / f'{stem}.metadata.u32.bin', 24)
            seen_width, seen_height, seen_case, subresource, format_value, row_pitch = struct.unpack('<6I', metadata)
            require((seen_width, seen_height, seen_case, subresource, format_value)
                    == (width, height, case, 3 if case == 9 else 0, 87 if case in (6, 8) else 28),
                    f'{stem}: shape, format or subresource differs')
            require(row_pitch >= width * 4, f'{stem}: map pitch is short')
            for y in range(height):
                for x in range(width):
                    at = (y * width + x) * 4
                    require(actual[at:at + 4] == pixel(case, x, y), f'{stem}: exact pixel {x}/{y} differs')
            names.update((f'{stem}.actual.u32.bin', f'{stem}.metadata.u32.bin'))
            count = width * height; total += count
            images.append(dict(profile=profile, case=case, pixels=count, row_pitch=row_pitch,
                               actual=actual_identity, metadata=metadata_identity))
        stem = f'blt-present-{profile}'
        metadata, metadata_identity = original(directory / f'{stem}.metadata.u32.bin', 16)
        width, height, pitch, size = struct.unpack('<4I', metadata)
        require((width, height) == (8, 6) and pitch >= 32 and size == pitch * height, 'Present allocation shape differs')
        actual, actual_identity = original(directory / f'{stem}.actual.bin', size + 32)
        require(actual[:16] == actual[-16:] == b'\xa5' * 16, 'Present boundary guards changed')
        for y in range(height):
            for x in range(width):
                at = 16 + y * pitch + x * 4
                require(actual[at:at + 4] == pixel(8, x, y), 'Present allocation pixel differs')
            require(actual[16 + y * pitch + 32:16 + (y + 1) * pitch] == b'\xa5' * (pitch - 32),
                    'Present allocation row padding changed')
        names.update((f'{stem}.actual.bin', f'{stem}.metadata.u32.bin'))
        uploaded.append(dict(profile=profile, pixels=48, actual=actual_identity, metadata=metadata_identity))
    require(total == 1536 and len(names) == 68, 'Blt workload totals differ')
    require({path.name for path in directory.glob('blt-*')} == names, 'Missing, extra or ambiguous Blt originals')
    return dict(schema='native-dxgi-blt-originals-v1', verified=True, profiles=3, snapshots=30,
                pixels=total, uploaded_pixels=144, original_files=68, programs=programs, images=images,
                uploaded=uploaded, stdout=stdout_identity, backend='WARP production-DDI reference',
                hardware_admission=False, primary_scanout_admission=False,
                oracle='exact rational bilinear pixel centers, CCW rotation and literal encoded RGBA/BGRA')


def verify(directory, stdout):
    # Preserve every legacy byte/program oracle and exact 68-file closure;
    # require the additional identity phase independently in the same run.
    proof = verify_legacy(directory, stdout)
    path = Path(__file__).with_name('verify-native-identity-blt-originals.py')
    spec = importlib.util.spec_from_file_location('native_identity_blt_originals', path)
    require(spec is not None and spec.loader is not None, 'Identity reader is absent')
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    proof['identity_copy'] = reader.verify(directory, stdout)
    path = Path(__file__).with_name('verify-native-private-transfer-originals.py')
    spec = importlib.util.spec_from_file_location('native_private_transfer_originals', path)
    require(spec is not None and spec.loader is not None, 'Private transfer reader is absent')
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    proof['private_transfer'] = reader.verify(directory, stdout)
    return proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    proof = verify(args.directory, args.stdout)
    with args.output.open('x') as stream:
        json.dump(proof, stream, indent=2); stream.write('\n')
    print(json.dumps({key: proof[key] for key in ('verified', 'profiles', 'snapshots', 'pixels', 'uploaded_pixels', 'original_files', 'hardware_admission')}))


if __name__ == '__main__':
    main()
