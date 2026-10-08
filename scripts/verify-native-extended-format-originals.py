#!/usr/bin/env python3
"""Literal independent encoded bytes, SRV decode, resolve and primary wire proof.

Controlled typed-DDI/WARP evidence only. Does not admit hardware or factories.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def require(value, why):
    if not value:
        raise ValueError(why)


def read(path, size, rows):
    require(path.is_file() and not path.is_symlink(), str(path))
    raw = path.read_bytes()
    require(len(raw) == size, path.name + ': byte count')
    rows.append(dict(path=str(path.resolve()), bytes=size,
                     sha256=hashlib.sha256(raw).hexdigest()))
    return raw


def literal(scene, x, y, x8):
    if scene <= 4:
        if scene == 2:
            x, y = 5 - y, x
        if scene == 3:
            r, g, b, a = 12 + 16*x, 24 + 32*y, 40 + 16*(x+y), 28 + 16*x + 32*y
        else:
            r, g, b, a = 8 + 8*x, 16 + 16*y, 32 + 8*(x+y), 16 + 8*x + 16*y
        if scene == 4:
            r, b = b, r
            if x8:
                a = 255
        return (a << 24) | (b << 16) | (g << 8) | r
    if scene == 5:
        return 0x408000ff  # Linear BGRA RTV: B=255 G=0 R=128 A=64.
    if scene == 6:
        return 0x40bc00ff  # sRGB BGRA RTV: B=255 G=0 R=188 A=64.
    if scene == 7:
        return 0xffff0080 if x8 else 0x40ff0080  # SRV decodes188 to128; X=1.
    if scene == 10:
        return 0xff281008  # Non-PRESENT DDI rejection leaves destination intact.
    return 0xff808080  # Disjoint black/white samples averaged in encoded domain.


def verify(blt, primary, blt_stdout, primary_stdout):
    require(blt_stdout.read_bytes() in (
        b'DXGI extended Blt PASS profiles=3 formats=2 images=57 pixels=1260 hardware_admission=0\n',
        b'DXGI extended Blt PASS profiles=3 formats=2 images=57 pixels=1260 hardware_admission=0\r\n'), 'Blt marker')
    require(primary_stdout.read_bytes() in (
        b'DXGI extended primary PASS profiles=3 images=18 pixels=576 hardware_admission=0\n',
        b'DXGI extended primary PASS profiles=3 images=18 pixels=576 hardware_admission=0\r\n'), 'primary marker')
    rows, blt_names, primary_names = [], set(), set()
    pixels, primary_pixels = 0, 0
    for profile in range(3):
        for fmt in range(2):
            for scene in range(11 if fmt == 0 else 8):
                width, height = (4, 6) if scene == 2 else (3, 2) if scene == 3 else (6, 4)
                original_fmt = 91 if fmt == 0 else 93
                output_fmt = (87 if fmt == 0 else 88) if scene == 1 else 28 if scene in (4, 7) else original_fmt
                stem = f'extended-blt-{profile}-{fmt}-{scene}'
                names = (stem+'.actual.bin', stem+'.metadata.bin')
                blt_names.update(names)
                actual = struct.unpack('<'+'I'*(width*height), read(blt/names[0], width*height*4, rows))
                meta = struct.unpack('<7I', read(blt/names[1], 28, rows))
                require(meta[:6] == (profile, fmt, scene, width, height, output_fmt)
                        and meta[6] >= width*4, stem+': shape')
                # Physical X is undefined after RT/filter writes. Raw moves
                # scene0/1 must retain all four original bytes; X SRV scene7
                # and conversion to RGBA scene4 must materialize alpha255.
                mask = 0xffffff if fmt and scene in (2, 3, 5, 6) else 0xffffffff
                for y in range(height):
                    for x in range(width):
                        require((actual[y*width+x] & mask) == (literal(scene, x, y, bool(fmt)) & mask),
                                (stem, x, y, hex(actual[y*width+x]), hex(literal(scene, x, y, bool(fmt)))))
                        pixels += 1
        handles = set()
        for scene in range(6):
            stem = f'extended-primary-{profile}-{scene}'
            names = (stem+'.actual.bin', stem+'.allocation.bin', stem+'.metadata.bin')
            primary_names.update(names)
            actual = struct.unpack('<32I', read(primary/names[0], 128, rows))
            allocation = struct.unpack('<4I4Q8I', read(primary/names[1], 80, rows))
            meta = struct.unpack('<7I', read(primary/names[2], 28, rows))
            flags, fmt = (1, 1) if scene < 2 else (2, 1 if scene == 2 else 2) if scene < 4 else (1, 3)
            require(meta[:4] == (profile, scene, 8, 4) and meta[4] and meta[5:] == (flags, fmt), stem+': metadata')
            if scene == 0:
                first = meta[4]
                handles.add(first)
            elif scene == 1:
                require(meta[4] == first, stem+': primary identity')
            elif scene == 5:
                require(meta[4] == rgba, stem+': RGBA primary identity')
            else:
                if scene == 4:
                    rgba = meta[4]
                require(meta[4] not in handles, stem+': distinct optional allocation')
                handles.add(meta[4])
            require(allocation == (0x504d5644, 0, 80, 0, 128, 4096, 0, 0,
                    flags, fmt, 8, 4, 32, 60000 if flags == 1 else 0, 1001 if flags == 1 else 0, 0), stem+': wire')
            wanted = 0x40ff0080 if scene >= 4 else 0x408000ff if scene == 1 else 0x40bc00ff
            mask = 0xffffff if scene == 3 else 0xffffffff
            for word in actual:
                require((word & mask) == (wanted & mask), stem+': primary literal')
                primary_pixels += 1
        name = f'extended-primary-{profile}.cast-formats.bin'
        primary_names.add(name)
        require(struct.unpack('<8I', read(primary/name, 32, rows)) == (28, 27, 30, 30, 28, 28, 0, 0), 'RGBA UINT/default original view formats')
    require({p.name for p in blt.iterdir()} == blt_names, 'Blt file closure')
    require({p.name for p in primary.iterdir()} == primary_names, 'primary file closure')
    require(pixels == 1260 and primary_pixels == 576, 'pixel count')
    return dict(passed=True, controlled_renderer=True, profiles=3,
                blt_images=57, blt_pixels=pixels, primary_images=18,
                primary_pixels=primary_pixels, originals=rows,
                hardware_admission=False, scanout_admission=False,
                ordinary_runtime_admission=False, default_driver_changed=False)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('blt', type=Path); p.add_argument('primary', type=Path)
    p.add_argument('--blt-stdout', required=True, type=Path)
    p.add_argument('--primary-stdout', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    result = verify(a.blt, a.primary, a.blt_stdout, a.primary_stdout)
    with a.output.open('x') as f:
        json.dump(result, f, indent=2); f.write('\n')
    print('DXGI extended originals PASS blt_images=57 primary_images=18 pixels=1836 files=171 hardware_admission=0')


if __name__ == '__main__':
    main()
