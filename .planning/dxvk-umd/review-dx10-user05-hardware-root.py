#!/usr/bin/env python3
"""Read actual DX10 originals and accept closed ownership; never call the target."""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import tarfile
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
WORKSPACE = Path('/home/sunf/droidvm-repos')
PROJECT = WORKSPACE / 'dxvk-umd-ci'
BASE = WORKSPACE / 'reference/codes/dxvk-umd-dx10-kmt-probe-20261007/artifacts/dx10-kmt-user-gate-20261007/prepared-user05-dual-profile-d7e5c7d-02'
RUN = BASE / 'native-execution-02'
RUN_ID = 'd7e5c7d-user05-hardware-02'
ARCHIVE_SHA = 'ad75e5ed39b76d5102effb64c2d9d529de2e652da84220942e75b04d5fbe4c96'
MANIFEST_SHA = 'fbe66424aa6711971de243441a1f13f67dc2ec82b5d94a952c82607408219bad'
AUTH_SHA = '716e7f7972df36992bfa258a3fc510d92d3e54ae12766ae00d0e47af6fb746b4'
CORE_SHA = 'b0fcbc3afe74a22ced7c4b35231b8064d6acc6b83f5d3b50671b408f2129f347'
PROBE_COMMIT = '05ffdc2b53ea2426129e9660a370b752d40ff817'
CORE_COMMIT = 'd7e5c7d46b8ce889e993bfab66a3b78b076c49d1'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def check(row, path=None):
    path = Path(row['path']) if path is None else path
    observed = pin(path)
    assert all(observed[k] == row[k] for k in ('bytes', 'sha256')), path
    return path


def process(row, stdout, stderr):
    assert row['Pid'] > 0 and row['ProcessHandle'] > 0
    assert row['Exited'] and row['ExitCodeAvailable'] and row['ExitCode'] == 0
    assert row['PipesDrained'] and not row['TimedOut'] and not row['ChildStillRunning']
    assert not row['Failure'] and row['Seconds'] >= 0
    assert row['StdoutBytes'] == len(stdout) and row['StderrBytes'] == len(stderr)


def image_check(directory, profile):
    literals = {'01-clear.raw': bytes((0, 255, 0, 255)),
                '02-vs-ps.raw': bytes((255, 0, 0, 255))}
    if profile == '10_1':
        literals.update({'03-gather.raw': bytes((0, 0, 255, 255)),
                         '04-msaa-clear.raw': bytes((0, 255, 0, 255)),
                         '05-sample-index.raw': None})
    assert {p.name for p in directory.glob('*.raw')} == set(literals)
    results = []
    for name, color in literals.items():
        path = directory / name
        data = path.read_bytes()
        assert len(data) == 1024, path
        if color is None:
            assert all(data[i:i + 4] in (bytes((127, 255, 0, 255)), bytes((128, 255, 0, 255)))
                       for i in range(0, 1024, 4)), path
        else:
            assert data == color * 256, path
        results.append(dict(profile=profile, name=name, pixels=256, original=pin(path)))
    return results


def task_check(root, manifest):
    config, result, closure = (read(root / leaf) for leaf in
                              ('task-config.json', 'task-result.json', 'task-collection.json'))
    assert result['completed'] and result['probe_passed'] and not result['failure']
    assert closure['task_removed'] and closure['task_exit'] == 0 and not closure['timed_out']
    token = result['process_token']
    assert token['user'] == manifest['user']['account'] and token['sid'] == manifest['user']['sid']
    assert token['session_id'] == 1 and token['elevated'] is False
    assert token['elevation_type'] == 3 and token['integrity_rid'] == 8192
    namespace = {'t': 'http://schemas.microsoft.com/windows/2004/02/mit/task'}
    for leaf in ('task-definition-original.xml', 'task-definition-before-start.xml', 'task-definition-after.xml'):
        path = root / leaf
        assert pin(path)['sha256'] == config['task_definition_sha256']
        principals = ET.fromstring(path.read_text(encoding='utf-8-sig')).findall('t:Principals/t:Principal', namespace)
        assert len(principals) == 1
        principal = principals[0]
        assert principal.findtext('t:UserId', namespaces=namespace).casefold() in {
            manifest['user']['sid'].casefold(), manifest['user']['account'].casefold()}
        assert principal.findtext('t:LogonType', namespaces=namespace) == 'InteractiveToken'
        assert principal.findtext('t:RunLevel', namespaces=namespace) in (None, 'LeastPrivilege')
    return dict(token=token, removed=True, task_result=pin(root / 'task-result.json'))


