#!/usr/bin/env python3
"""Recompute all scoped cube-array mip words from face/mip/coordinate arithmetic."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def expected(readback, face, mip, index):
    color = 0xff000000 | (face + 1) * 0x30303
    if mip == 1:
        return color
    scopes = ((6, 1, 3), (0, 3, 4), (12, 1, 2), (6, 2, 1), None)
    scope = scopes[readback]
    if scope is not None:
        first, cubes, levels = scope
        if first <= face < first + cubes * 6 and 2 <= mip < 1 + levels:
            return color
    return 0xff000000 | (face << 18) | (mip << 14) | (index + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    rows, texels = [], 0
    for readback in range(5):
        for face in range(18):
            for mip in range(5):
                edge = max(1, 16 >> mip)
                stem = f'cube-array-readback-{readback}-face-{face}-mip-{mip}'
                metadata = (args.directory / (stem + '.metadata')).read_bytes()
                assert len(metadata) == 24
                desc = struct.unpack('<6I', metadata)
                assert desc[:4] == (readback, face * 5 + mip, edge, edge), stem
                assert desc[4] >= edge * 4, stem
                data = (args.directory / (stem + '.words')).read_bytes()
                assert len(data) == edge * edge * 4, stem
                actual = struct.unpack(f'<{edge * edge}I', data)
                assert all(word == expected(readback, face, mip, index)
                           for index, word in enumerate(actual)), stem
                texels += len(actual)
                rows.append(dict(readback=readback, face=face, mip=mip, texels=len(actual),
                                 words_sha256=hashlib.sha256(data).hexdigest(),
                                 metadata_sha256=hashlib.sha256(metadata).hexdigest()))
    assert len(rows) == 450 and texels == 30690
    assert len(list(args.directory.glob('cube-array-readback-*.words'))) == 450
    assert len(list(args.directory.glob('cube-array-readback-*.metadata'))) == 450
    proof = dict(schema='native-cube-array-scoped-mips-independent-arithmetic-v1',
                 verified=True, readbacks=5, subresources=450, texels=30690,
                 originals=rows, backend='Microsoft WARP reference',
                 real_viogpu=False, ordinary_runtime_admission=False,
                 oracle='face/mip/index sentinel and literal uniform face-color arithmetic')
    with args.output.open('x') as stream:
        json.dump(proof, stream, indent=2); stream.write('\n')
    print(json.dumps(dict(verified=True, subresources=450, texels=30690,
                          ordinary_runtime_admission=False)))


if __name__ == '__main__':
    main()
