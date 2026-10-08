#!/usr/bin/env python3
"""Verify literal API-defined DSV fields and complete UAV words; no identity claim."""
import argparse
import json
from pathlib import Path
import re
import struct

MARKER = re.compile(
    r'D3D11 predicate view clear native PASS checks=([1-9][0-9]*) cases=83 fields=19256 bytes=73040 '
    r'immediate_matrix=8 null=2 aspects=7 deferred_replays=12 default_null=1 explicit_null=1 '
    r'historical_ends=2 nested_restore=2 parent_restore=2 raw_files=83 '
    r'ordinary_runtime_admission=0 predication_complete=0\n')

def cases():
    rows = []
    for visible in (False, True):
        for value in (False, True):
            for hint in (False, True):
                rows.append((hint or visible != value, 3, True))
    rows.extend(((True, 3, True), (True, 3, True)))
    rows.extend((True, aspect, False) for aspect in (0, 1, 2, 3, 2, 1, 0))
    for nested in (False, True):
        for restore in (False, True):
            for _ in range(3):
                rows.extend(((True, 1, True), (False, 3, True), (True, 2, True),
                             (True, 3, True), (not restore, 3, True)))
                if nested:
                    rows.append((not restore, 3, True))
    assert len(rows) == 83
    return rows

def expected(execute, aspects, uav_execute):
    # The D32_FLOAT_S8X24 format's 24 X bits are not API-defined fields.
    # Raw records retain all 32 depth bits and every stencil bit (five bytes).
    depth = 0x3e800000 if execute and aspects & 1 else 0x3f400000
    stencil = 0xa7 if execute and aspects & 2 else 0x3c
    result = bytearray(struct.pack('<IB', depth, stencil) * 16)
    integer = (0xdeadbeef, 0x01234567, 0x89abcdef, 0xa55a5aa5)
    floating = (0x3e800000, 0xc0000000, 0x40400000, 0x3f800000)
    for kind, count in enumerate((8, 32, 64, 32, 64)):
        for i in range(count):
            value = integer[0] if kind == 0 else integer[i % 4] if kind < 3 else floating[i % 4]
            if not execute or not uav_execute:
                value = (0x7e91c523 ^ (i * 0x1357acdf)) & 0xffffffff
            result.extend(struct.pack('<I', value))
    assert len(result) == 880
    return bytes(result)

def verify(directory, stdout):
    raw_stdout = stdout.read_bytes()
    marker = MARKER.fullmatch(raw_stdout.decode('ascii'))
    if not marker or int(marker.group(1)) > 0xffffffff:
        raise ValueError('exact native marker required')
    names = {f'predicate-view-clear-{i:03d}.bytes' for i in range(83)}
    if {path.name for path in directory.glob('predicate-view-clear-*')} != names:
        raise ValueError('exact raw file set required')
    for i, state in enumerate(cases()):
        path = directory / f'predicate-view-clear-{i:03d}.bytes'
        if path.is_symlink() or not path.is_file():
            raise ValueError(f'case {i}: regular original required')
        raw = path.read_bytes()
        if raw != expected(*state):
            raise ValueError(f'case {i}: complete defined DSV/UAV bytes differ')
    return dict(raw_files=83, defined_fields=19256, raw_bytes=73040,
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
