#!/usr/bin/env python3
"""Join direct public cube GenerateMips observations with independent arithmetic."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def verify(directory, stdout):
    records = []
    views = []
    originals = set()
    counts = []
    words = 0
    for case, (requested, selected) in enumerate(((0xffffffff, 4), (2, 2), (1, 1))):
        view = directory / f'public-cube-direct-{case}.view'
        expected_view = (case, requested, 16, 16, 5, 6, 28, 1, 0, 0, 0x28, 5, 9, 1, selected)
        raw_view = view.read_bytes()
        assert len(raw_view) == 60 and struct.unpack('<15I', raw_view) == expected_view
        originals.add(view.name)
        views.append(pin(view))
        case_mismatches = 0
        for face in range(6):
            for mip in range(5):
                edge = max(1, 16 >> mip)
                stem = f'public-cube-direct-{case}-face-{face}-mip-{mip}'
                metadata = directory / (stem + '.metadata')
                raw = metadata.read_bytes()
                assert len(raw) == 48
                observed_metadata = struct.unpack('<12I', raw)
                assert observed_metadata[:6] == (case, face, mip, face * 5 + mip, edge, edge)
                assert observed_metadata[6] >= edge * 4
                assert observed_metadata[8:] == (1, selected, 5, 6)
                actual_path = directory / (stem + '.words')
                expected_path = directory / (stem + '.expected')
                actual_bytes, expected_bytes = actual_path.read_bytes(), expected_path.read_bytes()
                assert len(actual_bytes) == len(expected_bytes) == edge * edge * 4
                actual = struct.unpack('<' + str(edge * edge) + 'I', actual_bytes)
                wanted = [0xff000000 | (face << 20) | (mip << 16) | (index + 1)
                          for index in range(edge * edge)]
                if 1 <= mip < 1 + selected:
                    wanted = [0xff000000 | face * 0x20202] * len(wanted)
                assert list(struct.unpack('<' + str(edge * edge) + 'I', expected_bytes)) == wanted
                mismatches = [dict(index=index, x=index % edge, y=index // edge,
                    actual=f'{value:08x}', expected=f'{required:08x}')
                    for index, (value, required) in enumerate(zip(actual, wanted)) if value != required]
                case_mismatches += len(mismatches)
                words += len(actual)
                records.append(dict(case=case, face=face, mip=mip, words=len(actual),
                    scope='source' if mip == 1 else 'generated' if 2 <= mip < 1 + selected else 'excluded',
                    mismatches=mismatches, metadata=pin(metadata), actual=pin(actual_path), expected=pin(expected_path)))
                originals.update((metadata.name, actual_path.name, expected_path.name))
        counts.append((case, selected, case_mismatches))
    assert len(records) == 90 and words == 6138 and len(originals) == 273
    assert {p.name for p in directory.glob('public-cube-direct-*')} == originals
    text = stdout.read_text(encoding='utf-8-sig').replace('\r\n', '\n')
    observed = [tuple(map(int, groups)) for groups in re.findall(
        r'PUBLIC TextureCube directGenerateMips\ncase=(\d+)\nfirst_mip=1\nmip_count=(\d+)\nmismatched_words=(\d+)\n', text)]
    assert observed == counts
    return dict(schema='native-cube-direct-public-mips-original-observations-v1',
        verified=True, observations_verified=True, cases=3, subresources=90,
        original_files=273, original_words=6138, recorded_mismatch_counts=counts,
        views=views, records=records, original_stdout=pin(stdout),
        oracle='Literal face/mip/index words and uniform selected-mip values; no production helper calls.',
        public_pixel_equality_required=False, production_oracle_unchanged=True,
        hardware_acceptance=False, ordinary_runtime_admission=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    proof = verify(args.directory, args.stdout)
    with args.output.open('x') as out:
        json.dump(proof, out, indent=2)
        out.write('\n')
    print(json.dumps(dict(observations_verified=True, subresources=90, original_files=273,
        original_words=6138, recorded_mismatch_counts=proof['recorded_mismatch_counts'], hardware_acceptance=False)))


if __name__ == '__main__':
    main()
