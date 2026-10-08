#!/usr/bin/env python3
"""Independent full-storage oracle for whole-subresource depth/stencil copies."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

MARKER = re.compile(r'Depth stencil regional copy PASS checks=([1-9][0-9]*) profiles=3 families=2 copies=16 rejections=74 noops=12 snapshots=50 pixels=4100 bytes_each_native_public=24600 raw_files=150 hardware_admission=0')


def recipe():
    for profile in range(3):
        for family in range(2):
            for case in range(9 if profile == 0 else 8):
                yield profile, family, case


def shape(profile, family, case):
    actual_family = 1-family if case == 6 else family
    seed = 17 if case in (0, 5) else 67 if case == 6 else 107 if case == 7 else 131 if case == 8 else 93
    return actual_family, (44, 19)[actual_family], (4, 8)[actual_family], seed


def plane(family, seed, subresource):
    width, height = (7, 5) if subresource % 2 == 0 else (3, 2)
    result = bytearray()
    for y in range(height):
        for x in range(width):
            stencil = (seed + subresource*13 + x*7 + y*11) % 256
            if family == 0:
                depth = 0x200000 + seed*1024 + subresource*512 + x*17 + y*31
                result.extend(struct.pack('<I', depth + stencil*0x1000000))
            else:
                depth = 0.25 + ((seed + subresource*3 + x + y*5) % 128)/256
                result.extend(struct.pack('<fI', depth, stencil))
    return bytes(result)


def expected(profile, family, case):
    actual_family, _, _, seed = shape(profile, family, case)
    planes = [plane(actual_family, seed, sub) for sub in range(4)]
    if case in (2, 3, 4):
        planes[2] = plane(family, 17, 0)
    if case in (3, 4):
        planes[3] = plane(family, 17, 1)
    if case == 5 and profile > 0:
        planes[3] = plane(family, 17, 1)
    return b''.join(planes)


def stem(row):
    return 'depth-stencil-copy-' + '-'.join(map(str, row))


def verify(directory, stdout):
    directory = Path(directory)
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('A real original directory is required')
    markers = [line for line in stdout.splitlines() if line.startswith('Depth stencil regional copy PASS')]
    if len(markers) != 1 or MARKER.fullmatch(markers[0]) is None:
        raise ValueError('Unique exact fixture marker required')
    rows = list(recipe())
    wanted = {stem(row)+suffix for row in rows for suffix in ('.native.bin', '.public.bin', '.metadata.u32.bin')}
    if {p.name for p in directory.iterdir()} != wanted:
        raise ValueError('Exact original closure: missing, renamed or extra file')
    if any(p.is_symlink() or not p.is_file() for p in directory.iterdir()):
        raise ValueError('Originals must be regular files')
    originals = []
    bytes_each_role = pixels = 0
    for row in rows:
        profile, family, case = row
        actual_family, fmt, stride, seed = shape(*row)
        prefix = stem(row)
        path = directory/(prefix+'.metadata.u32.bin')
        raw = path.read_bytes()
        if len(raw) != 80:
            raise ValueError('Exact 20-word metadata extent: '+prefix)
        metadata = struct.unpack('<20I', raw)
        if metadata[:12] != (7, 5, 2, 2, profile, family, case, fmt, stride, 4, seed, 0):
            raise ValueError('Fixed recipe metadata differs: '+prefix)
        for pitches in (metadata[12:16], metadata[16:20]):
            for sub, pitch in enumerate(pitches):
                width = 7 if sub % 2 == 0 else 3
                if pitch < width*stride:
                    raise ValueError('Mapped pitch does not contain logical row: '+prefix)
        originals.append(dict(name=path.name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        oracle = expected(*row)
        if len(oracle) != 82*(4, 8)[actual_family]:
            raise AssertionError('Internal fixed recipe extent')
        for suffix in ('.native.bin', '.public.bin'):
            path = directory/(prefix+suffix)
            raw = path.read_bytes()
            if raw != oracle:
                raise ValueError('Full depth/stencil storage oracle mismatch: '+path.name)
            originals.append(dict(name=path.name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        bytes_each_role += len(oracle)
        pixels += 82
    if (len(rows), len(originals), pixels, bytes_each_role) != (50, 150, 4100, 24600):
        raise AssertionError('Fixed observation totals')
    return dict(verified=True, profiles=3, families=2, copies=16, rejections=74, noops=12,
                snapshots=50, pixels=4100, bytes_each_native_public=24600, byte_observations=49200,
                raw_files=150, originals=originals, hardware_admission=False, registration=False,
                scope='Full depth/stencil storage, sources and untouched mip/array destinations; source/compiler/process and runtime admission are separate')


def selftest(root):
    """Independent integer-byte producer and mutations; no native execution."""
    root = Path(root)
    root.mkdir()
    directory = root/'synthetic-originals'
    directory.mkdir()
    stdout = 'Depth stencil regional copy PASS checks=1 profiles=3 families=2 copies=16 rejections=74 noops=12 snapshots=50 pixels=4100 bytes_each_native_public=24600 raw_files=150 hardware_admission=0\n'
    for row in recipe():
        profile, family, case = row
        actual = family ^ (case == 6)
        stride = 8 if actual else 4
        seed = {0:17, 5:17, 6:67, 7:107, 8:131}.get(case, 93)
        raw = bytearray()
        for sub in range(4):
            selected, value = sub, seed
            if case in (2, 3, 4) and sub == 2:
                selected, value = 0, 17
            if ((case in (3, 4)) or (case == 5 and profile != 0)) and sub == 3:
                selected, value = 1, 17
            width, height = [(7,5), (3,2)][sub % 2]
            for index in range(width*height):
                x, y = index % width, index // width
                stencil = (value + 13*selected + 7*x + 11*y) & 255
                if actual:
                    delta = (value + 3*selected + x + 5*y) & 127
                    word = 0x3e800000+(delta << 17) if delta < 64 else 0x3f000000+((delta-64) << 16)
                    raw.extend(word.to_bytes(4, 'little'))
                    raw.extend(bytes((stencil, 0, 0, 0)))
                else:
                    word = 0x200000 + 1024*value + 512*selected + 17*x + 31*y
                    raw.extend(word.to_bytes(3, 'little'))
                    raw.append(stencil)
        prefix = stem(row)
        for suffix in ('.native.bin', '.public.bin'):
            (directory/(prefix+suffix)).write_bytes(raw)
        fixed = [7,5,2,2,profile,family,case,19 if actual else 44,stride,4,seed,0]
        pitches = [7*stride+16, 3*stride+16]*2
        (directory/(prefix+'.metadata.u32.bin')).write_bytes(struct.pack('<20I', *(fixed+pitches+pitches)))
    golden = verify(directory, stdout)
    mutations = 0
    def rejected(text=stdout):
        nonlocal mutations
        try:
            verify(directory, text)
        except (ValueError, OSError):
            mutations += 1
            return
        raise AssertionError('Malformed original was accepted')
    for name, offset in [('depth-stencil-copy-0-0-0.native.bin',3),
                         ('depth-stencil-copy-2-1-5.public.bin',4),
                         ('depth-stencil-copy-1-1-4.native.bin',5),
                         ('depth-stencil-copy-1-0-4.public.bin',-1),
                         ('depth-stencil-copy-0-1-8.native.bin',4)]:
        path = directory/name
        before = path.read_bytes()
        mutated = bytearray(before);mutated[offset] ^= 1
        path.write_bytes(mutated);rejected();path.write_bytes(before)
        path.write_bytes(before[:-1]);rejected();path.write_bytes(before)
    a=directory/'depth-stencil-copy-1-0-4.native.bin'; b=directory/'depth-stencil-copy-1-0-4.public.bin'
    before_a,before_b=a.read_bytes(),b.read_bytes()
    wrong=bytearray(before_a);wrong[3]^=1;a.write_bytes(wrong);b.write_bytes(wrong);rejected()
    a.write_bytes(before_a);b.write_bytes(before_b)
    path=directory/'depth-stencil-copy-2-1-3.metadata.u32.bin';before=path.read_bytes()
    for index in range(12):
        words=list(struct.unpack('<20I',before));words[index]+=1
        path.write_bytes(struct.pack('<20I',*words));rejected();path.write_bytes(before)
    for index in (12,15,16,19):
        words=list(struct.unpack('<20I',before));words[index]=0
        path.write_bytes(struct.pack('<20I',*words));rejected();path.write_bytes(before)
    path.write_bytes(before[:-4]);rejected();path.write_bytes(before)
    path.write_bytes(before+b'\0');rejected();path.write_bytes(before)
    extra=directory/'extra.bin';extra.write_bytes(b'x');rejected();extra.unlink()
    before=a.read_bytes();a.unlink();rejected();a.write_bytes(before)
    extra=directory/'depth-stencil-copy-extra.native.bin';extra.symlink_to(a);rejected();extra.unlink()
    before=a.read_bytes();a.unlink();a.symlink_to(b);rejected();a.unlink();a.write_bytes(before)
    rejected('');rejected(stdout+stdout);rejected(stdout.replace('hardware_admission=0','hardware_admission=1'))
    assert verify(directory,stdout)==golden
    (root/'synthetic.stdout.raw').write_text(stdout)
    return dict(verified=True, mutations_rejected=mutations, synthetic_only=True,
                original_bytes_reopened=True, positive=golden, native_execution=False, hardware_admission=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory',type=Path)
    parser.add_argument('--stdout',type=Path)
    parser.add_argument('--selftest',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.selftest is not None:
        result=selftest(args.selftest)
    else:
        if args.directory is None or args.stdout is None:
            parser.error('Provide --directory and the retained process --stdout')
        result=verify(args.directory,args.stdout.read_text(encoding='utf-8-sig'))
    if args.output is not None:
        with args.output.open('x') as stream:
            json.dump(result,stream,indent=2);stream.write('\n')
    print(json.dumps({key:value for key,value in result.items() if key not in ('originals','positive')}))


if __name__=='__main__':
    main()
