#!/usr/bin/env python3
"""Verify complete CS UAV/counter words, without inferring native process identity."""
import argparse
import json
from pathlib import Path
import re
import struct

MARKER = re.compile(
    r'D3D11 predicate dispatch native PASS checks=([1-9][0-9]*) cases=158 words=8216 '
    r'direct=79 indirect=79 immediate_matrix=16 null=4 zero_dimensions=6 deferred_replays=24 '
    r'default_null=1 explicit_null=1 historical_ends=2 nested_restore=2 parent_restore=2 '
    r'cs_state=1 counters=1 raw_files=158 ordinary_runtime_admission=0 predication_complete=0\n')

def cases():
    rows = []
    for _indirect in (False, True):
        for visible in (False, True):
            for value in (False, True):
                for hint in (False, True):
                    rows.append((hint or visible != value, 0xd1700000))
        rows.extend(((True, 0xd1700000),) * 2)
        rows.extend(((False, 0xd1700000),) * 3)
        for nested in (False, True):
            for restore in (False, True):
                for _ in range(3):
                    rows.extend(((True, 0xd1700000), (False, 0xd1700000),
                        (True, 0xd1700000), (True, 0xd1700000), (not restore, 0xd2700000)))
                    if nested:
                        rows.append((not restore, 0xd2700000))
    assert len(rows) == 158
    return rows

def expected(execute, seed):
    initial = [((0x7e91c523 ^ (i * 0x1357acdf)) & 0xffffffff) for i in range(24)]
    output = [seed | i if execute and i < 12 else initial[i] for i in range(24)]
    append = [0xa11ec0de if execute and 5 <= i < 17 else initial[i] for i in range(24)]
    count = [0x13b58ad0, 17 if execute else 5, 0x917cd53b, 0xfb7542aa]
    return struct.pack('<52I', *(output + append + count))

def verify(directory, stdout):
    marker = MARKER.fullmatch(stdout.read_bytes().decode('ascii'))
    if not marker or int(marker.group(1)) > 0xffffffff:
        raise ValueError('exact native marker required')
    names = {f'predicate-dispatch-{i:03d}.words' for i in range(158)}
    if {path.name for path in directory.glob('predicate-dispatch-*')} != names:
        raise ValueError('exact raw file set required')
    for i, state in enumerate(cases()):
        path = directory / f'predicate-dispatch-{i:03d}.words'
        if path.is_symlink() or not path.is_file():
            raise ValueError(f'case {i}: regular original required')
        if path.read_bytes() != expected(*state):
            raise ValueError(f'case {i}: complete CS UAV/counter words differ')
    return dict(raw_files=158, raw_words=8216, raw_bytes=32864,
        native_execution_identity_verified=False, ordinary_runtime_admission=False,
        predication_complete=False)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', required=True, type=Path)
    parser.add_argument('--stdout', required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.directory, args.stdout), sort_keys=True))

if __name__ == '__main__':
    main()
