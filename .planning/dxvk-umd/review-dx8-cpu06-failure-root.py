#!/usr/bin/env python3
"""Independently join CPU06 failure originals and the post-failure release."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tarfile

WORKSPACE = Path('/home/sunf/droidvm-repos')
BASE = WORKSPACE / 'artifacts/dxvk-native-d3d8-system-device-20261008/guest-native-device-db0e183-06'
COMMIT = 'db0e1833a0fc1c77435c118a56f0c0c43a665aa2'
RUNNER = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def decode(data):
    return json.loads(data.decode('utf-8-sig'))


def closed(row, expected):
    assert row['pid'] > 0 and row['retained_process_handle'] != 0
    assert row['exited'] and row['exit_code_available'] and row['exit'] == expected
    assert row['pipes_drained'] and not row['timeout'] and not row['child_still_running']
    assert not row['capture_failure']


def pin(row):
    path = Path(row['path'])
    data = path.read_bytes()
    assert len(data) == row['bytes'] and sha(data) == row['sha256']
    return data


def main():
    archive = BASE / 'dx8-system-device-cpu-db0e183-06.tar.gz'
    assert archive.stat().st_size == 31180717
    assert sha(archive.read_bytes()) == '4a153ecf3b66c106b6c0415dccd90344485398b66c14a62821f8d556f9d8bab0'
    with tarfile.open(archive) as tar:
        members = [m for m in tar.getmembers() if m.isfile()]
        assert len(members) == 118 and len({m.name for m in members}) == 118
        originals = {m.name: tar.extractfile(m).read() for m in members}
    collection = decode(originals['collection-original.json'])
    assert len(collection['files']) == 117
    for row in collection['files']:
        data = originals[row['path']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        assert (BASE / 'originals' / row['path']).read_bytes() == data
    result = decode(originals['result.json'])
    assert result['status'] == 'FAIL' and result['error'] == 'process-api-diagnostics exit=1'
    assert result['source_commit'] == COMMIT and result['target_arch'] == 'x86'
    assert result['gpu_runs'] == result['system_runtime_calls'] == result['selector_calls'] == 0
    assert not result['installation'] and not result['production_core_built']
    manifest = decode(originals['source-manifest.json'])
    assert len(manifest['inputs']) == len(result['source_before']) == 33
    source_rows = {r['input_path']: r for r in result['source_before']}
    git_count = 0
    for row in manifest['inputs']:
        data = originals['source/' + row['path']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        assert source_rows[row['path']]['sha256'] == row['sha256']
        if 'git_commit' in row:
            git_count += 1
            raw = subprocess.run(['git', '-C', str(WORKSPACE / 'dxvk-umd-ci'),
                                  'show', row['git_commit'] + ':' + row['path']],
                                 capture_output=True, check=True).stdout
            assert raw == data
        else:
            origin = row
            while 'original_input' in origin:
                origin = origin['original_input']
            assert Path(origin['source_path']).read_bytes() == data
    assert git_count == 30
    provenance = decode(originals['original-compiler-provenance.json'])
    ready_sha = sha(originals['original-compiler-provenance.json'])
    assert ready_sha == result['compiler_provenance']['sha256'] == 'c6333c67b4f3725f513e82c488b844054859b28456bccb0131eb59b805a5db48'
    assert provenance['file_count'] == len(provenance['files']) == len(result['compiler_full_before']) == 575
    assert {(r['path'], r['bytes'], r['sha256']) for r in provenance['files']} == {
        (r['path'], r['bytes'], r['sha256']) for r in result['compiler_full_before']}
    assert result['raw_process_source']['sha256'] == RUNNER
    prefix = 'C:\\Users\\Public\\DxvkD3D8Runtime-db0e183-06\\'

    def local(path):
        assert path.startswith(prefix)
        return path[len(prefix):].replace('\\', '/')

    commands = result['commands']
    assert len(commands) == 16
    for index, row in enumerate(commands):
        expected = 1 if index == 15 else 0
        closed(row, expected)
        assert row['expected'] == 0
        for stream in ('stdout', 'stderr'):
            assert len(originals[local(row[stream])]) == row[stream + '_bytes']
        if row['name'].endswith('-compile'):
            assert not originals[local(row['stderr'])]
            output = originals[local(row['stdout'])].lower()
            assert b'warning ' not in output and b'error ' not in output
    assert commands[-1]['name'] == 'process-api-diagnostics'
    assert not any(r['name'].startswith('invalid-cli-') or r['name'] in
                   ('policy-positive', 'callbacks-positive') for r in commands)
    assert len(result['compile_inputs']) == 5
    for row in result['compile_inputs']:
        response = originals[local(row['response']['path'])].decode('utf-8-sig').splitlines()
        assert all(flag in response for flag in ('/W4', '/WX', '/MT'))
    coff = pe = 0
    for row in result['outputs']:
        data = originals[local(row['path'])]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        if data[:2] == b'MZ':
            pe += 1
            offset = struct.unpack_from('<I', data, 0x3c)[0]
            assert data[offset:offset+4] == b'PE\0\0'
            machine = struct.unpack_from('<H', data, offset+4)[0]
        else:
            coff += 1
            machine = struct.unpack_from('<H', data, 6 if data[:4] == b'\0\0\xff\xff' else 0)[0]
        assert machine == 0x14c
    assert (coff, pe) == (5, 4)
    stdout = originals['process-api-diagnostics.stdout.txt']
    assert len(stdout) == 3642 and not originals['process-api-diagnostics.stderr.txt']
    text = stdout.decode('ascii').replace('\r\n', '\n')
    for token in (
        'provider=kernel32.dll symbol=GetSystemWow64Directory2W present=0 error=127',
        'provider=kernelbase.dll symbol=GetSystemWow64Directory2W present=1 error=0',
        'symbol=IsWow64Process2 status=1 error=0 process=014c native=aa64',
        'effective=014c pointer_bytes=4 legacy_status=1 legacy_wow=1',
        'D3D8_FAILED reason=canonical-I386-Kernel32-provider'):
        assert token in text
    assert 'path=C:\\WINDOWS\\System32\\KERNEL32.DLL machine=014c' in text
    post = BASE / 'post-failure-snapshot-06'
    snapshot = read(post / 'snapshot-original.json')
    assert snapshot['status'] == 'PASS' and snapshot['source_commit'] == COMMIT
    assert snapshot['original_result_sha256'] == snapshot['original_result_after']['sha256'] == sha(originals['result.json'])
    for group, count in [('source_before', 33), ('compiler_full_before', 575), ('sdk', 11), ('libraries', 9)]:
        assert snapshot['groups'][group] == result[group] and len(result[group]) == count
    assert snapshot['compiler_ready_after']['sha256'] == ready_sha
    assert result['before'] == result['failure_state'] == snapshot['target_after']
    assert result['before']['sys']['sha256'] == 'd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a'
    for key in ('runtime_factories', 'KMT_calls', 'core_loads', 'GPU_runs', 'compiler_writes'):
        assert snapshot[key] == 0
    worker = read(post / 'snapshot-child-original.json')
    closed(worker, 0)
    assert worker['runner_sha256'] == RUNNER and worker['parser_errors'] == 0
    assert worker['input_before'] == worker['input_after']
    for stream in ('stdout', 'stderr'):
        assert (post / ('snapshot-worker.' + stream + '.raw')).stat().st_size == worker[stream + '_bytes']
    collector = read(Path(str(archive) + '.collection-process.json'))
    closed(collector, 0)
    assert collector['runner_sha256'] == RUNNER
    for stream in ('stdout', 'stderr'):
        assert Path(str(archive) + '.collection.' + stream + '.txt').stat().st_size == collector[stream + '_bytes']
    parser = read(BASE / 'native-parser-01.json')
    assert parser['status'] == 'PASS' and parser['inputs_before'] == parser['inputs_after']
    assert parser['raw_process_source_sha256'] == RUNNER and parser['raw_process_type_compiled']
    assert len(parser['script_parse']) == 2 and all(r['parse_errors'] == 0 for r in parser['script_parse'])
    hosts = []
    for directory in (BASE, post):
        for row in read(directory / 'host-operations.json'):
            assert row['pid'] > 0 and not row['timeout']
            assert row['exit'] == (1 if row['name'] == 'build-command' else 0)
            assert not Path('/proc', str(row['pid'])).exists()
            for stream in ('stdout', 'stderr'):
                assert sha((directory / (row['name'] + '.' + stream + '.raw')).read_bytes()) == row[stream + '_sha256']
            hosts.append(row)
    assert len(hosts) == 10
    release_path = BASE / 'target-cpu-release-06.json'
    assert release_path.stat().st_size == 9790 and sha(release_path.read_bytes()) == '831ea2c228e9dd15eafe751957fe56158838d3131f81c903c3a69a3955c43cd6'
    release = read(release_path)
    for key in ('authorization', 'original_archive', 'independent_original_review', 'independent_reviewer_command',
                'main_host_operations', 'post_failure_host_operations', 'post_failure_snapshot',
                'post_failure_native_worker', 'outer_process'):
        pin(release[key])
    assert release['target_release_explicit'] and release['target_owner_after'] == '/root'
    assert release['pending_native_children'] == release['pending_host_transports'] == 0
    native = commands + [collector, worker]
    assert release['native_children_total'] == len(release['native_children']) == len(native) == 18
    assert {(r['pid'], r['retained_process_handle'], r['exit']) for r in native} == {
        (r['pid'], r['retained_process_handle'], r['exit']) for r in release['native_children']}
    for row in release['host_operations']:
        assert row['own_host_pid_absent_after_wait']
    for key in ('tasks_created', 'compiler_writes', 'valid_KMT_queries', 'runtime_factories', 'core_loads', 'GPU_runs', 'registry_writes'):
        assert release[key] == 0
    for key in ('compiler_repair_replay', 'installation', 'VM_changes', 'retry', 'next_phase_executed'):
        assert not release[key]
    proof = dict(schema='root-DX8-CPU06-failure-originals-direct-review-v1', verified=True,
                 build_passed=False, compilation_passed=True, archive_sha256=sha(archive.read_bytes()),
                 original_files=118, source_commit=COMMIT, source_files=33, Git_inputs=30,
                 compiler_files=575, SDK_headers=11, libraries=9, I386_COFF=coff, I386_PE=pe,
                 before_failure_after_unchanged=True, native_children_closed=18, host_transports_closed=10,
                 exact_API_lookup_observed=True, physical_mapped_file_identity_unproven=True,
                 failure='canonical-I386-Kernel32-provider', policy_callback_CLI_unrun=True,
                 release_sha256=sha(release_path.read_bytes()), target_released_to_ROOT=True,
                 KMT=False, factory=False, core=False, GPU=False, target_calls=False)
    destination = BASE / 'root-native-CPU06-failure-originals-direct-review-01.json'
    with destination.open('x') as output:
        json.dump(proof, output, indent=2)
        output.write('\n')
    print(json.dumps(proof, sort_keys=True))


if __name__ == '__main__':
    main()
