#!/usr/bin/env python3
"""Verify every cube-fixture word using face/mip arithmetic, independently of DDI."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def expected(readback, face, mip, edge):
    words = [0xff000000 | (face << 20) | (mip << 16) | (index + 1)
             for index in range(edge * edge)]
    if readback == 1:
        if face == 1 and mip == 1:
            for y in range(2, 4):
                for x in range(1, 4):
                    words[y * edge + x] = 0xff0000ff
        if face == 5 and mip == 0:
            for y in range(2):
                for x in range(3):
                    words[(y + 4) * edge + x + 6] = 0xff010000 | ((y + 1) * 7 + x + 3)
    elif readback == 2 and mip == 1:
        color = 0xff000000 | (0xff if face & 1 else 0) | (0xff00 if face & 2 else 0) | (0xff0000 if face & 4 else 0)
        words = [color] * len(words)
    elif readback in (3, 4, 5):
        selected = {3: 4, 4: 2, 5: 1}[readback]
        if 1 <= mip < 1 + selected:
            words = [0xff000000 | face * 0x20202] * len(words)
    elif readback == 6:
        words = [0x3f000000 if 2 <= face < 5 and mip == 1 else 0] * len(words)
    return words


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    shapes = [(15, 4), (15, 4), (8, 2), (16, 5), (16, 5), (16, 5), (7, 3)]
    files = []
    texels = 0
    for readback, (width, levels) in enumerate(shapes):
        for face in range(6):
            for mip in range(levels):
                edge = max(1, width >> mip)
                stem = f'cube-readback-{readback}-face-{face}-mip-{mip}'
                metadata = (args.directory / (stem + '.metadata')).read_bytes()
                assert len(metadata) == 24
                actual = struct.unpack('<6I', metadata)
                assert actual[:4] == (readback, face * levels + mip, edge, edge), stem
                assert actual[4] >= edge * 4, stem
                data = (args.directory / (stem + '.words')).read_bytes()
                assert len(data) == edge * edge * 4, stem
                observed = list(struct.unpack(f'<{edge * edge}I', data))
                wanted = expected(readback, face, mip, edge)
                assert observed == wanted, (stem, [(i, a, b) for i, (a, b) in enumerate(zip(observed, wanted)) if a != b][:8])
                texels += len(observed)
                files.append(dict(readback=readback, face=face, mip=mip, texels=len(observed),
                                  sha256=hashlib.sha256(data).hexdigest()))
    assert len(files) == 168 and texels == 10380
    assert len(list(args.directory.glob('cube-readback-*.words'))) == 168
    assert len(list(args.directory.glob('cube-readback-*.metadata'))) == 168
    proof = dict(schema='native-cube-originals-independent-arithmetic-v1', verified=True,
                 readbacks=7, subresources=168, texels=10380, files=files,
                 oracle='face/mip/index arithmetic and literal clear values',
                 backend='WARP reference fixture', real_viogpu=False, ordinary_runtime_admission=False)
    with args.output.open('x') as stream:
        json.dump(proof, stream, indent=2); stream.write('\n')
    print(json.dumps(dict(verified=True, readbacks=7, subresources=168, texels=10380)))


if __name__ == '__main__':
    main()
