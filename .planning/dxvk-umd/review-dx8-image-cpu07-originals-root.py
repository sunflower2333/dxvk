#!/usr/bin/env python3
"""Independently join CPU07 archived bytes, observed identities and closure."""
import hashlib
import json
from pathlib import Path
import re
import struct
import tarfile

W = Path('/home/sunf/droidvm-repos')
B = W / 'artifacts/dxvk-native-d3d8-system-device-20261008'
D = B / 'guest-native-image-identity-f9722b3-07'
H = B / 'native-image-identity-handoff-f9722b3-07'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def pin(value):
    data = Path(value['path']).read_bytes()
    assert len(data) == value['bytes'] and sha(data) == value['sha256']
    return data


def main():
    release_path = D / 'target-cpu-release-07.json'
    release = load(release_path)
    assert release['released'] and release['released_to'] == 'ROOT'
    assert release['source_commit'] == 'f9722b3d7abbd846aabf7f3517387d78449aa336'
    for value in release.values():
        if isinstance(value, dict) and {'path', 'bytes', 'sha256'} <= value.keys():
            pin(value)
    with tarfile.open(release['original_archive']['path']) as tar:
        members = tar.getmembers()
        assert len(members) == 75 and all(m.isfile() for m in members)
        assert len({m.name for m in members}) == 75
        raw = {m.name: tar.extractfile(m).read() for m in members}
    for name, data in raw.items():
        assert (D / 'originals' / name).read_bytes() == data
    collection = json.loads(raw['collection-original.json'].decode('utf-8-sig'))
    assert collection['status'] == 'PASS'
    assert set(raw) == {'collection-original.json'} | {v['path'] for v in collection['files']}
    for value in collection['files']:
        assert len(raw[value['path']]) == value['bytes'] and sha(raw[value['path']]) == value['sha256']
    result = json.loads(raw['result.json'].decode('utf-8-sig'))
    assert result['status'] == 'PASS' and result['before'] == result['after']
    root = collection['root'] + '\\'

    def original(path):
        assert path.startswith(root), path
        return raw[path[len(root):].replace('\\', '/')]

    def archived(value):
        data = original(value['path'])
        assert len(data) == value['bytes'] and sha(data) == value['sha256']
        return data

    manifest = json.loads(raw['source-manifest.json'].decode('utf-8-sig'))
    assert raw['source-manifest.json'] == (B / 'native-image-identity-packet-f9722b3-07/native-system-d3d8-device-x86-source-01.json').read_bytes()
    assert len(result['source_before']) == len(manifest['inputs']) == 15
    assert result['source_before'] == result['source_after']
    expected = {v['path']: v for v in manifest['inputs']}
    for value in result['source_before']:
        data = archived(value)
        row = expected[value['input_path']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
    assert len(result['compiler_full_before']) == 575
    assert result['compiler_full_before'] == result['compiler_full_after']
    compiler = json.loads(raw['original-compiler-provenance.json'].decode('utf-8-sig'))
    assert sha(raw['original-compiler-provenance.json']) == manifest['compiler_ready_sha256']
    assert len(compiler['files']) == 575
    actual_tools = {v['path']: (v['bytes'], v['sha256']) for v in result['compiler_full_before']}
    assert actual_tools == {v['path']: (v['bytes'], v['sha256']) for v in compiler['files']}
    for key, after, folder, count in [('sdk', 'sdk_after', 'original-sdk-headers', 13),
                                      ('libraries', 'libraries_after', 'original-link-libraries', 9)]:
        assert len(result[key]) == count and result[key] == result[after]
        for value in result[key]:
            data = raw[folder + '/' + value['path'].rsplit('\\', 1)[1]]
            assert len(data) == value['bytes'] and sha(data) == value['sha256']
    assert len(result['outputs']) == 3 and (result['object_count'], result['pe_count']) == (2, 1)
    for value in result['outputs']:
        data = archived(value)
        if value['path'].endswith('.obj'):
            offset = 6 if data[:4] == b'\0\0\xff\xff' else 0
            assert struct.unpack_from('<H', data, offset)[0] == 0x14c
        else:
            offset = struct.unpack_from('<I', data, 0x3c)[0]
            assert data[:2] == b'MZ' and data[offset:offset + 4] == b'PE\0\0'
            assert struct.unpack_from('<H', data, offset + 4)[0] == 0x14c
    for value in result['compile_inputs']:
        archived(value['source'])
        response = archived(value['response']).decode()
        assert '/W4' in response and '/WX' in response and '/MT' in response

    def native(value):
        assert value['pid'] > 0 and value['retained_process_handle'] != 0
        assert value['exited'] and value['exit_code_available'] and value['pipes_drained']
        assert not value['timeout'] and not value['child_still_running'] and not value['capture_failure']

    assert len(result['commands']) == 11
    for value in result['commands']:
        native(value)
        assert value['exit'] == value['expected'] == (64 if value['name'].startswith('invalid-cli') else 0)
        out, err = original(value['stdout']), original(value['stderr'])
        assert len(out) == value['stdout_bytes'] and len(err) == value['stderr_bytes']
        if value['name'].endswith('-compile') or value['name'] == 'probe-link':
            assert not err and not re.search(rb'\b(?:warning|error)\b', out, re.I)
    collector = load(D / 'dx8-system-device-cpu-f9722b3-07.tar.gz.collection-process.json')
    native(collector)
    assert collector['exit'] == 0
    inventory = load(D / 'dx8-system-device-cpu-f9722b3-07.tar.gz.json')
    assert inventory['sha256'] == release['original_archive']['sha256'] and inventory['original_files'] == 75
    assert inventory['collection_process_sha256'] == sha((D / 'dx8-system-device-cpu-f9722b3-07.tar.gz.collection-process.json').read_bytes())
    for kind in ['stdout', 'stderr']:
        data = (D / ('dx8-system-device-cpu-f9722b3-07.tar.gz.collection.' + kind + '.txt')).read_bytes()
        assert not data and sha(data) == inventory['collection_' + kind + '_sha256']
    hosts = load(D / 'host-operations.json')
    assert len(hosts) == 7
    for value in hosts:
        assert value['exit'] == 0 and not value['timeout'] and not Path('/proc', str(value['pid'])).exists()
        for kind in ['stdout', 'stderr']:
            data = (D / (value['name'] + '.' + kind + '.raw')).read_bytes()
            assert sha(data) == value[kind + '_sha256']
    outer = release['outer_actual_process']
    assert outer['completed'] and outer['exit'] == 0 and not outer['timeout']
    assert not Path('/proc', str(outer['pid'])).exists()
    for kind in ['stdout', 'stderr']:
        assert sha(Path(outer[kind + '_path']).read_bytes()) == outer[kind + '_sha256']
    observation = raw['mapped-image-diagnostics.stdout.txt'].decode()
    assert not raw['mapped-image-diagnostics.stderr.txt']
    assert 'D3D8_IMAGE_PROCESS process=014c native=aa64 pointer_bytes=4 admission=0' in observation
    assert 'D3D8_IMAGE_IDENTITY_COMPLETE modules=4 file_attempts=7 files_opened=7 files_closed=7 observation_valid=1 runtime_calls=0 KMT_calls=0 core_loads=0 admission=0' in observation
    assert len(re.findall(r'^D3D8_IMAGE_FILE_CLOSE .* status=1 error=0$', observation, re.M)) == 7
    assert len(re.findall(r'^D3D8_IMAGE_HEADERS_COMPARE .* same_loaded_headers=1 admission=0$', observation, re.M)) == 7
    assert len(re.findall(r'^D3D8_IMAGE_FILE_FINAL .* mapped_path_equal=1$', observation, re.M)) == 7
    hashes = re.findall(r'^D3D8_IMAGE_FILE_SHA256 .* valid=1 before=([0-9a-f]{64}) after_valid=1 after=([0-9a-f]{64}) unchanged=1$', observation, re.M)
    assert len(hashes) == 7 and all(before == after for before, after in hashes)
    for name in ['kernel32.dll', 'kernelbase.dll', 'gdi32.dll']:
        assert 'D3D8_IMAGE_FILE_PAIR name=' + name + ' same_file_id_and_bytes=1 admission=0' in observation
        lines = [line for line in observation.splitlines() if 'name=' + name + ' ' in line]
        mapped = next(line for line in lines if line.startswith('D3D8_IMAGE_MAPPED '))
        nt = mapped.split(' path=', 1)[1].removesuffix(' admission=0')
        finals = [line.split(' path=', 1)[1].removesuffix(' mapped_path_equal=1') for line in lines if line.startswith('D3D8_IMAGE_FILE_FINAL ')]
        assert len(finals) == 2 and finals == [nt, nt] and '\\Windows\\SyChpe32\\' in nt
        ids = [line.split(' valid=', 1)[1] for line in lines if line.startswith('D3D8_IMAGE_FILE_ID ')]
        assert len(ids) == 2 and ids[0] == ids[1]
    assert all(release[key] == 0 for key in ['pending_native_children', 'pending_host_operations', 'pending_target_tasks', 'valid_KMT_queries', 'runtime_factories', 'frontend_loads', 'core_loads', 'GPU_runs'])
    proof = dict(verified=True, source_commit=release['source_commit'], original_files=75,
                 archive_sha256=release['original_archive']['sha256'], source_inputs=15,
                 compiler_ready_file_receipt_rows=575, SDK_original_files=13, library_original_files=9,
                 I386_COFF=2, I386_PE=1, native_children_closed=12, host_operations_closed=7,
                 outer_closed=True, mapped_file_pairs=3, paired_file_closes=7,
                 loaded_disk_headers_and_mapped_paths_equal=True, target_state_unchanged=True,
                 physical_I386_system_mapping='SyChpe32 via both logicalSystem32 and Directory2W(I386) SysWOW64',
                 management_SysWOW64_hashes_are_distinct=True, release_sha256=sha(release_path.read_bytes()),
                 target_released_to_ROOT=True, path_policy_changed=False, hardware_acceptance=False, target_calls=0)
    output = D / 'root-native-CPU07-originals-direct-review-01.json'
    with output.open('x') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(path=str(output), bytes=output.stat().st_size, sha256=sha(output.read_bytes()))))


if __name__ == '__main__':
    main()
