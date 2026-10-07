#!/usr/bin/env python3
"""Independently retain the failed DX10 attempt and verify target release."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tarfile

sys.dont_write_bytecode = True
WORKSPACE = Path('/home/sunf/droidvm-repos')
BASE = WORKSPACE / 'reference/codes/dxvk-umd-dx10-kmt-probe-20261007/artifacts/dx10-kmt-user-gate-20261007/prepared-user05-dual-profile-d7e5c7d-01'
RUN = BASE / 'native-execution-01'

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

def main():
    release_path = RUN / 'target-cpu-release-01.json'
    assert pin(release_path)['sha256'] == 'bc6a7b6549b7a4ff37dffe6e59634b3147dd261bdf2cc488043524e96860a283'
    release = read(release_path)
    assert release['released'] and release['target_idle'] and release['next_owner'] == '/root'
    for key in ('pending_native_children', 'pending_owned_tasks', 'pending_host_transports',
                'profiles_started', 'graphics_tasks_created', 'graphics_probe_invocations', 'pixels_rendered'):
        assert release[key] == 0
    assert not release['hardware_passed'] and not release['native_attempt_passed']
    proof = read(check(release['original_proof']))
    check(release['actual_local_reviewer'])
    archive = check(release['original_archive'])
    members = {}
    with tarfile.open(archive) as stream:
        for member in stream:
            name = member.name.removeprefix('./')
            assert member.isfile() and not Path(name).is_absolute() and '..' not in Path(name).parts
            assert name not in members
            data = stream.extractfile(member).read()
            assert data == (RUN / 'originals' / name).read_bytes(), name
            members[name] = hashlib.sha256(data).hexdigest()
    assert len(members) == release['original_files'] == 101
    rows = read(BASE / 'sealed-root-inputs-01/sealed-stage-original-files-81.json')['files']
    assert len(rows) == 81
    for row in rows:
        check(row, RUN / 'originals' / row['path'])
    children = release['native_owned_children']
    assert len(children) == 5
    for child in children:
        receipt = read(check(child['receipt']))
        process = receipt['process']
        assert process == child['actual_retained_process']
        assert process['Pid'] > 0 and process['ProcessHandle'] > 0
        assert process['Exited'] and process['ExitCodeAvailable'] and process['PipesDrained']
        assert process['ExitCode'] == 0 and not process['TimedOut'] and not process['ChildStillRunning']
        assert not process['Failure']
        for row in receipt['raw_outputs']:
            check(row, RUN / 'owned-stage-originals' / row['path'].split('\\')[-1])
        assert read(RUN / 'owned-stage-originals' / (child['stage'] + '.process-result.json')) == process
        assert receipt['passed'] == (child['stage'] not in ('before', 'after'))
        assert all(row['errors'] == 0 for row in receipt['parser_rows'])
    transports = release['host_owned_transports']
    assert len(transports) == 12
    for row in transports:
        path = check(row['receipt'])
        original = read(path)
        assert original['local_owned_transport_pid'] == row['actual_pid'] > 0
        expected = 1 if path.name in ('native-before.original-command.json', 'native-after.original-command.json') else 0
        assert original['exit_code'] == row['exit_code'] == expected
        assert not original['timed_out'] and not row['timed_out']
        check(original['stdout']); check(original['stderr'])
    for row in release['collector_sidecars']:
        check(row)
    check(release['transfer_original'])
    preflight = read(RUN / 'originals/native-input-preflight-original.json')
    assert preflight['passed'] and preflight['file_roles'] == 72 and preflight['helpers'] == 9 and preflight['ci_source_rows'] == 12
    assert not preflight['core_loaded'] and not preflight['gpu_execution'] and not preflight['USER_task_registered']
    before = read(check(proof['management_before']))
    after = read(check(proof['management_after']))
    fresh = read(check(proof['frozen_fresh_readiness']))
    assert {k:v for k,v in before.items() if k != 'timestamp'} == {k:v for k,v in after.items() if k != 'timestamp'}
    assert before['desktop'][0] == dict(Name='dwm', Id=1864, StartTime='/Date(1791378029900)/')
    assert fresh['desktop'][0] == dict(Name='dwm', Id=1864, StartTime=None)
    assert before['desktop'][1:] == fresh['desktop'][1:]
    fields = ('device_id','status','binary','installed_package','driver_binding','service','service_state','pnp_error','pnp_status','key')
    assert all(before['active_device'][k] == fresh['active_device'][k] for k in fields)
    reader = BASE / 'original-readers/originals/verify-registration-originals-09.py'
    assert pin(reader)['sha256'] == '28f29b0e69e06adb78fc2d6da10d3cd3d1709cfbb7da4619c47b0f90cbbbe051'
    spec = importlib.util.spec_from_file_location('original_registration', reader)
    registration = importlib.util.module_from_spec(spec); spec.loader.exec_module(registration)
    static = registration.verify_registration(before, fresh)
    assert static['verified'] and len(static['protected_static_value_names']) == 34
    assert not (RUN / 'native-dual.original-command.json').exists()
    assert not list((RUN / 'originals').rglob('probe-process-original.json'))
    result = dict(schema='root-DX10-user05-failure-release-direct-review-v1', verified=True,
                  failure_retained=True, release_accepted=True, release=pin(release_path),
                  original_archive_members=101, sealed_stage_files=81, native_children=5, host_transports=12,
                  protected_active_fields=10, protected_registration_values=34,
                  management_same_attempt_only_difference='timestamp',
                  cross_privilege_difference='DWM StartTime null under limited USER, known under management',
                  management_baseline_original=pin(Path(proof['management_before']['path'])),
                  profiles_started=0, graphics_probe_invocations=0, rendered_pixels=0,
                  hardware_passed=False, ordinary_runtime_admission=False, target_calls=False)
    path = RUN / 'root-failed-native-originals-direct-review-01.json'
    with path.open('x') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
