#!/usr/bin/env python3
"""Root read-only review of the actual d7 locked-buffer capture and release."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tarfile

WORKSPACE = Path('/home/sunf/droidvm-repos')
BASE = WORKSPACE / 'artifacts/dxvk-native-d3d9-locked-draw-20261007/current-d7-locked-gpu-retry-01'
GIT = WORKSPACE / 'dxvk-umd-ci'

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def sha(data):
    return hashlib.sha256(data).hexdigest()

def pin(row):
    path = Path(row['path'])
    data = path.read_bytes()
    assert len(data) == row['bytes'] and sha(data) == row['sha256'], path
    return path

def checked_local_pins(value):
    if isinstance(value, dict):
        if {'path', 'bytes', 'sha256'} <= value.keys() and str(value['path']).startswith('/home/'):
            pin(value)
        for child in value.values():
            checked_local_pins(child)
    elif isinstance(value, list):
        for child in value:
            checked_local_pins(child)

def main():
    release = read(BASE / 'target-d3d9-locked-release-01.json')
    assert release['released'] is True and release['released_to'] == '/root'
    checked_local_pins(release)
    table = read(pin(release['archive_member_review']))
    rows = {r['member']: r for r in table['files']}
    assert len(rows) == 143
    with tarfile.open(pin(release['archive'])) as archive:
        members = archive.getmembers()
        assert len(members) == 199
        regular = [m for m in members if m.isfile()]
        assert len(regular) == 143 and {m.name for m in regular} == set(rows)
        for member in regular:
            data = archive.extractfile(member).read()
            row = rows[member.name]
            assert len(data) == row['original']['bytes'] and sha(data) == row['original']['sha256']
            assert pin(row['retained']).read_bytes() == data
    root = BASE / 'originals/DxvkD3D9LockedInteractive-d7e5c7d-locked-01'
    output = root / 'output'
    manifest = read(pin(release['manifest']))
    assert (output / 'pinned-inputs-original.json').read_bytes() == pin(release['manifest']).read_bytes()
    files = {r['role']: r for r in manifest['files']}
    assert len(files) == 63
    for row in files.values():
        data = (output / row['destination']).read_bytes()
        assert len(data) == row['bytes'] and sha(data) == row['sha256'], row['role']
    git_rows = manifest['source_inputs'] + manifest['d3d9_source_inputs']
    assert len(git_rows) == 32
    for row in git_rows:
        data = subprocess.run(['git', '-C', str(GIT), 'show', row['source_commit'] + ':' + row['git_path']], check=True, capture_output=True).stdout
        assert sha(data) == row['git_sha256'] == files[row['file_role']]['sha256']
    result = read(output / 'probe-result.json')
    assert result['completed'] and result['probe_passed'] and result['exit'] == 0 and result['failure'] is None
    assert result['source_commit'] == manifest['source_commit'] == 'd7e5c7d46b8ce889e993bfab66a3b78b076c49d1'
    assert result['ci_run'] == manifest['ci']['run_id'] == 37648387721
    assert result['adapter_luid'] == 'ec6b000000000000'
    assert result['pixels_verified'] == 2624 and result['locked_buffer_pixels_verified'] == 1152
    token = result['process_token']
    assert token['user'] == 'DROIDVM\\USER' and token['session_id'] == 1 and not token['elevated']
    assert token['elevation_type'] == 3 and token['integrity_rid'] == 8192
    for category in ('original_inputs', 'staged_inputs'):
        assert result[category + '_retained'] and result[category + '_before'] == result[category + '_after']
        assert {(r['role'], r['bytes'], r['sha256']) for r in result[category + '_before']} == {(r['role'], r['bytes'], r['sha256']) for r in files.values()}
    stdout = (output / 'probe.stdout.raw').read_text()
    oracle = read(output / files['d3d9-locked-oracle']['destination'])
    assert sha((output / files['d3d9-locked-oracle']['destination']).read_bytes()) == 'b28d0953126864ffb2538122f7bb8d50441b484e4de55f0bf99ebb868d063915'
    expected = [(str(s['stage']), str(x), str(y), s['expected_color']) for s in oracle['new_stages'] for y in range(8) for x in range(8)]
    actual = re.findall(r'D3D9_LOCKED_BUFFER_PIXEL stage=(\d+) x=(\d+) y=(\d+) value=([0-9a-f]{8})', stdout)
    assert len(actual) == 1152 and actual == expected
    word = byte = 2166136261
    for _, _, _, value in actual:
        color = int(value, 16)
        word = ((word ^ color) * 16777619) & 0xffffffff
        for channel in color.to_bytes(4, 'little'):
            byte = ((byte ^ channel) * 16777619) & 0xffffffff
    assert f'{word:08x}' == '6d390bc5' and f'{byte:08x}' == '41e03dc5'
    assert 'D3D9_CALLBACKS query=1765 context=1/1 allocation=15/15 lock=14/14 render=42 escape=328 wrong_thread=0' in stdout
    assert 'KMT_RESIDENCY references=15 evictions=15 remaining=0' in stdout
    for fragment in ('D3D9_CLEAR_READBACK PASS pixels=192 checksum=ffefa655', 'D3D9_DRAW_READBACK PASS pixels=192 checksum=53a03d45', 'D3D9_SHADER_READBACK PASS pixels=192 checksum=1384c5a5', 'D3D9_TEXTURE_READBACK PASS pixels=512 checksum=d3afb9c5', 'D3D9_BUFFER_READBACK PASS pixels=384 checksum=621cd685'):
        assert fragment in stdout
    before, after = (read(output / ('readiness-' + label + '.json')) for label in ('before', 'after'))
    assert before == result['readiness_before'] and after == result['readiness_after']
    assert before['desktop'] == after['desktop'] and before['desktop_processes_complete'] == after['desktop_processes_complete']
    for key in ('device_id', 'driver_binding', 'service', 'installed_package', 'binary', 'service_state', 'pnp_status', 'pnp_error'):
        assert before['active_device'][key] == after['active_device'][key], key
    assert before['active_device']['binary']['sha256'] == 'd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a'
    original_review = read(pin(release['hardware_original_review']))
    names = original_review['static34']['protected_static_value_names']
    assert len(names) == 34
    for key in ('values',):
        assert {n: before['active_device'][key].get(n) for n in names} == {n: after['active_device'][key].get(n) for n in names}
    a = {r['key']: r['values'] for r in before['driver_registry']}
    z = {r['key']: r['values'] for r in after['driver_registry']}
    assert len(a) == len(z) == 2 and set(a) == set(z)
    for key in a:
        assert {n: a[key].get(n) for n in names} == {n: z[key].get(n) for n in names}
    owners = read(pin(release['ownership_original_review']))
    checked_local_pins(owners)
    assert len(owners['native_owned_children']) == 10 and len(owners['host_transports']) == 9
    for row in owners['native_owned_children']:
        receipt = read(pin(row['evidence']))
        if receipt.get('schema') == 'd3d9-locked-native-owned-collection-v1':
            assert receipt['passed'] and receipt['failure'] is None
            receipt = receipt['collection']
        child = receipt.get('owned_child', receipt)
        assert child == row['actual_child']
        assert child['Pid'] > 0 and child['ProcessHandle'] != 0
        assert child['Exited'] and child['ExitCodeAvailable'] and child['ExitCode'] == 0 and child['PipesDrained']
        assert not child['TimedOut'] and not child['ChildStillRunning'] and not child['Failure']
        assert pin(row['raw_streams']['stdout']).stat().st_size == child['StdoutBytes']
        assert pin(row['raw_streams']['stderr']).stat().st_size == child['StderrBytes']
    for row in owners['host_transports']:
        receipt = read(pin(row['evidence']))
        assert receipt == row['actual_receipt'] and receipt['exit_code'] == 0 and not receipt['timed_out']
        assert receipt['local_owned_transport_pid'] > 0
        checked_local_pins(receipt)
    collection = read(root / 'task-collection.json')
    assert collection['task_removed'] and collection['task_exit'] == 0 and not collection['timed_out']
    cleanup = read(pin(owners['final_cleanup']))
    assert owners['owned_task_absent_after'] and owners['owned_task_removed']
    for key in ('pending_native_owned_children', 'pending_host_transports', 'pending_collections', 'pending_transfers'):
        assert release[key] == 0
    assert release['matching_live_target_transports'] == []
    proof = dict(schema='root-d3d9-locked-originals-direct-review-v1', verified=True, passed=True,
                 archive_regular_files=143, archive_members=199, input_roles=63, raw_Git_inputs=32,
                 source_commit=manifest['source_commit'], ci_run=37648387721, core_sha256=files['core']['sha256'],
                 native_asserted_pixels=2624, root_independent_locked_pixels=1152, stages=18,
                 word_checksum=f'{word:08x}', byte_checksum=f'{byte:08x}', static_registration_values=34,
                 native_children_closed=10, host_transports_closed=9, exact_task_removed=True,
                 release_accepted=True, ordinary_runtime_admission=False, presentation=False,
                 latest_618_source_hardware_admission=False, target_calls=False)
    destination = BASE / 'root-D3D9-locked-originals-direct-review-01.json'
    with destination.open('x') as stream:
        json.dump(proof, stream, indent=2); stream.write('\n')
    print(json.dumps(proof))

if __name__ == '__main__':
    main()
