#!/usr/bin/env python3
"""Independently calculate the complete volume oracle from original readbacks."""
from pathlib import Path
import argparse
import hashlib
import json
import struct


def words(path):
    data = path.read_bytes()
    assert len(data) % 4 == 0, str(path)
    return list(struct.unpack('<' + 'I' * (len(data) // 4), data))


def shape(base, mip):
    return tuple(max(1, value // (2 ** mip)) for value in base)


def pattern(extent, mip):
    w, h, d = extent
    return [0xff000000 + mip * 2 ** 20 + z * 2 ** 12 + y * 2 ** 6 + x
            for z in range(d) for y in range(h) for x in range(w)]


def expected(readback, mip):
    base = (9, 5, 7) if readback < 2 else ((7, 3, 5) if readback < 5 else
            ((8, 4, 8) if readback == 5 else (8, 8, 8)))
    extent = shape(base, mip)
    result = pattern(extent, mip)
    if readback == 1 and mip == 0:
        w, h, _ = extent
        for z in range(3):
            for y in range(2):
                for x in range(3):
                    result[((z + 2) * h + y + 1) * w + x + 2] = 0xff000000 + z * 4096 + y * 64 + x
        for z in range(2):
            for y in range(2):
                for x in range(3):
                    result[((z + 4) * h + y + 2) * w + x + 5] = 0xff000000 + (z + 1) * 4096 + (y + 1) * 64 + x + 1
    elif 2 <= readback <= 4:
        result = [value ^ ((readback - 2) * 0x10101) for value in result]
    elif readback == 5 and mip == 1:
        w, h, d = extent
        for z in range(1, d):
            for y in range(h):
                for x in range(w):
                    result[(z * h + y) * w + x] = 0xff0000ff if z == 1 else 0xff00ff00
    elif readback >= 6:
        end = {6: 4, 7: 3, 8: 2}[readback]
        if 1 <= mip < end:
            result = [0xff00ffff] * len(result)
    return extent, result


def verify(directory):
    originals = sorted(directory.glob('volume-*'))
    assert len(originals) == 309, ('original count', len(originals))
    total_voxels = total_sampled = records = 0
    retained = []
    for profile in range(3):
        for readback in range(9):
            levels = 4 if readback < 2 or readback >= 6 else (1 if readback < 5 else 3)
            for mip in range(levels):
                prefix = directory / ('volume-%u-readback-%u-mip-%u' % (profile, readback, mip))
                extent, oracle = expected(readback, mip)
                actual = words(Path(str(prefix) + '.actual.u32.bin'))
                saved_expected = words(Path(str(prefix) + '.expected.u32.bin'))
                metadata = words(Path(str(prefix) + '.dimensions-pitches.u32.bin'))
                assert actual == oracle and saved_expected == oracle, str(prefix)
                assert len(metadata) == 7 and metadata[:5] == [readback, mip, *extent]
                w, h, d = extent
                assert metadata[5] >= w * 4
                assert d == 1 or metadata[6] >= (h - 1) * metadata[5] + w * 4
                total_voxels += len(actual)
                records += 1
        for z in range(7):
            prefix = directory / ('volume-%u-sampled-slice-%u' % (profile, z))
            oracle = [0xff000000 + z * 4096 + y * 64 + x for y in range(5) for x in range(9)]
            assert words(Path(str(prefix) + '.actual.u32.bin')) == oracle
            assert words(Path(str(prefix) + '.expected.u32.bin')) == oracle
            metadata = words(Path(str(prefix) + '.dimensions-pitches.u32.bin'))
            assert len(metadata) == 4 and metadata[:3] == [z, 9, 5] and metadata[3] >= 36
            total_sampled += len(oracle)
        for stage in ['vs', 'ps']:
            hlsl = directory / ('volume-%u-%s.hlsl' % (profile, stage))
            bytecode = directory / ('volume-%u-%s.dxbc' % (profile, stage))
            data = bytecode.read_bytes()
            assert hlsl.stat().st_size and len(data) >= 32 and data[:4] == b'DXBC'
            assert struct.unpack_from('<I', data, 24)[0] == len(data)
    assert records == 78 and total_voxels == 9138 and total_sampled == 945
    for path in originals:
        data = path.read_bytes()
        retained.append(dict(name=path.name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
    return dict(passed=True, original_files=309, mapped_volume_records=78,
                independently_calculated_voxels=9138, independently_calculated_sampled_pixels=945,
                profiles=3, source_scope='WARP production resource/view/transfer DDI only',
                hardware_acceptance=False, originals=retained)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.directory)
    with args.receipt.open('x') as stream:
        stream.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'originals'}))


if __name__ == '__main__':
    main()
