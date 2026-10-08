#!/usr/bin/env python3
"""Literal reader for the standalone CPU-ICD Vulkan composition probe."""
import argparse
import hashlib
import json
from pathlib import Path

# Fixed encoded block layouts for logical 24x16,12x8,6x4,3x2,1x1 mips.
BLOCKS = ((6,4),(3,2),(2,1),(1,1),(1,1))
CASES = ((3,0,0,0,1,3,1,1,1),(2,0,1,0,2,1,1,1,1),
         (0,2,0,0,1,0,1,1,1),(0,3,0,0,0,0,1,1,1),
         (3,4,0,0,0,0,1,1,1),(2,2,1,0,1,0,1,1,1),
         (0,0,1,1,2,1,1,1,1),(1,2,1,0,0,0,2,1,2))
ROLES = ('source-initial','destination-initial','destination-expected',
         'source-actual','destination-actual','readback-with-guards')

def offsets(block_bytes):
    starts, size = [], 0
    for columns, rows in BLOCKS:
        starts.append(size)
        size += columns * rows * block_bytes
    return starts, size

def initial(seed, block_bytes):
    result = bytearray()
    for layer in range(2):
        for mip, (columns, rows) in enumerate(BLOCKS):
            result.extend((seed + layer*17 + mip*29 + n*7 + n//8) & 255
                          for n in range(columns*rows*block_bytes))
    return bytes(result)

def read_case(directory, block_bytes, case):
    directory = Path(directory)
    wanted = {f'vulkan-bc-{block_bytes}-{case}-{role}.bin' for role in ROLES}
    assert {p.name for p in directory.iterdir()} == wanted, 'exact original directory required'
    source = initial(0x31, block_bytes)
    destination = initial(0xa3, block_bytes)
    expected = bytearray(destination)
    starts, layer_size = offsets(block_bytes)
    sm, dm, sx, sy, dx, dy, columns, rows, layers = CASES[case]
    for layer in ([0,1] if layers == 2 else [1]):
        for row in range(rows):
            for column in range(columns):
                for n in range(block_bytes):
                    si = layer*layer_size + starts[sm] + ((sy+row)*BLOCKS[sm][0]+sx+column)*block_bytes+n
                    di = layer*layer_size + starts[dm] + ((dy+row)*BLOCKS[dm][0]+dx+column)*block_bytes+n
                    expected[di] = source[si]
    literals = (source, destination, bytes(expected), source, bytes(expected),
                b'\xe7'*16 + source + bytes(expected) + b'\xe7'*16)
    originals = []
    for role, literal in zip(ROLES, literals):
        path = directory / f'vulkan-bc-{block_bytes}-{case}-{role}.bin'
        assert path.is_file() and not path.is_symlink(), 'regular original required'
        data = path.read_bytes()
        assert data == literal, f'literal {role} mismatch'
        originals.append(dict(path=str(path.resolve()), bytes=len(data),
                              sha256=hashlib.sha256(data).hexdigest()))
    return dict(case=case, block_bytes=block_bytes, bridge=case not in (5,6),
                source_bytes=len(source), destination_bytes=len(destination),
                source_unchanged=True, untouched_destination_and_guards=True,
                original_files=originals, public_vulkan_composition_only=True,
                embedded_dxvk_execution=False, hardware_admission=False)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', required=True)
    parser.add_argument('--block-bytes', type=int, choices=(8,16), required=True)
    parser.add_argument('--case', type=int, choices=range(8), required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    result = read_case(args.directory, args.block_bytes, args.case)
    out = Path(args.output)
    assert out.resolve().parent != Path(args.directory).resolve(), 'reader JSON must be outside originals'
    assert not out.exists(), 'fresh reader output required'
    out.write_text(json.dumps(result, indent=2)+'\n')
    print(f'Vulkan BC edge originals PASS block_bytes={args.block_bytes} case={args.case} files=6 hardware_admission=0')

if __name__ == '__main__':
    main()
