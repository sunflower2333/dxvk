#!/usr/bin/env python3
"""Join exact source and original local prerequisites before the native CPU run."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tarfile

WORKSPACE = Path('/home/sunf/droidvm-repos')
BASE = WORKSPACE / 'artifacts/dxvk-native-d3d8-system-device-20261008'
HANDOFF = BASE / 'native-device-handoff-db0e183-06'
REPO = WORKSPACE / 'reference/codes/dxvk-umd-dx8-system-device-20261008'
SOURCE = 'db0e1833a0fc1c77435c118a56f0c0c43a665aa2'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def check(row):
    path = Path(row['path'])
    assert all(pin(path)[k] == row[k] for k in ('bytes', 'sha256')), path
    return path


def main():
    descriptor_path = HANDOFF / 'prepared-native-device-api-provider-06.json'
    assert pin(descriptor_path)['sha256'] == '0cc89588905e6f88f441148d822618943447896d1a87014e005d07e97a1fe0d1'
    descriptor = read(descriptor_path)
    assert descriptor['ready'] is False and descriptor['source_commit'] == SOURCE
    assert descriptor['expected_owned_native_build_children'] == 51 and descriptor['malformed_CLI_guards'] == 32
    for field in ('runtime_factories', 'valid_KMT_queries', 'core_loads', 'GPU_runs'):
        assert descriptor[field] == 0
    for field in ('archive', 'manifest', 'local_original_header_compile', 'actual_source_selector_controls',
                  'original_export_proof', 'root_original_export_review', 'previous_target_release', 'orchestrator', 'deferred_pins'):
        check(descriptor[field])
    release = read(Path(descriptor['previous_target_release']['path']))
    assert release['released'] and release['released_to'] == '/root'
    assert release['pending_owned_processes'] == release['pending_owned_transports'] == 0
    assert read(Path(descriptor['root_original_export_review']['path']))['release_accepted']
    pins = read(Path(descriptor['deferred_pins']['path']))
    assert pins['source_commit'] == SOURCE and pins['valid_runtime_modes_executed'] == 0 and len(pins['inputs']) == 13
    for row in pins['inputs']:
        check(row)
    manifest = read(Path(descriptor['manifest']['path']))
    assert manifest['source_commit'] == SOURCE and len(manifest['inputs']) == 33
    assert manifest['expected_original_coffs'] == 5 and manifest['expected_original_pes'] == 4
    assert manifest['malformed_cli_guards'] == 32 and manifest['native_policy_checks'] == 329
    assert manifest['core_binary_built'] is False and manifest['core_required_for_cpu_build'] is False
    members = {}
    with tarfile.open(Path(descriptor['archive']['path'])) as tar:
        for member in tar:
            assert member.isfile() and not Path(member.name).is_absolute() and '..' not in Path(member.name).parts
            assert member.name not in members
            members[member.name] = tar.extractfile(member).read()
    assert set(members) == {row['path'] for row in manifest['inputs']}
    git_inputs = licensed = production = 0
    for row in manifest['inputs']:
        data = members[row['path']]
        assert len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256']
        if 'git_commit' in row:
            original = subprocess.check_output(['git', 'show', row['git_commit'] + ':' + row['path']], cwd=REPO)
            assert data == original
            git_inputs += 1
            production += int(row['role'] == 'uncompiled-production-reference')
        else:
            assert row['role'] == 'original-licensed-legacy-header'
            original = row['original_input']['original_input']['source_path']
            assert data == Path(original).read_bytes()
            licensed += 1
    assert (git_inputs, licensed, production) == (30, 3, 12)
    local = read(Path(descriptor['local_original_header_compile']['path']))
    assert local['verified'] and local['all_diagnostics'] == 0 and local['objects'] == 4 and local['PEs'] == 2
    assert local['native_execution'] is False
    closure = read(BASE / 'process-api-provider-local-preflight-03/compiled-inputs-before-after-original.json')
    assert closure['unique_inputs'] == len(closure['inputs']) == 886 and len(closure['units']) == 4
    for row in closure['inputs']:
        check(row)
    for unit in closure['units'].values():
        assert unit['before'] == unit['after']
        obj = check(unit['object'])
        assert struct.unpack_from('<H', obj.read_bytes())[0] == 0x14c
        directory = Path(unit['original_input_directory'])
        for row in unit['before']:
            data = (directory / row['sha256']).read_bytes()
            assert len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256']
    commands = read(BASE / 'process-api-provider-local-preflight-03/commands-original.json')
    assert len(commands) == 12
    for row in commands:
        assert row['pid'] > 0 and row['exit'] == 0
        check(row['stdout']); check(row['stderr'])
        assert row['stderr']['bytes'] == 0
    for row in local['outputs']:
        path = check(row); data = path.read_bytes(); offset = struct.unpack_from('<I', data, 60)[0]
        assert data[offset:offset + 4] == b'PE\0\0' and struct.unpack_from('<H', data, offset + 4)[0] == 0x14c
    controls_path = Path(descriptor['actual_source_selector_controls']['path'])
    controls = read(controls_path)
    assert controls['verified'] and controls['cases_per_compiler'] == 16 and controls['target_calls'] == 0
    control_source = Path(controls['source']['path'])
    assert pin(control_source)['sha256'] == controls['source']['sha256']
    controls_commands = read(controls_path.parent / 'commands-original.json')
    assert len(controls_commands) == 4
    for row in controls_commands:
        assert row['pid'] > 0 and row['exit'] == 0
        for stream in ('stdout', 'stderr'):
            path = controls_path.parent / (row['name'] + '.' + stream + '.raw')
            assert pin(path)['sha256'] == row[stream + '_sha256']
    proof = dict(schema='root-DX8-provider-CPU06-prepared-direct-review-v1', verified=True,
                 prepared_descriptor=pin(descriptor_path), manifest=descriptor['manifest'], archive=descriptor['archive'],
                 source=pin(Path(__file__)), source_commit=SOURCE, exact_Git_inputs=30, exact_licensed_inputs=3,
                 unchanged_uncompiled_production_inputs=12, deferred_pins=13, local_original_inputs=886,
                 local_commands=12, local_I386_COFF=4, local_I386_PE=2, local_mock_cases_per_compiler=16,
                 previous_release=descriptor['previous_target_release'], root_original_export_review=descriptor['root_original_export_review'],
                 native_CPU_execution_pending=True, expected_native_build_children=51, expected_native_collector_children=1,
                 expected_native_SDK_headers=11, expected_native_libraries=9, native_compiler_rows=575,
                 target_calls=False, valid_KMT_queries=0, factories=0, core_loads=0, GPU_runs=0,
                 allowed_readonly_Win32_process_diagnostics=1, repair_replay=False)
    output = HANDOFF / 'root-native-provider-CPU06-prepared-review-01.json'
    with output.open('x') as stream:
        json.dump(proof, stream, indent=2); stream.write('\n')
    print(json.dumps(dict(verified=True, exact_source_inputs=33, local_original_inputs=886,
                         local_commands=12, native_CPU_execution_pending=True, proof=pin(output))))


if __name__ == '__main__':
    main()
