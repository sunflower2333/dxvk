#!/usr/bin/env python3
"""Read actual native I386 outputs, source provenance and closed CPU05 owners."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tarfile

WORKSPACE = Path('/home/sunf/droidvm-repos')
BASE = WORKSPACE / 'artifacts/dxvk-native-d3d8-system-device-20261008/guest-native-device-5c420e4-05'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def decode(data):
    return json.loads(data.decode('utf-8-sig'))

def closed(child, expected):
    assert child['pid'] > 0 and child['retained_process_handle'] != 0
    assert child['exited'] and child['exit_code_available'] and child['exit'] == expected
    assert not child['timeout'] and not child['child_still_running'] and child['pipes_drained']
    assert not child['capture_failure']

def main():
    archive = BASE / 'dx8-system-device-cpu-5c420e4-05.tar.gz'
    meta = read(Path(str(archive) + '.json'))
    assert archive.stat().st_size == meta['bytes'] == 31169398
    assert sha(archive.read_bytes()) == meta['sha256'] == 'c48fbc5ddc20123352bfa7be3ea9bcbb67a0a0c7a83f65cdd5e1b04897ad2dc9'
    with tarfile.open(archive) as tar:
        originals = {m.name: tar.extractfile(m).read() for m in tar.getmembers() if m.isfile()}
    assert len(originals) == meta['original_files'] == 181
    collection = decode(originals['collection-original.json'])
    assert len(collection['files']) == 180
    for row in collection['files']:
        data = originals[row['path']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
    result = decode(originals['result.json'])
    assert result['status'] == 'PASS' and result['source_commit'] == '5c420e4daddc39effb2c8e8a28bd07ec7407c402'
    assert result['gpu_runs'] == result['system_runtime_calls'] == result['selector_calls'] == 0
    assert not result['installation'] and not result['production_core_built']
    manifest = decode(originals['source-manifest.json'])
    assert len(manifest['inputs']) == 33
    assert result['source_before'] == result['source_after'] and len(result['source_before']) == 33
    for row in manifest['inputs']:
        data = originals['source/' + row['path']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        if 'git_commit' in row:
            raw = subprocess.run(['git', '-C', str(WORKSPACE / 'dxvk-umd-ci'), 'show', row['git_commit'] + ':' + row['path']], check=True, capture_output=True).stdout
            assert raw == data
    provenance = decode(originals['original-compiler-provenance.json'])
    assert sha(originals['original-compiler-provenance.json']) == result['compiler_provenance']['sha256'] == 'c6333c67b4f3725f513e82c488b844054859b28456bccb0131eb59b805a5db48'
    assert provenance['file_count'] == len(provenance['files']) == 575
    assert result['compiler_full_before'] == result['compiler_full_after'] and len(result['compiler_full_before']) == 575
    expected_tools = {(r['path'], r['bytes'], r['sha256']) for r in provenance['files']}
    assert {(r['path'], r['bytes'], r['sha256']) for r in result['compiler_full_before']} == expected_tools
    assert result['sdk'] == result['sdk_after'] and len(result['sdk']) == 10
    assert result['libraries'] == result['libraries_after'] and len(result['libraries']) == 9
    assert result['raw_process_source'] == result['raw_process_source_after']
    assert result['raw_process_source']['sha256'] == 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
    assert result['before'] == result['after']
    assert result['before']['sys']['sha256'] == 'd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a'
    root = 'C:\\Users\\Public\\DxvkD3D8Runtime-5c420e4-05\\'
    def local(name):
        assert name.startswith(root)
        return name[len(root):].replace('\\', '/')
    commands = result['commands']
    assert len(commands) == 48
    guards = [r for r in commands if r['name'].startswith('invalid-cli-')]
    assert len(guards) == result['malformed_cli_guards'] == 30
    for command in commands:
        closed(command, command['expected'])
        assert len(originals[local(command['stdout'])]) == command['stdout_bytes']
        assert len(originals[local(command['stderr'])]) == command['stderr_bytes']
        if command['name'].endswith('-compile'):
            assert command['expected'] == 0
            assert not originals[local(command['stderr'])]
            assert b'warning ' not in originals[local(command['stdout'])] and b'error ' not in originals[local(command['stdout'])]
    for row in result['compile_inputs']:
        response = originals[local(row['response']['path'])].decode('utf-8-sig').splitlines()
        assert '/W4' in response and '/WX' in response and '/MT' in response
    assert len(result['outputs']) == 9 and result['object_count'] == 5 and result['pe_count'] == 4
    for row in result['outputs']:
        data = originals[local(row['path'])]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        if data[:2] == b'MZ':
            offset = struct.unpack_from('<I', data, 0x3c)[0]
            assert data[offset:offset+4] == b'PE\0\0' and struct.unpack_from('<H', data, offset+4)[0] == 0x14c
        else:
            offset = 6 if data[:4] == b'\0\0\xff\xff' else 0
            assert struct.unpack_from('<H', data, offset)[0] == 0x14c
    assert result['policy_checks'] == 329 and result['callback_checks'] == 62
    assert b'checks=329' in originals['policy-positive.stdout.txt']
    assert b'checks=62' in originals['callbacks-positive.stdout.txt'] and b'forwarded=11' in originals['callbacks-positive.stdout.txt']
    collector = read(Path(str(archive) + '.collection-process.json'))
    closed(collector, 0)
    assert collector['runner_sha256'] == result['raw_process_source']['sha256']
    assert collector['stdout_bytes'] == Path(str(archive) + '.collection.stdout.txt').stat().st_size
    assert collector['stderr_bytes'] == Path(str(archive) + '.collection.stderr.txt').stat().st_size
    hosts = read(BASE / 'host-operations.json')
    assert len(hosts) == 7
    for row in hosts:
        assert row['pid'] > 0 and row['exit'] == 0 and not row['timeout']
        for stream in ('stdout', 'stderr'):
            assert sha((BASE / (row['name'] + '.' + stream + '.raw')).read_bytes()) == row[stream + '_sha256']
    proof = dict(schema='root-DX8-CPU05-originals-direct-review-v1', verified=True, passed=True,
                 archive_sha256=meta['sha256'], original_files=181, source_commit=result['source_commit'],
                 original_source_files=33, raw_Git_inputs=30, compiler_original_files=575,
                 original_SDK_headers=10, original_libraries=9, I386_COFF=5, I386_PE=4,
                 policy_checks=329, callback_checks=62, forwarded_callbacks=11, invalid_CLI_guards=30,
                 native_closed_children=49, host_closed_transports=7, full_before_after_unchanged=True,
                 native_system_device=False, GPU=False, target_calls=False)
    with (BASE / 'root-native-CPU05-originals-direct-review-01.json').open('x') as stream:
        json.dump(proof, stream, indent=2); stream.write('\n')
    print(json.dumps(proof))

if __name__ == '__main__':
    main()
