#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Recompute cube face/mip words independently; process/core joins stay separate."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

HELPER = Path(__file__).with_name('verify-d3d11-kmt-graphics-originals.py')
spec = importlib.util.spec_from_file_location('graphics_originals', HELPER)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)


def bits(integer):
    return struct.unpack('<I', struct.pack('<f', integer))[0]


def cube_words(original):
    resources = []
    texels = 0
    shapes = [(7, 3, 12), (7, 3, 12), (8, 4, 12), (8, 4, 12), (8, 4, 12), (7, 3, 6)]
    for index, (base, levels, faces) in enumerate(shapes):
        words = []
        for face in range(faces):
            for mip in range(levels):
                edge = max(1, base >> mip)
                values = []
                for y in range(edge):
                    for x in range(edge):
                        if index in (2, 3, 4):
                            origin = 1 if ((index == 2 and face >= 6 and mip >= 2)
                                            or (index == 3 and face < 6 and mip == 2)) else mip
                            value = 100 * face + 10 * origin + 1
                        elif index == 1 and face == 7 and mip == 1 and x < 2 and y < 2:
                            value = 123456
                        elif index == 1 and face == 11 and mip == 0 and 3 <= x < 5 and 2 <= y < 4:
                            value = 1000 + (y - 1) * 7 + x - 2
                        else:
                            value = 1000 + 100 * face + 10 * mip + y * edge + x
                        values.append(bits(value))
                stem = f'cube-readback-{index}-face-{face}-mip-{mip}'
                original.image(stem, values)
                meta = common.u32(original.read(stem + '.shape-pitches.u32'))
                common.require(len(meta) == 9 and meta[:7] == (index, base, levels, faces, face, mip, edge)
                               and meta[7] >= 4 * edge, 'fixed cube metadata/pitch')
                words.append(values)
                texels += len(values)
        resources.append(words)
    requests = []
    for first, cubes, firstMip, levels in [(0, 1, 0, 3), (6, 1, 1, 2), (0, 2, 0, 3)]:
        requests += [(0, first, cubes, firstMip, relative) for relative in range(levels)]
    for resource, first, levels in [(2, 6, 3), (3, 0, 2), (4, 6, 1)]:
        requests += [(resource, first, 1, 1, relative) for relative in range(levels)]
    requests += [(5, 0, 1, 0, relative) for relative in range(3)]
    sampled = 0
    for index, (resource, first, cubes, firstMip, relative) in enumerate(requests):
        base, levels, faces = shapes[resource]
        mip = firstMip + relative
        edge = max(1, base >> mip)
        count = 6 * cubes
        expected = [resources[resource][(first + face) * levels + mip][(edge // 2) * edge + edge // 2]
                    for face in range(count)] + [0xcdcdcdcd] * (64 - count)
        stem = f'cube-sampled-{index}'
        original.image(stem, expected)
        original.exact(stem + '.shape-relative-mip.u32', common.packed(
            (index, base, levels, faces, first, cubes, firstMip, relative, 64, count)))
        sampled += count
    common.require(len(requests) == 17 and texels == 4830 and sampled == 120, 'independent cube coverage')
    return dict(whole_resource_words=texels,
                sampled_words=sampled, sample_buffer_words=64 * len(requests),
                untouched_sample_tail_words=64 * len(requests) - sampled, readbacks=6, sample_runs=17)


def verify(directory, stdout, source):
    text = stdout.read_text(encoding='ascii')
    common.require(re.search(r'^D3D11_KMT_CUBE_PASS cases=6 readbacks=6 texels=4830 sampled=120 '
                             r'sample_runs=17 balanced=1 hr=00000000 ordinary_runtime_admission=0\r?$', text, re.M),
                   'raw cube PASS summary')
    common.require('DX11_CORE_ERROR' not in text and 'D3D11_KMT_CUBE_FAILURE' not in text,
                   'unexpected raw cube callback or failure')
    original = common.Originals(directory)
    original.shader('cube-point-cs', common.hlsl(source.read_text(), 'SampleSource'), 5)
    counts = cube_words(original)
    original.complete()
    return dict(verified=True, raw_files=len(original.seen), **counts,
                exact_originals=original.rows, stdout_sha256=hashlib.sha256(stdout.read_bytes()).hexdigest(),
                probe_source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                graphics_reader_sha256=hashlib.sha256(HELPER.read_bytes()).hexdigest(),
                process_ownership_verified=False, core_identity_verified=False,
                hardware_acceptance=False, ordinary_runtime_admission=False, target_calls=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--probe-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    proof = verify(args.directory, args.stdout, args.probe_source)
    with args.output.open('x') as output:
        json.dump(proof, output, indent=2)
        output.write('\n')
    print(json.dumps({key: value for key, value in proof.items() if key != 'exact_originals'}, sort_keys=True))


if __name__ == '__main__':
    main()
