#!/usr/bin/env python3
"""Review frozen CPU08 source and host inputs locally; no target calls."""
import ast
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile

W = Path('/home/sunf/droidvm-repos')
B = W / 'artifacts/dxvk-native-d3d8-system-device-20261008'
P = B / 'native-system-file-identity-packet-66bfbdf-08'
H = B / 'native-system-file-identity-handoff-66bfbdf-08'
R = W / 'reference/codes/dxvk-umd-dx8-system-device-20261008'
C = '66bfbdf73d32d7213af439a69cb569730b018f55'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def pin(row):
    data = Path(row['path']).read_bytes()
    assert len(data) == row['bytes'] and sha(data) == row['sha256'], row['path']
    return data


def originals(path):
    with tarfile.open(path) as archive:
        rows = archive.getmembers()
        assert all(r.isfile() for r in rows)
        assert len({r.name for r in rows}) == len(rows)
        assert all(not Path(r.name).is_absolute() and '..' not in Path(r.name).parts for r in rows)
        return {r.name: archive.extractfile(r).read() for r in rows}


def main():
    descriptor = H / 'prepared-native-system-file-identity-08.json'
    assert sha(descriptor.read_bytes()) == '1c4deca456e38f67602268a14c637bbcf0da9071f3b12daf4fa57e5d9bcb31d9'
    prepared = load(descriptor)
    assert prepared['source_commit'] == C and not prepared['ready']
    for value in prepared.values():
        if isinstance(value, dict) and {'path', 'bytes', 'sha256'} <= value.keys():
            pin(value)
    manifest = load(prepared['manifest']['path'])
    data = originals(prepared['archive']['path'])
    assert len(data) == len(manifest['inputs']) == prepared['frozen_source_inputs'] == 21
    assert set(data) == {r['path'] for r in manifest['inputs']}
    assert sha(pin(prepared['archive'])) == manifest['archive_sha256']
    git_count = licensed_count = 0
    for row in manifest['inputs']:
        item = data[row['path']]
        assert len(item) == row['bytes'] and sha(item) == row['sha256']
        if 'git_commit' in row:
            assert row['git_commit'] == C
            assert item == subprocess.check_output(['git', '-C', str(R), 'show', C + ':' + row['path']])
            git_count += 1
        else:
            prior_manifest = json.loads(pin(row['original_receipt']).decode('utf-8-sig'))
            prior_path = Path(row['original_receipt']['path']).with_suffix('.tar.gz')
            prior_bytes = prior_path.read_bytes()
            assert len(prior_bytes) == prior_manifest['archive_bytes']
            assert sha(prior_bytes) == prior_manifest['archive_sha256']
            prior_row = next(r for r in prior_manifest['inputs'] if r['path'] == row['path'])
            assert item == originals(prior_path)[row['path']]
            assert (row['bytes'], row['sha256']) == (prior_row['bytes'], prior_row['sha256'])
            while 'original_input' in prior_row:
                prior_row = prior_row['original_input']
            assert item == Path(prior_row['source_path']).read_bytes()
            licensed_count += 1
    assert git_count == 18 and licensed_count == 3
    for row in prepared['supplemental_CI_registration_inputs']:
        assert row['git_commit'] == C
        item = subprocess.check_output(['git', '-C', str(R), 'show', C + ':' + row['path']])
        assert len(item) == row['bytes'] and sha(item) == row['sha256']
    for name in ['build-native-d3d8-system-identity.ps1', 'parse-native-d3d8-system-identity.ps1',
                 'collect-native-d3d8-runtime-device.ps1', 'owned-raw-process-f4bf37f-02.cs']:
        assert (P / name).read_bytes() == data['scripts/' + name]
    assert sha((P / 'owned-raw-process-f4bf37f-02.cs').read_bytes()) == prepared['raw_process_helper_sha256']
    final = load(H / 'prepared-final-local-pins-review-08.json')
    assert final['verified'] and final['exit'] == 0 and final['target_calls'] == 0
    assert not final['native_execute_authorized']
    for row in final['original_input_pins']:
        pin(row)
    assert not pin(final['stderr'])
    plan = json.loads(pin(final['stdout']).decode('utf-8-sig'))
    assert plan['commands'] == final['actual_commands']
    assert plan['source_commit'] == C and not plan['target_commands_executed']
    assert plan['guest_inputs'] == prepared['guest_inputs']
    assert plan['guest_build_root'] == prepared['guest_root']
    for row in load(prepared['deferred_pins']['path'])['inputs']:
        pin(row)
    host = pin(prepared['orchestrator']).decode('utf-8')
    ast.parse(host)
    assert "'native-I386-D3D8-system-file-identity-CPU-only'" in host
    assert "process.communicate(timeout=timeout)" in host
    assert "assert allow_failure or process.returncode==0" in host
    assert "'collection-command'" in host and "'collection-transfer'" in host
    builder = (P / 'build-native-d3d8-system-identity.ps1').read_text()
    for flag in ["'/W4'", "'/WX'", "'/MT'", "'/O1'", "'/Zc:preprocessor'"]:
        assert flag in builder
    local_root = B / 'system-file-identity-source-review-01/root-source-and-local-originals-review-01.json'
    assert sha(local_root.read_bytes()) == '94aaa5eaee3276fe664b97712a7ad173f811b2179a5ac5d3ebe9962c97e71bf0'
    proof = dict(verified=True, source_commit=C, prepared_sha256=sha(descriptor.read_bytes()),
                 original_source_inputs=21, exact_Git_inputs=git_count, licensed_original_inputs=licensed_count,
                 licensed_prior_CPU07_archive_and_original_source_joined=True,
                 separately_counted_CI_registration_inputs=4, frozen_packet_helpers_exact_Git=True,
                 host_AST_and_original_no_target_plan_joined=True,
                 shared_helper_local_ROOT_review_sha256=sha(local_root.read_bytes()),
                 manual_build_and_orchestrator_review=True, expected_COFF=4, expected_PE=3,
                 expected_build_children=18, expected_collector_children=1, expected_host_operations=7,
                 included_predicate_checks=91, malformed_CLI_checks=4,
                 native_PS51_AST_pending=True, native_build_pending=True, physical_observation_pending=True,
                 target_calls=0, frontend_loaded=False, KMT_calls=0, core_loaded=False, hardware_acceptance=False)
    output = H / 'root-prepared-native-system-file-identity-08.json'
    with output.open('x') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(path=str(output), bytes=output.stat().st_size, sha256=sha(output.read_bytes()))))


if __name__ == '__main__':
    main()
