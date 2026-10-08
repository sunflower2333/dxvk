#!/usr/bin/env python3
"""Literal full-storage oracle for the separate Texture1D depth-copy fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

MARKER = re.compile(r'Depth stencil 1D regional copy PASS checks=([1-9][0-9]*) profiles=3 families=2 copies=22 rejections=68 noops=12 snapshots=68 pixels=1360 bytes_each_native_public=4080 raw_files=204 hardware_admission=0')


def recipe():
    for profile in range(3):
        for family in range(2):
            for case in range(12):
                if case != 8 or profile == 0:
                    yield profile, family, case


def shape(profile, family, case):
    actual = 1-family if case == 6 else family
    stride = (2, 4)[actual]
    storage = (53, 39)[actual]
    depth = (55, 40)[actual]
    fmt = depth if case in (0, 5, 8) or (case in (1, 2, 3, 4) and profile > 0) else storage
    seed = {0:17, 5:17, 6:67, 7:107, 8:131, 9:47, 10:47, 11:71}.get(case, 93)
    return actual, fmt, stride, seed, storage


def plane(family, seed, subresource):
    result = bytearray()
    for x in range(7 if subresource % 2 == 0 else 3):
        if family == 0:
            result.extend(struct.pack('<H', 0x2000 + seed*64 + subresource*31 + x*17))
        else:
            result.extend(struct.pack('<f', 0.25 + ((seed + subresource*3 + x) % 128)/256))
    return bytes(result)


def expected(profile, family, case):
    actual, _, stride, seed, _ = shape(profile, family, case)
    planes = [plane(actual, seed, sub) for sub in range(4)]
    if case in (2, 3, 4):
        planes[2] = plane(family, 17, 0)
    if case in (3, 4) or (case == 5 and profile > 0):
        planes[3] = plane(family, 17, 1)
    if case == 10:
        out = bytearray(planes[2])
        out[2*stride:5*stride] = plane(family, 71, 0)[stride:4*stride]
        planes[2] = bytes(out)
    return b''.join(planes)


def stem(row):
    return 'depth-stencil-copy-1d-' + '-'.join(map(str, row))


def verify(directory, stdout):
    directory = Path(directory)
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('A real original directory is required')
    markers = [line for line in stdout.splitlines() if line.startswith('Depth stencil 1D regional copy PASS')]
    if len(markers) != 1 or MARKER.fullmatch(markers[0]) is None:
        raise ValueError('Unique exact 1D fixture marker required')
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
        _, fmt, stride, seed, storage = shape(*row)
        prefix = stem(row)
        path = directory/(prefix+'.metadata.u32.bin')
        raw = path.read_bytes()
        if len(raw) != 80:
            raise ValueError('Exact 20-word metadata extent: '+prefix)
        metadata = struct.unpack('<20I', raw)
        if metadata[:12] != (7, 1, 2, 2, profile, family, case, fmt, stride, 4, seed, storage):
            raise ValueError('Fixed literal metadata differs: '+prefix)
        for pitches in (metadata[12:16], metadata[16:20]):
            for sub, pitch in enumerate(pitches):
                if pitch < (7 if sub % 2 == 0 else 3)*stride:
                    raise ValueError('Mapped pitch does not contain logical row: '+prefix)
        originals.append(dict(name=path.name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        oracle = expected(*row)
        if len(oracle) != 20*stride:
            raise AssertionError('Internal fixed 1D recipe extent')
        for suffix in ('.native.bin', '.public.bin'):
            path = directory/(prefix+suffix)
            raw = path.read_bytes()
            if raw != oracle:
                raise ValueError('Full depth storage oracle mismatch: '+path.name)
            originals.append(dict(name=path.name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        bytes_each_role += len(oracle)
        pixels += 20
    if (len(rows), len(originals), pixels, bytes_each_role) != (68, 204, 1360, 4080):
        raise AssertionError('Fixed observation totals')
    return dict(verified=True, profiles=3, families=2, copies=22, rejections=68, noops=12,
                snapshots=68, pixels=1360, bytes_each_native_public=4080, byte_observations=8160,
                raw_files=204, originals=originals, hardware_admission=False, registration=False,
                scope='Texture1D D16/D32 whole depth copies and ordinary R16/R32 typeless color-box controls; all source/untouched destination bytes; native execution and runtime admission remain separate')


def selftest(root):
    """Independent integer-byte producer and malformed-original controls."""
    root = Path(root)
    root.mkdir()
    directory = root/'synthetic-originals'
    directory.mkdir()
    stdout = 'Depth stencil 1D regional copy PASS checks=1 profiles=3 families=2 copies=22 rejections=68 noops=12 snapshots=68 pixels=1360 bytes_each_native_public=4080 raw_files=204 hardware_admission=0\n'
    for row in recipe():
        profile, family, case = row
        actual = family ^ (case == 6)
        stride = 4 if actual else 2
        storage = 39 if actual else 53
        typed = case in (0, 5, 8) or (1 <= case <= 4 and profile != 0)
        fmt = (40 if actual else 55) if typed else storage
        seed = {0:17, 5:17, 6:67, 7:107, 8:131, 9:47, 10:47, 11:71}.get(case, 93)
        raw = bytearray()
        for sub in range(4):
            selected, value = sub, seed
            if case in (2, 3, 4) and sub == 2:
                selected, value = 0, 17
            if (case in (3, 4) or (case == 5 and profile != 0)) and sub == 3:
                selected, value = 1, 17
            for index in range(7 if sub % 2 == 0 else 3):
                x = index
                chosen, chosen_seed = selected, value
                if case == 10 and sub == 2 and 2 <= index <= 4:
                    chosen, chosen_seed, x = 0, 71, index-1
                if actual:
                    delta = (chosen_seed + 3*chosen + x) & 127
                    word = 0x3e800000+(delta << 17) if delta < 64 else 0x3f000000+((delta-64) << 16)
                    raw.extend(word.to_bytes(4, 'little'))
                else:
                    word = 8192 + 64*chosen_seed + 31*chosen + 17*x
                    raw.extend(bytes((word & 255, word >> 8)))
        prefix = stem(row)
        for suffix in ('.native.bin', '.public.bin'):
            (directory/(prefix+suffix)).write_bytes(raw)
        fixed = [7,1,2,2,profile,family,case,fmt,stride,4,seed,storage]
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
        raise AssertionError('Malformed 1D original was accepted')
    for name, offset in [('depth-stencil-copy-1d-0-0-0.native.bin',0),
                         ('depth-stencil-copy-1d-2-1-5.public.bin',4),
                         ('depth-stencil-copy-1d-1-1-4.native.bin',0),
                         ('depth-stencil-copy-1d-1-0-4.public.bin',-1),
                         ('depth-stencil-copy-1d-0-1-8.native.bin',4),
                         ('depth-stencil-copy-1d-2-1-10.native.bin',48)]:
        path = directory/name
        before = path.read_bytes()
        mutated = bytearray(before);mutated[offset] ^= 1
        path.write_bytes(mutated);rejected();path.write_bytes(before)
        path.write_bytes(before[:-1]);rejected();path.write_bytes(before)
    a=directory/'depth-stencil-copy-1d-1-0-4.native.bin';b=directory/'depth-stencil-copy-1d-1-0-4.public.bin'
    before_a,before_b=a.read_bytes(),b.read_bytes()
    wrong=bytearray(before_a);wrong[0]^=1;a.write_bytes(wrong);b.write_bytes(wrong);rejected()
    a.write_bytes(before_a);b.write_bytes(before_b)
    path=directory/'depth-stencil-copy-1d-2-1-3.metadata.u32.bin';before=path.read_bytes()
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
    extra=directory/'depth-stencil-copy-1d-extra.native.bin';extra.symlink_to(a);rejected();extra.unlink()
    before=a.read_bytes();a.unlink();a.symlink_to(b);rejected();a.unlink();a.write_bytes(before)
    rejected('');rejected(stdout+stdout);rejected(stdout.replace('hardware_admission=0','hardware_admission=1'))
    assert mutations == 38
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
