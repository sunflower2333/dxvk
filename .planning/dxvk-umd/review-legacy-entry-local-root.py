#!/usr/bin/env python3
"""Reopen the actual local public legacy entry source and COFF evidence."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path('/home/sunf/droidvm-repos')
REPO = ROOT / 'reference/codes/dxvk-umd-legacy-system-entry-20261008'
SOURCE = '318d3309a554eefe1d9461887b078f381670c310'
BASE = ROOT / 'artifacts/legacy-system-entry-20261008'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def checked(row):
    path = Path(row['path'])
    actual = pin(path)
    assert actual['bytes'] == row['bytes'] and actual['sha256'] == row['sha256'], path
    return path.read_bytes()


def main():
    manifest_path = BASE / 'local-source-compiler-input-join-01/source-compiler-inputs.json'
    assert pin(manifest_path)['sha256'] == '82ff8d40f2d0f6ea303d7fb23961d1ca319918fa11848d5d9ec7582db4c5bc4a'
    manifest = read(manifest_path)
    assert manifest['source_commit'] == SOURCE
    assert len(manifest['source_git_current']) == 8
    for row in manifest['source_git_current']:
        data = checked(row)
        original = subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':' + row['git_path']])
        assert original == data
        assert hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == row['git_blob']
    assert len(manifest['recorded_compile_source_before_after']) == 2
    for row in manifest['recorded_compile_source_before_after']:
        assert checked(row['before']) == checked(row['after'])
        inputs = read(Path(row['before']['path']))
        assert len(inputs) == row['recorded_source_inputs'] == 6
        for source in inputs:
            data = checked(source)
            relative = str(Path(source['path']).relative_to(REPO))
            assert data == subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':' + relative])
    successful = manifest['successful_original_compilations']
    failures = manifest['preserved_initial_arm_intrinsic_failures']
    assert len(successful) == 15 and len(failures) == 5
    targets = {'i686-pc-windows-msvc': 0x14c, 'x86_64-pc-windows-msvc': 0x8664, 'aarch64-pc-windows-msvc': 0xaa64}
    machines = {}
    for row in successful + failures:
        receipt = json.loads(checked(row['receipt']))
        assert receipt['argv'] == row['argv'] and receipt['exit_code'] == row['actual_exit_code']
        argv = row['argv']
        assert all(flag in argv for flag in ['-O1', '-Wall', '-Wextra', '-Werror', '-c', '-MD'])
        assert checked(row['stdout']) == b''
        diagnostics = checked(row['stderr'])
        if row in failures:
            assert receipt['exit_code'] == 1 and b'__prefetch' in diagnostics
            continue
        assert receipt['exit_code'] == 0 and diagnostics == b''
        image = checked(row['object'])
        assert receipt['object'] == row['object']
        target = next(item.split('=', 1)[1] for item in argv if item.startswith('--target='))
        assert struct.unpack_from('<H', image)[0] == targets[target]
        machines[target] = machines.get(target, 0) + 1
        assert checked(row['dependency_file'])
    assert machines == {target: 5 for target in targets}
    llvm = read(Path(manifest['original_llvm_review']['path']))
    checked(manifest['original_llvm_review'])
    assert llvm['passed'] and len(llvm['objects']) == 6
    for row, original in zip(manifest['original_llvm_reopens'], llvm['objects']):
        image = checked(row['object'])
        assert original['argv'] == row['argv'] and original['exit_code'] == row['actual_exit_code'] == 0
        assert original['object_bytes'] == len(image) and original['object_sha256'] == row['object']['sha256']
        output = checked(row['stdout'])
        assert checked(row['stderr']) == b'' and original['public_entry'].encode() in output
        assert b'Relocations [' in output and b'Symbols [' in output
    dependencies = manifest['current_used_compiler_inputs']
    assert len(dependencies) == 303 and len({row['path'] for row in dependencies}) == 303
    for row in dependencies + manifest['current_auxiliary_compiler_inputs'] + manifest['capability_document_inputs_not_claimed_as_compiled']:
        checked(row)
    for tool in manifest['current_tools']:
        checked(tool['current_executable']); checked(tool['stdout']); assert checked(tool['stderr']) == b''
        assert tool['actual_exit_code'] == 0
    assert manifest['headers_pinned_before_original_compilation'] is False
    assert not manifest['native_msvc_execution'] and not manifest['native_fixture_execution']
    result = dict(verified=True, source_commit=SOURCE, source_files=8, original_compile_source_controls=2,
        successful_strict_local_COFF=15, preserved_initial_failures=5, original_LLVM_reopens=6,
        current_used_dependencies=303, historical_whole_toolchain_attestation=False,
        native_execution=False, hardware_admission=False, default_binding_changed=False,
        original_manifest=pin(manifest_path), reviewer=pin(Path(__file__)))
    output = BASE / 'local-source-compiler-input-join-01/root-local-originals-review-01.json'
    with output.open('x') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    print(json.dumps(pin(output)))


if __name__ == '__main__':
    main()
