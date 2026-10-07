#!/usr/bin/env python3
"""Read original CI3d ZIP members and independently join retained evidence."""
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import struct
import subprocess
import zipfile

ROOT = Path('/home/sunf/droidvm-repos')
REPO = ROOT / 'dxvk-umd-ci'
OUTPUT = ROOT / 'artifacts/dxvk-trunk-integration-20261008/successful-original-ci-3d39760-01'
SOURCE = '3d39760822662122a3c9d6fc7b8c9c665fa63bb6'
RUN = 37689715890
RAW = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(path.read_bytes().decode('utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=sha(data))


def check_process(row, members):
    assert row['pid'] > 0 and row['retained_process_handle'] > 0
    assert row['exit_code'] == row['expected_exit'] == 0
    assert row['exited'] and row['exit_code_available'] and row['pipes_drained']
    assert not row['timed_out'] and not row['child_still_running'] and not row['capture_failure']
    assert row['deadline_ms'] == 30000 and row['runner_sha256'] == RAW
    for stream in ['stdout', 'stderr']:
        assert len(members[row[stream + '_member']]) == row[stream + '_bytes']


def main():
    review = read(OUTPUT / 'root-current-native-ci-verified-01.json')
    assert review['verified'] and review['source_commit'] == SOURCE and review['run'] == RUN
    api = read(OUTPUT / 'ci-artifacts.json')
    actual = {row['id']: row for row in api['artifacts']}
    originals = {}
    count = 0
    for row in review['original_archives']:
        archive = OUTPUT / '_archives' / (row['name'] + '.zip')
        data = archive.read_bytes()
        assert len(data) == row['bytes'] == actual[row['id']]['size_in_bytes']
        assert sha(data) == row['sha256'] and actual[row['id']]['digest'] == 'sha256:' + sha(data)
        assert actual[row['id']]['workflow_run']['id'] == RUN
        assert actual[row['id']]['workflow_run']['head_sha'] == SOURCE
        members = {}
        with zipfile.ZipFile(archive) as stream:
            assert stream.testzip() is None
            for member in stream.infolist():
                if member.is_dir():
                    continue
                path = PurePosixPath(member.filename)
                assert not path.is_absolute() and '..' not in path.parts and '\\' not in member.filename
                assert member.filename not in members and (member.external_attr >> 16) & 0xf000 != 0xa000
                raw = stream.read(member)
                assert raw == (OUTPUT / row['name'] / member.filename).read_bytes()
                members[member.filename] = raw
        assert len(members) == row['original_members']
        count += len(members)
        originals[row['name']] = members
    assert len(originals) == 5 and count == 1721

    # Build the source rows from raw Git objects rather than any CI summary.
    entries, gitlinks = [], []
    raw_tree = subprocess.check_output(['git', '-C', str(REPO), 'ls-tree', '-rz', '--full-tree', SOURCE])
    for line in raw_tree.split(b'\0'):
        if not line:
            continue
        metadata, name = line.split(b'\t', 1)
        mode, kind, blob = metadata.decode().split()
        if kind == 'commit':
            assert mode == '160000'
            gitlinks.append(blob)
        else:
            assert kind == 'blob' and mode in ['100644', '100755']
            entries.append((name.decode(), mode, blob))
    assert len(entries) == 881 and len(gitlinks) == 5
    batch = subprocess.check_output(['git', '-C', str(REPO), 'cat-file', '--batch'],
        input=''.join(blob + '\n' for _, _, blob in entries).encode())
    objects = io.BytesIO(batch)
    source_rows = []
    for path, mode, blob in entries:
        actual_blob, kind, length = objects.readline().decode().split()
        data = objects.read(int(length))
        assert actual_blob == blob and kind == 'blob' and objects.read(1) == b'\n'
        assert hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == blob
        source_rows.append(dict(path=path, mode=mode, git_blob=blob, bytes=len(data), sha256=sha(data)))
    assert not objects.read()
    expected = dict(source_commit=SOURCE, run_id=str(RUN), method='raw-git-cat-file-batch',
        tracked_files=881, sources=source_rows)
    machines = {'arm64': 0xaa64, 'x64': 0x8664, 'x86': 0x14c}
    cores = {}
    for row in review['architectures']:
        arch = row['architecture']
        members = originals['dxvk-umd-backend-' + arch + '-' + SOURCE]
        assert json.loads(members['native-canonical-source.json'].decode('utf-8-sig')) == expected
        for binary in row['original_outputs']:
            data = members[binary['path']]
            assert len(data) == binary['bytes'] and sha(data) == binary['sha256']
            offset = struct.unpack_from('<I', data, 0x3c)[0]
            assert data[:2] == b'MZ' and data[offset:offset + 4] == b'PE\0\0'
            assert struct.unpack_from('<H', data, offset + 4)[0] == machines[arch]
        cores[arch] = dict(bytes=len(members['viogpudxvk.dll']), sha256=sha(members['viogpudxvk.dll']))
        assert sha(members['owned-raw-process-original.cs.txt']) == RAW
        for receipt in row['bounded_processes']:
            assert json.loads(members[receipt['name'] + '.process.json'].decode('utf-8-sig')) == receipt
            check_process(receipt, members)
        assert len(row['bounded_processes']) == (0 if arch == 'arm64' else 30)
        for key, filename in [('debug_directory_sha256', 'root-original-core-debug-directory.txt'),
                              ('pdb_summary_sha256', 'root-original-pdb-summary.txt')]:
            assert sha((OUTPUT / ('dxvk-umd-backend-' + arch + '-' + SOURCE) / filename).read_bytes()) == row['dll_pdb_identity'][key]
    arm = originals['dxvk-native-runtime-arm64-validation-' + SOURCE]
    assert len(arm) == 511 and json.loads(arm['arm64-native-canonical-source.json'].decode('utf-8-sig')) == expected
    assert len(review['original_arm64_executions']) == 30
    for row in review['original_arm64_executions']:
        receipt = json.loads(arm[row['process_member']].decode('utf-8-sig'))
        assert receipt == row['owned_process_receipt'] and sha(arm[row['process_member']]) == row['process_sha256']
        check_process(receipt, arm)
        for stream in ['stdout', 'stderr']:
            assert sha(arm[receipt[stream + '_member']]) == row[stream + '_sha256']
    for arch, volume in review['volume_original_reviews'].items():
        assert volume['passed'] and volume['original_files'] == 309
        assert volume['independently_calculated_voxels'] == 9138
        assert volume['independently_calculated_sampled_pixels'] == 945
        members = arm if arch == 'arm64' else originals['dxvk-umd-backend-' + arch + '-' + SOURCE]
        assert sum(PurePosixPath(name).name.startswith('public-volume-') for name in members) == 108
    assert len(review['original_job_logs']) == 6
    for row in review['original_job_logs']:
        assert sha((OUTPUT / ('job-' + str(row['id']) + '.log')).read_bytes()) == row['sha256']
    owned = read(OUTPUT.parent / 'ci-original-collector-3d39760-01/execution-originals/collect-original-CI3d-01.original-command.json')
    assert owned['exit_code'] == 0 and not owned['timed_out'] and owned['local_owned_transport_pid'] > 0
    for stream in ['stdout', 'stderr']:
        assert pin(Path(owned[stream]['path'])) == owned[stream]
    result = dict(verified=True, source_commit=SOURCE, ci_run=RUN, original_archives=5,
        original_ZIP_files=count, raw_Git_root_blobs=881, gitlinks=5, canonical_receipts=4,
        owned_fixture_children=90, bounded_ARM64=30, bounded_x64_x86=60,
        volume_production_files_per_arch=309, volume_public_files_per_arch=108, cores=cores,
        original_collector_review=pin(OUTPUT / 'root-current-native-ci-verified-01.json'),
        actual_host_collector_exit=0, candidate_preparation_authorized=True,
        target_hardware_acceptance=False, ordinary_runtime_admission=False)
    with (OUTPUT / 'root-CI3d-originals-direct-review-01.json').open('x') as file:
        file.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
