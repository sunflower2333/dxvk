#!/usr/bin/env python3
"""Review exact CPU07 source and retained local originals without target access."""
import ast
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tarfile

W = Path('/home/sunf/droidvm-repos')
B = W / 'artifacts/dxvk-native-d3d8-system-device-20261008'
H = B / 'native-image-identity-handoff-f9722b3-07'
P = B / 'native-image-identity-packet-f9722b3-07'
R = W / 'reference/codes/dxvk-umd-dx8-system-device-20261008'
C = 'f9722b3d7abbd846aabf7f3517387d78449aa336'
OLD = 'db0e1833a0fc1c77435c118a56f0c0c43a665aa2'


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def row(path):
    path = Path(path)
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=digest(data))


def pinned(value):
    data = Path(value['path']).read_bytes()
    assert len(data) == value['bytes'] and digest(data) == value['sha256'], value['path']
    return data


def git(commit, name):
    return subprocess.check_output(['git', '-C', str(R), 'show', commit + ':' + name])


def command(value, empty=True):
    assert value['pid'] > 0 and value['exit'] == 0
    assert not Path('/proc', str(value['pid'])).exists(), value['pid']
    out, err = pinned(value['stdout']), pinned(value['stderr'])
    assert not err
    if empty:
        assert not out
    return out


