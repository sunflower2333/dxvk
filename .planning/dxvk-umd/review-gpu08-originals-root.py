#!/usr/bin/env python3
"""Read original GPU08 evidence independently; no target operations."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tarfile

sys.dont_write_bytecode = True
WS = Path('/home/sunf/droidvm-repos')
REPO = WS / 'dxvk-umd-ci'
BASE = WS / 'artifacts/dxvk-native-dx10-dx11-20261007'
ROOT = BASE / 'guest-compute-gpu-d7e5c7d-08'
TASK = ROOT / 'originals/DxvkD3D11ComputeInteractive-d7e5c7d-gpu-08'
OUT = TASK / 'output'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def main():
    reviewed = read(ROOT / 'native-d3d11-compute-originals-verified-08.json')
    assert reviewed['verified'] is True
    archive = ROOT / 'compute-gpu-d7e5c7d-gpu-08-evidence.tar.gz'
    assert pin(archive)['sha256'] == '93c1d6f87695481c9be3990dd1d537051636f6b05375dff6409e73e7d8aaee60'
    joins = read(ROOT / 'actual-native-archive-member-joins-08.json')
    with tarfile.open(archive, 'r:gz') as tar:
        members = [m for m in tar.getmembers() if m.isfile()]
        assert len(members) == len(joins['members']) == 126
        rows = {r['name']: r for r in joins['members']}
        assert len(rows) == 126
        for member in members:
            name = member.name
            assert not Path(name).is_absolute() and '..' not in Path(name).parts
            data = tar.extractfile(member).read()
            local = ROOT / 'originals' / name
            assert local.read_bytes() == data
            assert (len(data), hashlib.sha256(data).hexdigest()) == (rows[name]['bytes'], rows[name]['sha256'])
    manifest = read(ROOT / 'ready-compute-gpu-manifest-08.json')
    assert manifest['ready'] and len(manifest['files']) == 47
    role_files = {}
    for row in manifest['files']:
        path = OUT / row['destination']
        assert (path.stat().st_size, pin(path)['sha256']) == (row['bytes'], row['sha256'])
        role_files[row['role']] = path
    assert len(manifest['source_inputs']) == 19
    for row in manifest['source_inputs']:
        data = subprocess.check_output(['git', 'show', row['source_commit'] + ':' + row['git_path']], cwd=REPO)
        assert data == role_files[row['file_role']].read_bytes()
        assert hashlib.sha256(data).hexdigest() == row['git_sha256']
    raw = (OUT / 'compute/compute-readback.raw').read_bytes()
    assert len(raw) == 1536
    actual = struct.unpack('<384I', raw)
    expected = []
    for z in range(4):
        for y in range(6):
            for x in range(4):
                gx, gy, gz = x // 2, y // 3, z // 2
                tx, ty, tz = x % 2, y % 3, z % 2
                expected.extend([x | y << 8 | z << 16, gx | gy << 8 | gz << 16,
                                 tx | ty << 8 | tz << 16, tx + 2 * ty + 6 * tz])
    assert tuple(expected) == actual
    text = (OUT / 'probe.stdout.raw').read_text(encoding='utf-8-sig')
    words = re.findall(r'D3D11_KMT_COMPUTE_WORD element=(\d+) component=([0-3]) actual=([0-9a-f]{8}) expected=([0-9a-f]{8})', text)
    assert len(words) == 384
    for index, row in enumerate(words):
        assert (int(row[0]), int(row[1])) == (index // 4, index % 4)
        assert int(row[2], 16) == int(row[3], 16) == actual[index]
    result = read(OUT / 'probe-result.json')
    task = read(TASK / 'task-result.json')
    assert result['completed'] and result['probe_passed'] and task['completed'] and task['probe_passed']
    for token in [result['process_token'], task['process_token']]:
        assert (token['user'], token['sid'], token['session_id'], token['elevated'], token['elevation_type'], token['integrity_rid']) == (
            'DROIDVM\\USER', 'S-1-5-21-362894365-441372107-2852668596-1000', 1, False, 3, 8192)
    child = result['owned_child']
    assert child['Exited'] and child['ExitCodeAvailable'] and child['ExitCode'] == 0
    assert child['PipesDrained'] and not child['TimedOut'] and not child['ChildStillRunning']
    assert result['original_inputs_before'] == result['original_inputs_after']
    assert result['staged_inputs_before'] == result['staged_inputs_after']
    assert result['desktop_retained'] and result['binding_retained'] and result['environment_restored']
    auth = read(ROOT / 'ownership-authorization.json')
    assert pin(ROOT / 'ready-compute-gpu-manifest-08.json')['sha256'] == auth['manifest_sha256']
    assert result['authorization_sha256'] == pin(ROOT / 'ownership-authorization.json')['sha256']
    assert read(OUT / 'ownership-authorization-original.json') == auth
    registration = BASE / 'compute-gpu-registration-follow-on-09/verify-registration-originals-09.py'
    assert pin(registration)['sha256'] == '28f29b0e69e06adb78fc2d6da10d3cd3d1709cfbb7da4619c47b0f90cbbbe051'
    spec = importlib.util.spec_from_file_location('frozen_static34', registration)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    static = module.verify_registration(read(OUT / 'readiness-before.json'), read(OUT / 'readiness-after.json'))
    assert len(static['protected_static_value_names']) == 34
    cleanup = read(ROOT / 'originals/DxvkComputeGPU08Owners-d7e5c7d-gpu-08/Result-owned-cleanup-control-original.json')
    assert cleanup['passed'] and cleanup['task_absent_after']
    hosts = sorted(ROOT.glob('*.original-command.json'))
    assert len(hosts) == 9
    for path in hosts:
        row = read(path)
        assert row['exit_code'] == 0 and row['timed_out'] is False
        for key in ['stdout', 'stderr']:
            original = row[key]
            local = pin(Path(original['path']))
            assert local == original
    release = ROOT / 'target-gpu-release-08.json'
    # Closure and release must be reviewed separately before target handoff.
    proof = dict(schema='root-gpu08-original-direct-review-v1', passed=True,
                 source_commit=manifest['source_commit'], CI_run=manifest['ci']['run_id'],
                 archive=pin(archive), directly_joined_tar_members=126, directly_joined_inputs=47,
                 raw_Git_inputs=19, actual_words=384, readback=pin(OUT / 'compute/compute-readback.raw'),
                 actual_callback_tuple=reviewed['oracle']['callback_counts'], actual_identity=(2, 0, 2200),
                 native_probe_child=child, host_transports=len(hosts),
                 static34=static, limited_USER_session1=True, task_absent=True,
                 original_strict_review=pin(ROOT / 'native-d3d11-compute-originals-verified-08.json'),
                 real_compute_acceptance=True, ordinary_runtime_admission=False, presentation=False,
                 target_calls=False, source=pin(Path(__file__)))
    with (ROOT / 'root-GPU08-originals-direct-review.json').open('x') as out:
        json.dump(proof, out, indent=2)
        out.write('\n')
    print('ROOT_GPU08_ORIGINALS_PASS members=126 inputs=47 Git=19 words=384 USER_session=1 ordinary_runtime=0')


if __name__ == '__main__':
    main()
