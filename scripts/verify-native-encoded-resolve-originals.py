#!/usr/bin/env python3
"""Literal raw oracle for the bounded FL10_0 encoded-resolve WARP fixture."""
from pathlib import Path
import argparse, json, struct

MARKER = 'Encoded resolve native PASS profiles=1 families=2 shaders=5 samples=4 images=12 pixels=3072 rejections=6 hardware_admission=0'

def pattern(pixel, sample):
    return ((pixel + sample * 53) & 255,
            (pixel * 17 + sample * 31) & 255,
            (255 - pixel + sample * 67) & 255,
            (pixel * 7 + sample * 43) & 255)

def nearest_even_quarter(total):
    whole, remainder = divmod(total, 4)
    return whole + (remainder > 2 or (remainder == 2 and whole & 1))

def verify(directory, stdout):
    directory = Path(directory)
    assert directory.is_dir() and not directory.is_symlink()
    text = Path(stdout).read_text(encoding='utf-8-sig')
    matches = [line for line in text.splitlines() if line.startswith('Encoded resolve native ')]
    assert matches == [MARKER], 'Unique exact fixture marker required'
    expected = {f'encoded-resolve-native-{family}-{case}.{role}.bin'
                for family in range(2) for case in range(6) for role in ('actual', 'metadata')}
    names = set()
    for path in directory.iterdir():
        if path.name.startswith('encoded-resolve-native-'):
            assert not path.is_symlink() and path.is_file(), 'Regular raw files required'
            names.add(path.name)
    assert names == expected, 'Raw observation closure differs'
    pixels = actual_bytes = 0
    for family in range(2):
        for case in range(6):
            prefix = directory / f'encoded-resolve-native-{family}-{case}'
            actual = prefix.with_name(prefix.name + '.actual.bin').read_bytes()
            metadata = prefix.with_name(prefix.name + '.metadata.bin').read_bytes()
            assert len(actual) == 1024 and len(metadata) == 32
            observed = struct.unpack('<8I', metadata)
            assert observed[:6] == (case, 16, 16, 4, 91 if family else 29,
                                    (87 if family else 28) if case < 4 else (91 if family else 29))
            assert observed[6] >= 64 and observed[7] == 0xa000
            for pixel in range(256):
                if case < 4:
                    channels = pattern(pixel, case)
                elif case == 4:
                    samples = [pattern(pixel, sample) for sample in range(4)]
                    channels = tuple(nearest_even_quarter(sum(sample[lane] for sample in samples))
                                     for lane in range(4))
                else:
                    channels = (255, 0, 255, 255)
                assert actual[pixel * 4:pixel * 4 + 4] == bytes(channels), (
                    f'family={family} case={case} pixel={pixel} actual={actual[pixel*4:pixel*4+4].hex()} expected={bytes(channels).hex()}')
                pixels += 1
            actual_bytes += len(actual)
    assert pixels == 3072 and actual_bytes == 12288
    return dict(passed=True, profiles=1, families=2, samples=4, images=12, pixels=3072,
        source_sample_images=8, source_sample_pixels=2048, mean_pixels=512,
        unchanged_target_pixels=512, raw_actual_bytes=12288, raw_metadata_bytes=384,
        raw_combined_bytes=12672, raw_files=24, hardware_admission=False,
        ordinary_runtime_admission=False, generic_inverse_srgb_bit_exactness=False)

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path); parser.add_argument('stdout',type=Path)
    parser.add_argument('--output',type=Path,required=True); args=parser.parse_args()
    result=verify(args.directory,args.stdout)
    with args.output.open('x') as stream: json.dump(result,stream,indent=2); stream.write('\n')
    print(json.dumps(result,sort_keys=True))