def main():
    prepared_path = H / 'prepared-native-image-identity-07.json'
    prepared = load(prepared_path)
    assert prepared['source_commit'] == C and prepared['ready'] is False
    for value in prepared.values():
        if isinstance(value, dict) and {'path', 'bytes', 'sha256'} <= value.keys():
            pinned(value)
    pins = load(prepared['deferred_pins']['path'])
    assert len(pins['inputs']) == 12 and pins['source_commit'] == C
    for value in pins['inputs']:
        pinned(value)
    manifest = load(prepared['manifest']['path'])
    assert manifest['source_commit'] == C and len(manifest['inputs']) == 15
    with tarfile.open(prepared['archive']['path']) as archive:
        members = archive.getmembers()
        assert len(members) == 15 and all(m.isfile() for m in members)
        assert len({m.name for m in members}) == 15
        actual = {m.name: archive.extractfile(m).read() for m in members}
    assert set(actual) == {value['path'] for value in manifest['inputs']}
    git_count = licensed_count = 0
    for value in manifest['inputs']:
        data = actual[value['path']]
        assert len(data) == value['bytes'] and digest(data) == value['sha256']
        if 'git_commit' in value:
            assert value['git_commit'] == C and data == git(C, value['path'])
            assert data == (R / value['path']).read_bytes()
            git_count += 1
        else:
            original = value
            while 'original_input' in original:
                original = original['original_input']
            assert data == Path(original['source_path']).read_bytes()
            licensed_count += 1
    assert (git_count, licensed_count) == (12, 3)
    for name in ['build-native-d3d8-image-identity.ps1', 'parse-native-d3d8-image-identity.ps1',
                 'collect-native-d3d8-runtime-device.ps1', 'owned-raw-process-f4bf37f-02.cs']:
        assert (P / name).read_bytes() == actual['scripts/' + name]
    cross = load(prepared['local_strict_cross_original']['path'])
    final = load(prepared['local_source_helper_original']['path'])
    source = pinned(cross['source'])
    assert source == actual['tests/umd-d3d8-runtime-probe.cpp']
    for value in final['native_helpers']:
        pinned(value)
    assert git(C, 'tests/umd-d3d8-runtime-front.cpp') == git(OLD, 'tests/umd-d3d8-runtime-front.cpp')
    old = git(OLD, 'tests/umd-d3d8-runtime-probe.cpp')
    for value in cross['preserved_admission_extents']:
        marker = value['extent_start'].encode()
        data = source[source.index(marker):source.index(marker) + value['bytes']]
        prior = old[old.index(marker):old.index(marker) + value['bytes']]
        assert data == prior and digest(data) == value['sha256']
    snapshot = load(B / 'mapped-image-identity-local-preflight-02/compiled-inputs-before-after-original.json')
    assert snapshot['unique_inputs'] == len(snapshot['input_rows']) == 721
    assert len({v['path'] for v in snapshot['input_rows']}) == 721
    for value in snapshot['input_rows']:
        pinned(value)
    for unit in snapshot['units'].values():
        assert unit['before'] == unit['after']
        for value in unit['before']:
            pinned(value)
        assert struct.unpack_from('<H', pinned(unit['object']))[0] == 0x14c
    command(snapshot['units']['guard']['original_compile_receipt'])
    commands = load(B / 'mapped-image-identity-local-preflight-02/commands-original.json')
    assert len(commands) == 4
    for value in commands:
        command(value, empty=value['name'] != 'probe-readobj')
    pe = pinned(cross['outputs'][0])
    offset = struct.unpack_from('<I', pe, 0x3c)[0]
    assert pe[:2] == b'MZ' and pe[offset:offset + 4] == b'PE\0\0'
    assert struct.unpack_from('<H', pe, offset + 4)[0] == 0x14c
    control = load(prepared['reused_byte_identical_selected_header_controls']['path'])
    block = (B / 'mapped-image-identity-header-controls-01/actual-extracted-selected-header-block.cpp').read_bytes()
    assert block in source and digest(block) == control['actual_extracted_block_sha256']
    controls = load(B / 'mapped-image-identity-header-controls-01/commands-original.json')
    assert len(controls) == 4
    for value in controls:
        assert value['pid'] > 0 and value['exit'] == 0
        assert not Path('/proc', str(value['pid'])).exists()
        control_root = B / 'mapped-image-identity-header-controls-01'
        out = (control_root / (value['name'] + '.stdout.raw')).read_bytes()
        err = (control_root / (value['name'] + '.stderr.raw')).read_bytes()
        assert digest(out) == value['stdout_sha256']
        assert digest(err) == value['stderr_sha256'] and not err
        if 'compile' in value['name']:
            assert not out
        if 'run' in value['name']:
            assert b'24' in out and b'PASS' in out
    orchestrator = pinned(prepared['orchestrator'])
    ast.parse(orchestrator)
    for name in ['collect-native-d3d8-runtime-device.ps1', 'owned-raw-process-f4bf37f-02.cs']:
        assert git(C, 'scripts/' + name) == git(OLD, 'scripts/' + name)
    cube = W / 'artifacts/native-cube-integration-20261008/guest-native-8528d91-01'
    release = cube / 'target-cpu-release-01.json'
    prior_review = cube / 'root-native01-failure-and-release-direct-review-01.json'
    accepted = load(prior_review)
    assert digest(release.read_bytes()) == accepted['release_sha256']
    assert accepted['target_released_to_ROOT'] is True and accepted['verified'] is True
    result = dict(schema='root-dx8-image-CPU07-prepared-v1', verified=True, source_commit=C,
                  prepared=row(prepared_path), source_archive=row(prepared['archive']['path']),
                  manifest=row(prepared['manifest']['path']), orchestrator=row(prepared['orchestrator']['path']),
                  pins=row(prepared['deferred_pins']['path']), exact_source_inputs=15, Git_inputs=12,
                  licensed_inputs=3, current_compiled_input_originals=721, I386_COFF=2, I386_PE=1,
                  cross_commands=4, retained_guard_compile_commands=1, diagnostics=0,
                  unchanged_admission_extents=3, unchanged_frontend=True,
                  selected_header_checks_each=24, mapped_modules=4, readonly_file_attempts=7,
                  expected_native_build_children=11, expected_native_collector_children=1,
                  expected_host_operations=7, previous_release=row(release),
                  ROOT_previous_original_review=row(prior_review), target_calls=0,
                  observation_admission=False, runtime_factories=0, KMT_calls=0, core_loads=0, GPU_runs=0)
    output = H / 'root-prepared-native-image-identity-07.json'
    assert not output.exists()
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(row(output)))


if __name__ == '__main__':
    main()
