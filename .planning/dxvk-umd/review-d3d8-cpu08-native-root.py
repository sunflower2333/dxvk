#!/usr/bin/env python3
"""Independently reopen the actual CPU08 originals, without target calls."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import struct
import subprocess
import tarfile

W = Path('/home/sunf/droidvm-repos')
B = W / 'artifacts/dxvk-native-d3d8-system-device-20261008'
P = B / 'native-system-file-identity-packet-66bfbdf-08'
D = B / 'guest-native-system-file-identity-66bfbdf-08'
C = '66bfbdf73d32d7213af439a69cb569730b018f55'
V = '8553425f1ed37524f67951f296a98b6e5c47f69e'
R = W / 'dxvk-umd-ci'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(data):
    return json.loads(data.decode('utf-8-sig'))


def read(path):
    return load(path.read_bytes())


archive = D / 'dx8-system-device-cpu-66bfbdf-08.tar.gz'
assert archive.stat().st_size == 28175314
assert sha(archive.read_bytes()) == 'c3bde12514d3bbbd4a9a5f48bca03038e8b99c3a3e88a4c3fa77e79e8fe7337f'
with tarfile.open(archive) as tar:
    entries = tar.getmembers()
    assert len({row.name for row in entries}) == len(entries)
    assert all(not PurePosixPath(row.name).is_absolute()
               and '..' not in PurePosixPath(row.name).parts
               and (row.isfile() or row.isdir()) for row in entries)
    members = {row.name: tar.extractfile(row).read() for row in entries if row.isfile()}
assert len(members) == 107
result = load(members['result.json'])
validator = R / 'scripts/verify-native-d3d8-system-phase.py'
assert validator.read_bytes() == subprocess.check_output([
    'git', '-C', str(R), 'show', V + ':scripts/verify-native-d3d8-system-phase.py'])
spec = importlib.util.spec_from_file_location('cpu08_frozen_validator', validator)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
details = module.verify_native_identity_cpu(result, members)

manifest = read(P / 'native-system-d3d8-device-x86-source-01.json')
assert members['source-manifest.json'] == (P / 'native-system-d3d8-device-x86-source-01.json').read_bytes()
git_count = 0
for row in manifest['inputs']:
    data = members['source/' + row['path']]
    assert len(data) == row['bytes'] and sha(data) == row['sha256']
    if 'git_commit' in row:
        assert row['git_commit'] == C
        assert data == subprocess.check_output(['git', '-C', str(R), 'show', C + ':' + row['path']])
        git_count += 1
assert git_count == 18

collector = load(members['collection-original.json'])
assert collector['status'] == 'PASS' and collector['source_commit'] == C
for row in collector['files']:
    data = members[row['path']]
    assert len(data) == row['bytes'] and sha(data) == row['sha256']
assert len(collector['files']) == 106
prefix = collector['root'] + chr(92)
coff = pe = 0
for row in result['outputs']:
    assert row['path'].startswith(prefix)
    data = members[row['path'][len(prefix):].replace(chr(92), '/')]
    assert len(data) == row['bytes'] and sha(data) == row['sha256']
    if data[:2] == b'MZ':
        offset = struct.unpack_from('<I', data, 0x3c)[0]
        assert data[offset:offset + 4] == b'PE\0\0'
        machine = struct.unpack_from('<H', data, offset + 4)[0]
        pe += 1
    else:
        machine = struct.unpack_from('<H', data, 6 if data[:4] == b'\0\0\xff\xff' else 0)[0]
        coff += 1
    assert machine == row['machine'] == 0x14c
assert coff == 4 and pe == 3

parser = read(D / 'native-parser-01.json')
assert parser['status'] == 'PASS' and parser['source_commit'] == C
assert parser['raw_process_type_compiled'] and parser['inputs_before'] == parser['inputs_after']
assert len(parser['inputs_before']) == 6
for row in parser['inputs_before']:
    data = (P / row['name']).read_bytes()
    assert len(data) == row['bytes'] and sha(data) == row['sha256']
assert len(parser['script_parse']) == 2
assert all(row['parse_errors'] == 0 and not row['errors'] for row in parser['script_parse'])
assert parser['core_loaded'] is False and parser['installation'] is False
assert all(parser[key] == 0 for key in ('executable_launches', 'system_runtime_calls', 'selector_calls', 'gpu_runs'))

operations = read(D / 'host-operations.json')
assert len(operations) == 7
for row in operations:
    assert row['exit'] == 0 and not row['timeout']
    for stream in ('stdout', 'stderr'):
        assert sha((D / (row['name'] + '.' + stream + '.raw')).read_bytes()) == row[stream + '_sha256']
    try:
        os.kill(row['pid'], 0)
    except ProcessLookupError:
        pass
    else:
        raise AssertionError('host PID remains live: ' + str(row['pid']))
collection = read(D / (archive.name + '.collection-process.json'))
assert collection['pid'] > 0 and collection['retained_process_handle'] != 0
assert collection['exited'] and collection['exit_code_available'] and collection['exit'] == 0
assert collection['pipes_drained'] and not collection['timeout'] and not collection['child_still_running']
assert not collection['capture_failure']
for stream in ('stdout', 'stderr'):
    data = (D / (archive.name + '.collection.' + stream + '.txt')).read_bytes()
    assert len(data) == collection[stream + '_bytes'] == 0

proof = dict(verified=True, source_commit=C, validator_commit=V,
             archive_sha256=sha(archive.read_bytes()), original_files=107,
             direct_Git_sources=18, original_licensed_sources=3,
             original_collected_files_rehashed=106, I386_COFF=4, I386_PE=3,
             native_PS51_parsed_helpers=2, original_parser_inputs=6,
             native_AddType_pass=True, native_build_children_closed=18,
             original_collector_child_closed=True, host_children_closed_absent=7,
             details=details, target_calls=0, hardware_acceptance=False,
             ordinary_runtime_admission=False)
output = D / 'root-native-originals-review-08.json'
assert not output.exists()
output.write_text(json.dumps(proof, indent=2) + '\n')
print(json.dumps(dict(path=str(output), bytes=output.stat().st_size, sha256=sha(output.read_bytes()))))
