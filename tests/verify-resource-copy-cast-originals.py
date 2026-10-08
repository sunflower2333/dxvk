#!/usr/bin/env python3
"""Rebuild every cast-copy byte oracle from the fixed native fixture recipe."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

PAIRS = [(42,41),(41,43),(39,42),(42,67),(67,43),(28,29),
         (29,27),(30,31),(34,36),(11,10),(2,4),(64,61)]
BYTES = {42:4,41:4,43:4,39:4,67:4,28:4,29:4,27:4,30:4,31:4,
         34:4,36:4,11:8,10:8,2:16,4:16,64:1,61:1,87:4,91:4,93:4,92:4}


def recipe():
    result = []
    for profile in range(2):
        for kind in range(1,4):
            for src,dst in PAIRS:
                for operation in range(4):
                    result.append((profile,kind,src,dst,operation))
            if kind == 2:
                for operation in range(4):
                    result.extend([(profile,kind,87,91,operation),(profile,kind,93,92,operation)])
            result.append((profile,kind,42,42,4))
        for operation in range(4):
            result.append((profile,0,0,0,operation))
        result.append((profile,0,0,0,4))
    assert len(result) == 320
    return result


def shape(kind):
    return (8 if kind else 128, 4 if kind > 1 else 1,
            4 if kind == 3 else 1, 3 if kind else 1,
            2 if kind in (1,2) else 1)


def initial(kind,texel,seed):
    width,height,depth,mips,arrays = shape(kind)
    result = []
    for sub in range(mips*arrays):
        mip = sub % mips
        w,h,d = max(1,width>>mip),max(1,height>>mip),max(1,depth>>mip)
        result.append(bytearray((seed+sub*37+z*23+y*11+x*3+lane*19)&255
            for z in range(d) for y in range(h) for x in range(w) for lane in range(texel)))
    if seed == 0x17 and texel == 4:
        result[0][:16] = struct.pack('<IIII',0x3f800000,0x80000000,0x7fc12345,0x00000001)
    return result


def expected(kind,texel,operation):
    source = initial(kind,texel,0x17)
    if operation < 2:
        return b''.join(source)
    destination = initial(kind,texel,0xa3)
    if operation < 4:
        width,height,depth,mips,arrays = shape(kind)
        src_sub,dst_sub = (1 if kind else 0),(3 if kind in (1,2) else 0)
        left,right = (1,3) if kind else (16,48)
        rows,slices = (2 if kind > 1 else 1),(2 if kind == 3 else 1)
        x,y,z = (3 if kind else 80),(1 if kind > 1 else 0),(1 if kind == 3 else 0)
        src_w,src_h = max(1,width>>(src_sub%mips)),max(1,height>>(src_sub%mips))
        dst_w,dst_h = max(1,width>>(dst_sub%mips)),max(1,height>>(dst_sub%mips))
        # Copy whole rows from source mip1 into destination mip0. The raw
        # output includes every untouched subresource and region sentinel.
        for dz in range(slices):
            for dy in range(rows):
                start = ((dz*src_h+dy)*src_w+left)*texel
                offset = (((z+dz)*dst_h+y+dy)*dst_w+x)*texel
                length = (right-left)*texel
                destination[dst_sub][offset:offset+length] = source[src_sub][start:start+length]
    return b''.join(destination)


def verify(directory):
    directory = Path(directory)
    entries = sorted(p.name for p in directory.iterdir())
    wanted = ['copy-cast-manifest.txt'] + [f'copy-cast-{index:03}-{side}.bin'
        for index in range(320) for side in ('native','public')]
    if entries != sorted(wanted):
        raise ValueError('missing, extra, or renamed originals')
    if any(p.is_symlink() or not p.is_file() for p in directory.iterdir()):
        raise ValueError('originals must be regular files')
    rows = (directory/'copy-cast-manifest.txt').read_text(encoding='ascii').splitlines()
    if len(rows) != 320:
        raise ValueError('manifest row count')
    pins,observed_bytes = [],0
    for index,(profile,kind,src,dst,operation) in enumerate(recipe()):
        texel = BYTES[dst] if kind else 1
        oracle = expected(kind,texel,operation)
        metadata = [index,profile,kind,src,dst,operation,*shape(kind),texel,0xa3,len(oracle)]
        if [int(word) for word in rows[index].split()] != metadata:
            raise ValueError(f'fixed original metadata changed at {index}')
        for side in ('native','public'):
            name = f'copy-cast-{index:03}-{side}.bin'
            raw = (directory/name).read_bytes()
            if raw != oracle:
                raise ValueError(f'byte oracle mismatch: {name}')
            pins.append(dict(name=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
        observed_bytes += len(oracle)
    return dict(verified=True,observations=320,raw_files=640,
                bytes_each_native_public=observed_bytes,files=pins,
                scope='original-byte-reader; producer execution and hardware require independent evidence')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args = parser.parse_args()
    result = verify(args.directory)
    if args.output:
        with args.output.open('x',encoding='utf-8') as file:
            json.dump(result,file,indent=2)
            file.write('\n')
    print(f"Resource copy cast originals passed: 320 observations, 640 raw files, {result['bytes_each_native_public']} bytes each native/public")


if __name__ == '__main__':
    main()
