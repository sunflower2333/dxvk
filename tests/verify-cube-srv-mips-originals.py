#!/usr/bin/env python3
"""Compare retained WARP cube-view readbacks with an absolute-face/mip oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def cases():
    for profile, mips, cubes in [('10_1', 4, 1), ('11', 5, 3)]:
        index = 0
        for first in range(cubes):
            for count in range(1, cubes - first + 1):
                for mip in range(mips):
                    remaining = mips - mip
                    for requested in [1, remaining, 0xffffffff]:
                        selected = remaining if requested == 0xffffffff else requested
                        # Walk absolute resource mip and face coordinates. This
                        # oracle does not call the production descriptor helper.
                        values = [struct.pack('<f', 1000 + absolute_face * 32 + absolute_mip)
                                  for cube in range(first, first + count)
                                  for absolute_mip in range(mip, mip + selected)
                                  for absolute_face in range(cube * 6, cube * 6 + 6)]
                        yield profile, index, (mip, requested, first * 6, count, selected, len(values)), b''.join(values)
                        index += 1


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def verify(directory):
    rows = []
    expected_files = set()
    words = 0
    for profile, index, metadata, expected in cases():
        stem = 'cube-srv-%s-%03d.' % (profile, index)
        meta = directory / (stem + 'metadata')
        native = directory / (stem + 'native.words')
        public = directory / (stem + 'public.words')
        assert meta.read_bytes() == struct.pack('<6I', *metadata), 'original cube/mip case metadata differs: ' + stem
        assert native.read_bytes() == expected, 'production DDI readback differs from absolute face/mip oracle: ' + stem
        assert public.read_bytes() == expected, 'original public WARP readback differs from absolute face/mip oracle: ' + stem
        words += len(expected) // 4
        rows.append(dict(profile=profile, index=index, metadata=list(metadata), words=len(expected) // 4,
                         originals=[pin(p) for p in [meta, native, public]]))
        expected_files.update(p.name for p in [meta, native, public])
    for profile in ['10_1', '11']:
        source = directory / ('cube-srv-%s-000.hlsl' % profile)
        binary = directory / ('cube-srv-%s-000.dxbc' % profile)
        assert b'TextureCubeArray<float>' in source.read_bytes() and b'SampleLevel' in source.read_bytes()
        dxbc = binary.read_bytes()
        assert len(dxbc) >= 32 and dxbc[:4] == b'DXBC', 'missing original FXC DXBC'
        assert struct.unpack_from('<I', dxbc, 24)[0] == len(dxbc), 'original FXC container size differs'
        expected_files.update([source.name, binary.name])
    actual = {p.name for p in directory.glob('cube-srv-*') if p.is_file()}
    assert actual == expected_files, 'original cube file set has missing or unexpected members'
    assert len(rows) == 102 and words == 2244 and len(expected_files) == 310
    return dict(schema='typed-cube-srv-mip-originals-v1', passed=True, backend='WARP',
                production_ddi_views=102, native_words=words, original_public_words=words,
                original_files=len(expected_files), hardware_acceptance=False, cases=rows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    parser.add_argument('--stdout', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.directory)
    text = args.stdout.read_text(encoding='utf-8-sig')
    assert all(marker in text for marker in ['BACKEND=WARP', 'production_DDI=true',
                                            'hardware_admission=false', 'registration=false'])
    assert re.search(r'(?m)^native D3D10\.1/D3D11 cube SRV mip ranges verified checks=\d+ views=102 words=2244 callbacks=36 WARP controls\r?$', text)
    result['original_stdout'] = pin(args.stdout)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
