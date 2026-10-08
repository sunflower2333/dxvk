#!/usr/bin/env python3
"""Strict public-core clear probe stdout and independent raw RGBA oracle.

This reads pixels only. It cannot establish execution/backend/adapter identity;
the native caller must separately close and pin its actual process/artifacts.
"""
import argparse
import json
from pathlib import Path
import re
import struct


def expected():
    rows=[]
    for visible in (False,True):
        for value in (False,True):
            for hint in (False,True):
                rows.append((256,hint or visible!=value,0xff0000ff))
    rows.extend([(256,True,0xff0000ff)]*2)
    for visible in (False,True):
        for value in (False,True):rows.append((16,visible!=value,0xff0000ff))
    for nested in (False,True):
        for restore in (False,True):
            for _ in range(3):
                rows.extend([(16,True,0xffff0000),(256,False,0xff0000ff),
                    (16,True,0xff00ff00),(256,True,0xff0000ff),(256,not restore,0xffff00ff)])
                if nested:rows.append((16,not restore,0xffffffff))
    assert len(rows)==80 and sum(row[0] for row in rows)==12320
    return rows


def validate(directory,stdout):
    marker=(r'D3D11 predicate clear native PASS checks=([1-9][0-9]*) cases=80 words=12320 '
      r'immediate_matrix=8 null=2 buffer_matrix=4 deferred_replays=12 default_null=1 explicit_null=1 '
      r'historical_ends=2 nested_restore=2 parent_restore=2 raw_files=80 '
      r'ordinary_runtime_admission=0 predication_complete=0\n')
    match=re.fullmatch(marker,stdout)
    if not match or int(match.group(1))>0xffffffff:raise ValueError('native stdout marker mismatch')
    names={f'predicate-clear-{case:03d}.rgba' for case in range(80)}
    if {p.name for p in directory.glob('predicate-clear-*.rgba')}!=names:raise ValueError('raw file set mismatch')
    observed=0
    for case,(count,cleared,color) in enumerate(expected()):
        path=directory/f'predicate-clear-{case:03d}.rgba'
        if not path.is_file() or path.is_symlink():raise ValueError('raw file is not a regular original')
        raw=path.read_bytes()
        if len(raw)!=count*4:raise ValueError(f'raw size mismatch case={case}')
        values=struct.unpack('<'+'I'*count,raw)
        for pixel,value in enumerate(values):
            wanted=color if cleared else (0x28a9f573^(pixel*0x1357acdf))&0xffffffff
            if value!=wanted:raise ValueError(f'RGBA word mismatch case={case} pixel={pixel} actual={value:08x} expected={wanted:08x}')
            observed+=1
    return dict(passed=True,raw_files=80,observed_words=observed,observed_bytes=observed*4,
        native_execution_identity_verified=False,ordinary_runtime_admission=False,predication_complete=False)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--directory',type=Path,required=True)
    parser.add_argument('--stdout',type=Path,required=True);args=parser.parse_args()
    print(json.dumps(validate(args.directory,args.stdout.read_text())))


if __name__=='__main__':main()