def kmt_counts(text, profile):
    lines = text.splitlines()
    interface, version = ('000a0001', '00040000') if profile == '10_0' else ('000a0002', '00010000')
    assert lines.count(f'D3D10_KMT_DEVICE interface={interface} version={version} flags=0 profile={profile}') == 1
    assert lines.count(f'D3D10_KMT_SELECTED profile={profile} luid=ec6b000000000000') == 1
    rows = [line for line in lines if line.startswith('DX11_KMT_COUNTS ')]
    assert len(rows) == 1
    row = re.fullmatch(r'DX11_KMT_COUNTS queries=(\d+) contexts=(\d+)/(\d+) allocations=(\d+)/(\d+) locks=(\d+)/(\d+) renders=(\d+) escapes=(\d+) residency=(\d+)/(\d+) wrong_threads=0 bad_cookies=0 core_errors=0 malformed_outputs=0 remaining_allocations=0 remaining_residents=0 pending_paging=0', rows[0])
    assert row
    queries, contexts, closes, allocs, frees, locks, unlocks, renders, escapes, residents, evictions = map(int, row.groups())
    assert queries > 0 and contexts == closes == 1
    assert allocs == frees > 0 and locks == unlocks > 0 and residents == evictions > 0
    assert renders > 0 and escapes >= 2
    counted = Counter(line.split()[0] for line in lines if line.startswith('DX11_KMT_'))
    expected = dict(CREATE_CONTEXT=contexts, DESTROY_CONTEXT=closes, ALLOCATE=allocs,
                    DEALLOCATE=frees, LOCK=locks, UNLOCK=unlocks, RENDER=renders,
                    ESCAPE=escapes, MAKE_RESIDENT=residents, EVICT=evictions)
    assert all(counted['DX11_KMT_' + key] == value for key, value in expected.items())
    draws, pixels = (1, 512) if profile == '10_0' else (3, 1280)
    assert lines[-1] == f'D3D10_KMT_PASS profile={profile} draws={draws} pixels={pixels} hr=00000000 ordinary_runtime_admission=0'
    return dict(queries=queries, **expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', type=Path, required=True)
    parser.add_argument('--release-sha256', required=True)
    args = parser.parse_args()
    assert pin(args.release)['sha256'] == args.release_sha256
    release = read(args.release)
    assert release['released'] and release['target_idle'] and release['next_owner'] == '/root'
    assert release['owner'] == '/root/port_dx10' and release['core_source_commit'] == CORE_COMMIT
    assert release['probe_source_commit'] == PROBE_COMMIT and release['ci_run'] == 37648387721
    assert release['input_manifest_sha256'] == MANIFEST_SHA and release['authorization_sha256'] == AUTH_SHA
    assert release['native_attempt_passed'] and release['isolated_real_kmt_hardware_pixels_passed']
    assert release['profiles_started'] == release['graphics_tasks_created'] == release['graphics_probe_invocations'] == 2
    assert release['draws'] == 4 and release['pixels_independently_verified'] == 1792
    assert not release['ordinary_runtime_admission'] and not release['production_admission']
    assert not release['native_retry_or_original_mutation'] and not release['blanket_registry_equality_asserted']
    for key in ('pending_native_children', 'pending_owned_tasks', 'pending_host_transports'):
        assert release[key] == 0
    proof_path = RUN / 'native-dual-profile-originals-finalized-03.json'
    assert pin(proof_path)['sha256'] == '7c0a408399a02d76713f88f0ffb6b70fcc4bafca9a79c23735ebbf32c74d2357'
    assert check(release['original_proof']) == proof_path
    for key in ('actual_local_reviewer', 'actual_pixel_reviewer'):
        local_reviewer = read(check(release[key]))
        assert local_reviewer['actual_owned_pid'] > 0 and local_reviewer['exit_code'] == 0
        assert not local_reviewer['timed_out'] and local_reviewer['process_exited'] and local_reviewer['raw_outputs_drained']
        check(local_reviewer['stdout']); assert check(local_reviewer['stderr']).stat().st_size == 0
    check(release['raw_management_registry_delta'])
    check(release['offline_reader_dependency_copy_proof'])
    for row in release['original_external_collector_sidecars']:
        check(row)
    proof = read(proof_path)
    assert proof['verified'] and proof['passed'] and proof['originals_finalized']
    assert proof['native_attempt_passed'] and proof['isolated_real_kmt_hardware_pixels_passed']
    assert proof['current_core_source_commit'] == CORE_COMMIT and proof['probe_source_commit'] == PROBE_COMMIT
    assert proof['ci_run'] == 37648387721 and proof['adapter_luid'] == 'ec6b000000000000'
    assert not proof['ordinary_runtime_admission'] and not proof['production_admission']
    archive = check(proof['archive'])
    assert check(release['original_archive']) == archive
    assert pin(archive)['sha256'] == ARCHIVE_SHA and archive.stat().st_size == 184189818
    files, directories = 0, 0
    seen = set()
    with tarfile.open(archive) as stream:
        for member in stream:
            name = member.name.removeprefix('./')
            assert name and not Path(name).is_absolute() and '..' not in Path(name).parts
            assert ':' not in name and '\\' not in name and name not in seen
            seen.add(name)
            assert member.isdir() or member.isfile()
            if member.isdir():
                directories += 1
                continue
            data = stream.extractfile(member).read()
            assert data == (RUN / 'originals' / name).read_bytes(), name
            files += 1
    assert files == proof['original_archive_files'] and directories == proof['original_archive_directories']
    assert files + directories == proof['original_archive_members']
    assert files == release['original_files'] and files + directories == release['original_tar_members']
    manifest_path = BASE / 'sealed-root-inputs-02/sealed-input-manifest.json'
    assert pin(manifest_path)['sha256'] == MANIFEST_SHA
    assert pin(BASE / 'root-runtime-authorization-02.json')['sha256'] == AUTH_SHA
    manifest = read(manifest_path)
    assert len(manifest['files']) == 72 and len(manifest['helpers']) == 9
    assert next(row for row in manifest['files'] if row['role'] == 'core')['sha256'] == CORE_SHA
    native = proof['actual_native_owned_children']
    assert len(native) == proof['actual_native_owned_children_count'] == 9
    assert len({row['actual_retained_process']['Pid'] for row in native}) == 9
    for child in native:
        receipt_path = check(child['receipt'])
        receipt = read(receipt_path)
        actual = child['actual_retained_process']
        if child['stage'].startswith('graphics-'):
            assert receipt == actual
            stdout = receipt_path.with_name('probe.stdout.raw').read_bytes()
            stderr = receipt_path.with_name('probe.stderr.raw').read_bytes()
        elif child['stage'] == 'archive-tar':
            assert receipt == actual
            stdout = Path(str(archive) + '.collector.stdout.raw').read_bytes()
            stderr = Path(str(archive) + '.collector.stderr.raw').read_bytes()
            assert not stderr
        else:
            assert receipt['process'] == actual and receipt['passed'] and not receipt['failure']
            assert receipt['manifest']['sha256'] == MANIFEST_SHA and receipt['authorization']['sha256'] == AUTH_SHA
            assert all(row['errors'] == 0 for row in receipt['parser_rows'])
            for row in receipt['raw_outputs']:
                check(row, RUN / 'owned-stage-originals' / row['path'].split('\\')[-1])
            stdout = (RUN / 'owned-stage-originals' / (child['stage'] + '.stdout.raw')).read_bytes()
            stderr = (RUN / 'owned-stage-originals' / (child['stage'] + '.stderr.raw')).read_bytes()
            assert not stderr
        process(actual, stdout, stderr)
    transports = proof['actual_host_transports']
    assert len(transports) == proof['actual_host_transports_count'] == 12
    for transport in transports:
        receipt = read(check(transport['receipt']))
        assert receipt['local_owned_transport_pid'] == transport['actual_pid'] > 0
        assert receipt['exit_code'] == transport['exit_code'] == 0
        assert not receipt['timed_out'] and not transport['timed_out']
        check(receipt['stdout']); check(receipt['stderr'])
    oracle_path = check(proof['original_pixel_shader_transport_oracle'])
    oracle = read(oracle_path)
    assert oracle['verified'] and oracle['passed'] and oracle['draws'] == 4 and oracle['pixels'] == 1792
    local = proof['original_pixel_reader_process']
    assert local['actual_owned_pid'] > 0 and local['exit_code'] == 0 and not local['timed_out']
    assert local['process_exited'] and local['raw_outputs_drained']
    check(local['stdout']); assert check(local['stderr']).stat().st_size == 0
    for row in proof['original_pixel_reader_closure']:
        check(row)
    reader_path = RUN / 'offline-reader-closure-03/verify-d3d10-user.py'
    assert pin(reader_path)['sha256'] == '3aa4380bfb6ddf9c293b8bb50d987b61c2caa4c05dc090a95eddaa05a0b1d3f2'
    assert reader_path.read_bytes() == (BASE.parent / 'prepared-user-runner-05/verify-d3d10-user.py').read_bytes()
    assert pin(reader_path.parent / 'originals/hardware-original.hlsl')['sha256'] == 'e738fec3109f13da0d9cf18a5f4e6871fe73cd3bb98c97805428a39e775852fc'
    retained_local_failures = []
    for name in ('independent-dual-profile-original-review-02', 'native-dual-profile-finalization-review-02'):
        path = RUN / (name + '.process-original.json')
        row = read(path)
        assert row['actual_owned_pid'] > 0 and row['exit_code'] == 1 and not row['timed_out']
        assert row['process_exited'] and row['raw_outputs_drained']
        check(row['stdout']); check(row['stderr'])
        retained_local_failures.append(pin(path))
    spec = importlib.util.spec_from_file_location('original_dx10_reader', reader_path)
    reader = importlib.util.module_from_spec(spec); spec.loader.exec_module(reader)
    before = read(RUN / 'management-readiness-before.json')
    after = read(RUN / 'management-readiness-after.json')
    baseline = read(BASE / 'sealed-root-inputs-02/stage/management-baseline-original.json')
    assert before['desktop'] == after['desktop'] == baseline['desktop']
    static = reader.static_reader.verify_registration(before, after)
    reader.static_reader.verify_registration(baseline, before)
    reader.static_reader.verify_registration(baseline, after)
    fields = ('device_id', 'status', 'binary', 'installed_package', 'driver_binding',
              'service', 'service_state', 'pnp_error', 'pnp_status', 'key')
    assert all(before['active_device'][k] == after['active_device'][k] == baseline['active_device'][k] for k in fields)
    profiles, images = [], []
    for profile in ('10_0', '10_1'):
        root = RUN / 'originals' / ('DxvkD3D10HardwareInteractive-' + RUN_ID + '-' + profile.replace('_', ''))
        result = reader.verify_attempt(root, manifest_path, MANIFEST_SHA, PROJECT, profile)
        image_rows = image_check(root / 'output/graphics', profile)
        images.extend(image_rows)
        token = task_check(root, manifest)
        counts = kmt_counts((root / 'output/probe.stdout.raw').read_text(encoding='utf-8-sig'), profile)
        profiles.append(dict(profile=profile, task=token, counts=counts,
                             frozen_original_reader=result, literal_pixel_files=len(image_rows)))
    assert len(images) == 7 and sum(row['pixels'] for row in images) == 1792
    result = dict(schema='root-DX10-user05-hardware-direct-review-v1', verified=True,
                  release_accepted=True, release=pin(args.release), original_proof=pin(proof_path),
                  archive=pin(archive), original_files=files, original_members=files + directories,
                  native_children=9, host_transports=12, profiles=profiles, images=images,
                  independently_verified_pixels=1792, draws=4,
                  retained_local_reader_dependency_failures=retained_local_failures,
                  management_desktop_equal=True, protected_active_fields=10,
                  explicit_static_registration=static, core_source_commit=CORE_COMMIT,
                  probe_source_commit=PROBE_COMMIT, ci_run=37648387721,
                  isolated_real_kmt_hardware_passed=True, ordinary_runtime_admission=False,
                  production_admission=False, target_calls=False)
    output = RUN / 'root-native-dual-profile-originals-direct-review-01.json'
    with output.open('x') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    print(json.dumps(dict(verified=True, release_accepted=True, files=files,
                          profiles=2, pixels=1792, ordinary_runtime_admission=False)))


if __name__ == '__main__':
    main()
