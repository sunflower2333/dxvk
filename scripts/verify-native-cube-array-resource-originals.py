#!/usr/bin/env python3
"""Recompute every typed cube-array resource word without DDI or fixture helpers."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def require(value, message):
    if not value:
        raise ValueError(message)


def identity(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data),
                sha256=hashlib.sha256(data).hexdigest())


def wanted_word(profile, readback, face, mip, x, y, edge):
    # Face and mip encode independent byte fields; the low field numbers each
    # texel in row order. The transfer readback modifies two disjoint regions.
    if profile == 1 and readback == 3:
        if face == 6 and mip == 1 and 1 <= x < 3 and 1 <= y < 3:
            return 0xff0000ff
        if face == 17 and mip == 0 and 4 <= x < 6 and 3 <= y < 5:
            source_x, source_y = x - 3, y - 1
            return 0xff000000 | (source_y * 7 + source_x + 1)
    return 0xff000000 | (face * 65536) | (mip * 4096) | (y * edge + x + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(args.directory.is_dir(), 'Original readback directory is absent')
    require(not args.stdout.is_symlink(), 'Original stdout must be a regular file')
    stdout = args.stdout.read_text()
    require('BACKEND=WARP; production_DDI=true; VIOGPU=false; registration=false' in stdout,
            'Actual production-reference scope marker differs')
    pattern = (r'(?m)^PASS cube-array resource\r?\nprofiles=2\r?\ncases=7\r?\n'
               r'checks=(\d+)\r?\ntexels=3540\r?\nfailures=56\r?$')
    summaries = list(re.finditer(pattern, stdout))
    require(len(summaries) == 1 and int(summaries[0].group(1)) > 3540,
            'Actual 7-case/3540-texel/56-failure summary is absent or ambiguous')

    # Fixed workloads: base10 single cube; 10.1 single/two/three cube arrays;
    # a separate three-cube transfer readback. Each has logical 7/3/1 mip edges.
    shapes = ((0, 0, 6), (1, 0, 6), (1, 1, 12), (1, 2, 18), (1, 3, 18))
    edges = (7, 3, 1)
    names, rows, texels = set(), [], 0
    for profile, readback, faces in shapes:
        for face in range(faces):
            for mip, edge in enumerate(edges):
                subresource = face * 3 + mip
                stem = f'cube-array-p{profile}-r{readback}-s{subresource}'
                paths = {suffix: args.directory / (stem + '.' + suffix)
                         for suffix in ('metadata', 'actual', 'expected')}
                for path in paths.values():
                    require(path.is_file() and not path.is_symlink(),
                            f'Original file absent or not regular: {path.name}')
                    names.add(path.name)
                metadata = paths['metadata'].read_bytes()
                require(len(metadata) == 32, f'{stem}: metadata is not eight original UINTs')
                desc = struct.unpack('<8I', metadata)
                require(desc[:6] == (profile, readback, subresource, face, mip, edge),
                        f'{stem}: profile/readback/subresource/face/mip/edge differs')
                require(desc[6] >= edge * 4, f'{stem}: original row pitch cannot cover a row')
                size = edge * edge * 4
                actual = paths['actual'].read_bytes()
                saved_expected = paths['expected'].read_bytes()
                require(len(actual) == len(saved_expected) == size,
                        f'{stem}: original pixel byte length differs')
                wanted = [wanted_word(profile, readback, face, mip, x, y, edge)
                          for y in range(edge) for x in range(edge)]
                observed = struct.unpack(f'<{edge * edge}I', actual)
                for index, (seen, reference) in enumerate(zip(observed, wanted)):
                    require(seen == reference,
                            f'{stem}: xy={index % edge}/{index // edge} '
                            f'actual={seen:08x} arithmetic={reference:08x}')
                # Saved expected bytes are checked only after deriving and
                # comparing the independent word oracle above.
                require(saved_expected == struct.pack(f'<{edge * edge}I', *wanted),
                        f'{stem}: saved expected bytes differ from independent arithmetic')
                texels += edge * edge
                rows.append(dict(profile=profile, readback=readback, face=face, mip=mip,
                                 subresource=subresource, edge=edge, texels=edge * edge,
                                 row_pitch=desc[6], depth_pitch=desc[7],
                                 originals={name: identity(path) for name, path in paths.items()}))
    discovered = {path.name for path in args.directory.glob('cube-array-p*')}
    require(discovered == names, 'Missing, extra or ambiguously named original readback files')
    require(len(rows) == 180 and len(names) == 540 and texels == 3540,
            'Original 180-subresource/540-file/3540-texel totals differ')
    proof = dict(schema='native-cube-array-resource-independent-arithmetic-v1',
                 verified=True, profiles=2, cases=7, readbacks=5, subresources=180,
                 texels=3540, deliberate_failures=56, original_readback_files=540,
                 actual_checks=int(summaries[0].group(1)), stdout=identity(args.stdout),
                 originals=rows, oracle='face/mip/xy arithmetic and literal red/cross-cube rectangles',
                 backend='WARP production DDI reference', real_viogpu=False,
                 ordinary_runtime_admission=False)
    with args.output.open('x') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(verified=True, profiles=2, cases=7, readbacks=5,
                          subresources=180, texels=3540, deliberate_failures=56,
                          original_readback_files=540, ordinary_runtime_admission=False)))


if __name__ == '__main__':
    main()
