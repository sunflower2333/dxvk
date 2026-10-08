#!/usr/bin/env python3
"""Reopen source, retained compiler inputs and actual local residency evidence."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tarfile

ROOT = Path('/home/sunf/droidvm-repos')
REPO = ROOT / 'reference/codes/dxvk-umd-runtime-admission-20261008'
BASE = REPO / 'artifacts/residency-local-04'
SOURCE = 'cc8782738a18e2bfc262c61d9ef1f4c49da949db'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def checked(row):
    path = Path(row['path']); value = pin(path)
    assert value['bytes'] == row['bytes'] and value['sha256'] == row['sha256'], path
    return path.read_bytes()


def main():
    proof_path = BASE / 'residency-local-originals-verified-04.json'
    proof = read(proof_path)
    assert proof['verified'] and proof['source_commit'] == SOURCE
    archive_receipt = read(BASE / 'residency-original-archive-04.json')
    archive = Path(archive_receipt['archive']['path'])
    checked(archive_receipt['archive'])
    assert archive_receipt['proof'] == pin(proof_path)
    with tarfile.open(archive, 'r:gz') as stream:
        files = [member for member in stream if member.isfile()]
        assert len(files) == archive_receipt['members'] == len({member.name for member in files}) == 501
        for member in files:
            assert not Path(member.name).is_absolute() and '..' not in Path(member.name).parts and not member.issym() and not member.islnk()
            assert stream.extractfile(member).read() == (BASE / member.name).read_bytes()
    source = read(BASE / 'source.json')
    assert source['source_commit'] == SOURCE and len(source['files']) == 72
    for row in source['files']:
        data = (Path(source['frozen_source']) / row['path']).read_bytes()
        assert data == subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':' + row['path']])
        assert len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256']
        assert hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == row['git_blob']
    compiler = read(BASE / 'compiler-input-original-copies.json')
    assert len(compiler) == 338
    for row in compiler:
        assert checked(row['original']) == checked(row['copy'])
    assert len(proof['windows_coff']) == 10
    for row in proof['windows_coff']:
        data = checked(row)
        assert struct.unpack_from('<H', data)[0] == int(row['machine'], 16)
        assert row['machine'] in ['0x8664', '0x14c']
        command = read(BASE / Path(row['path']).stem / 'command.json')
        assert command['deadline_seconds'] == 60
        assert all(flag in command['argv'] for flag in ['-O2', '-Wall', '-Wextra', '-Werror', '-c'])
    assert len(proof['actual_processes']) == 14
    for row in proof['actual_processes']:
        assert row == read(BASE / row['name'] / 'process-result.json')
        assert row['exit_code'] == 0 and not row['timed_out'] and row['streams_closed']
        stdout = checked(row['stdout']); assert checked(row['stderr']) == b''
        if row['name'].endswith('sanitizer-run'):
            assert stdout == b'DXGI residency transaction PASS checks=102085\n'
        else:
            assert stdout == b''
        try:
            os.kill(row['pid'], 0)
        except ProcessLookupError:
            pass
        else:
            raise AssertionError('Original compiler/test process remains')
    assert proof['portable_checks_each'] == 102085 and proof['portable_compilers'] == ['GCC', 'Clang']
    for path in ['src/umd/umd_contract.cpp', 'src/umd/umd_contract.h']:
        assert subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':' + path]) == subprocess.check_output(['git', '-C', str(REPO), 'show', '90fb093:' + path])
    result = dict(verified=True, source_commit=SOURCE, original_archive=pin(archive), original_files=501,
        Git_sources=72, compiler_originals=338, strict_optimized_COFF=10, actual_closed_processes=14,
        GCC_Clang_ASan_UBSan_checks_each=102085, production_contract_bytes_unchanged=True,
        manual_production_and_fixture_reviewed=True, native_execution=False,
        render_cache_residency_complete=False, ordinary_runtime_admission=False,
        original_agent_proof=pin(proof_path), reviewer=pin(Path(__file__)))
    output = BASE / 'root-residency-local-originals-review-01.json'
    with output.open('x') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    print(json.dumps(pin(output)))


if __name__ == '__main__':
    main()
