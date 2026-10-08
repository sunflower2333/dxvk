#!/usr/bin/env python3
"""Join original CI90 ZIPs, Git inputs, native children and independent pixels."""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path, PureWindowsPath
import re
import struct
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True
ROOT = Path('/home/sunf/droidvm-repos')
REPO = ROOT / 'dxvk-umd-ci'
SOURCE = '90fb093ce2a85af12de9028076517f1045c36e61'
RUN = 37709079286
OUT = ROOT / 'artifacts/dxvk-trunk-integration-20261008/successful-original-ci-90fb093-01'
RAW = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=sha(data))


def git(path):
    return subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':' + path])


def main():
    assert os.environ['PWF_PLAN_ROOT'] == str(REPO)
    output = OUT / 'root-CI90-originals-direct-review-01.json'
    assert not output.exists()
    collection = read(OUT / 'original-CI-collection-01.json')
    assert collection['collected'] and collection['source_commit'] == SOURCE and collection['ci_run'] == RUN
    api = {row['id']: row for row in read(OUT / 'ci-artifacts.api.json')['artifacts']}
    folders = {}
    count = 0
    for row in collection['original_archives']:
        path = Path(row['original_archive']['path'])
        assert pin(path) == row['original_archive']
        assert api[row['api_id']]['digest'] == 'sha256:' + pin(path)['sha256']
        assert api[row['api_id']]['size_in_bytes'] == path.stat().st_size
        folder = OUT / row['name']
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            members = [entry for entry in archive.infolist() if not entry.is_dir()]
            assert len(members) == row['original_members'] == len({entry.filename for entry in members})
            for member in members:
                assert archive.read(member) == (folder / member.filename).read_bytes()
        folders[row['name']] = folder
        count += len(members)
    assert len(folders) == 5 and count == 9368
    for receipt_path in OUT.glob('*.process-original.json'):
        receipt = read(receipt_path)
        assert receipt['completed'] and receipt['exit_code'] == 0 and not receipt['timed_out']
        for stream in ['stdout', 'stderr']:
            assert pin(Path(receipt[stream]['path'])) == receipt[stream]
        try:
            os.kill(receipt['pid'], 0)
        except ProcessLookupError:
            pass
        else:
            raise AssertionError('Collector child is still present')
    assert len(collection['original_job_logs']) == 6
    for row in collection['original_job_logs']:
        assert pin(Path(row['original_log']['path'])) == row['original_log']

    tree = subprocess.check_output(['git', '-C', str(REPO), 'ls-tree', '-rz', '--full-tree', SOURCE])
    blobs, links = [], []
    for line in tree.split(b'\0'):
        if not line:
            continue
        metadata, name = line.split(b'\t', 1)
        mode, kind, identity = metadata.decode().split()
        if kind == 'commit':
            links.append(identity)
        else:
            assert kind == 'blob' and mode in ['100644', '100755']
            blobs.append((name.decode(), mode, identity))
    assert len(blobs) == 966 and len(links) == 5
    batch = subprocess.check_output(['git', '-C', str(REPO), 'cat-file', '--batch'],
        input=''.join(blob + '\n' for _, _, blob in blobs).encode())
    stream = io.BytesIO(batch)
    sources = []
    for path, mode, blob in blobs:
        identity, kind, size = stream.readline().decode().split()
        data = stream.read(int(size))
        assert identity == blob and kind == 'blob' and stream.read(1) == b'\n'
        assert hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == blob
        sources.append(dict(path=path, mode=mode, git_blob=blob, bytes=len(data), sha256=sha(data)))
    assert not stream.read()
    canonical = dict(source_commit=SOURCE, run_id=str(RUN), method='raw-git-cat-file-batch', tracked_files=966, sources=sources)
    source = git('scripts/test-native-arm64.ps1').decode()
    patterns = dict(re.findall(r"(?m)^\s*'(dxvk-umd-[a-z0-9-]+\.exe)'\s*=\s*'([^']*)'\s*$",
        source.split('$cases = [ordered]@{', 1)[1].split('\n}', 1)[0]))
    assert len(patterns) == 40
    build = git('scripts/build-native-umd.ps1').decode()
    fixed = re.findall(r'(?m)^\s*Invoke-BoundedFixture (.*?) ([a-zA-Z0-9-]+)(?: (\$textureOriginals))?\r?$', build)
    cube_loop = re.search(r"foreach \(\$fixture in @\(([^\n]*)\)\)", build).group(1)
    backend_names = {'dxvk-umd-' + name + '.exe' for _, name, _ in fixed}
    backend_names |= {'dxvk-umd-' + name + '-test.exe' for name in re.findall(r"'([^']*)'", cube_loop)}
    assert len(fixed) == 35 and len(backend_names) == 40
    backend_patterns = {name: pattern for name, pattern in patterns.items() if name in backend_names}
    backend_patterns['dxvk-umd-adapter-test.exe'] = r'adapter lifecycle PASS checks=94208; mock runtime and backend, no GPU'
    backend_patterns['dxvk-umd-compute-oracle-test.exe'] = r'typed DX11 compute oracle PASS checks=12482 elements=96 words=384; CPU control only'
    assert set(backend_patterns) == backend_names
    cores, children = {}, {}
    arm = folders['dxvk-native-runtime-arm64-validation-' + SOURCE]
    assert read(arm / 'arm64-native-canonical-source.json') == canonical
    assert sha((arm / 'arm64-owned-raw-process-original.cs.txt').read_bytes()) == RAW
    for arch, machine in [('arm64', 0xaa64), ('x64', 0x8664), ('x86', 0x14c)]:
        folder = folders['dxvk-umd-backend-' + arch + '-' + SOURCE]
        config = read(folder / 'native-build-configuration.json')
        assert config['source_commit'] == SOURCE and config['arch'] == arch
        assert config['github']['sha'] == SOURCE and int(config['github']['run_id']) == RUN
        assert config['all_configuration_sources_match_git'] and config['loader_policy'] == 'module-local-private-no-fallback'
        assert read(folder / 'native-canonical-source.json') == canonical
        assert sha((folder / 'owned-raw-process-original.cs.txt').read_bytes()) == RAW
        for row in config['configuration_files'] + [config['dll'], config['pdb']]:
            data = (folder / row['member']).read_bytes()
            assert len(data) == row['bytes'] and sha(data) == row['sha256']
        before = {row['path']: row for row in config['configuration_source_before']}
        after = {row['path']: row for row in config['configuration_source_after']}
        assert before.keys() == after.keys()
        for path, row in before.items():
            data = git(path)
            assert len(data) == row['bytes'] == after[path]['bytes']
            assert sha(data) == row['sha256'] == after[path]['sha256'] and after[path]['matches_before']
        private = 'viogpu_gl_loader_' + arch + '.dll'
        assert config['vulkan_loader'] == private and private.encode('utf-16-le') in (folder / 'viogpudxvk.dll').read_bytes()
        for binary in list(folder.glob('*.exe')) + [folder / 'viogpudxvk.dll']:
            data = binary.read_bytes(); offset = struct.unpack_from('<I', data, 0x3c)[0]
            assert data[:2] == b'MZ' and data[offset:offset+4] == b'PE\0\0' and struct.unpack_from('<H', data, offset+4)[0] == machine
        cores[arch] = pin(folder / 'viogpudxvk.dll')
        owner = arm if arch == 'arm64' else folder
        receipts = list(owner.glob('*.process.json'))
        assert len(receipts) == 40
        expected_patterns = patterns if arch == 'arm64' else backend_patterns
        seen = set()
        for receipt_path in receipts:
            row = read(receipt_path)
            name = PureWindowsPath(row['executable']).name
            assert name in expected_patterns and name not in seen
            seen.add(name)
            assert row['pid'] > 0 and row['retained_process_handle'] > 0 and row['runner_sha256'] == RAW
            assert row['exit_code'] == row['expected_exit'] == 0 and row['deadline_ms'] == 30000
            assert row['exited'] and row['exit_code_available'] and row['pipes_drained']
            assert not row['timed_out'] and not row['child_still_running'] and not row['capture_failure']
            for label in ['stdout', 'stderr']:
                raw = (owner / row[label + '_member']).read_bytes()
                assert len(raw) == row[label + '_bytes']
            text = (owner / row['stdout_member']).read_text(encoding='utf-8-sig')
            assert re.search(expected_patterns[name], text, re.I), name
        assert seen == set(expected_patterns)
        children[arch] = len(receipts)

    readers = OUT / 'root-frozen-source-readers'
    readers.mkdir()
    specs = [
        ('scripts/verify-native-cube-originals.py', 'texturecube', False),
        ('scripts/verify-native-cube-public-mips-originals.py', 'texturecube', True),
        ('scripts/verify-native-cube-array-resource-originals.py', 'cube-array-resource', True),
        ('tests/verify-cube-srv-mips-originals.py', 'cube-srv-mips', True),
        ('scripts/verify-native-cube-array-mips-originals.py', 'cube-array-mips', False),
        ('tests/verify-cube-array-target-originals.py', 'cube-array-targets', True),
    ]
    reviews = []
    for arch in ['arm64', 'x64', 'x86']:
        folder = arm if arch == 'arm64' else folders['dxvk-umd-backend-' + arch + '-' + SOURCE]
        for path, fixture, wants_stdout in specs:
            script = readers / Path(path).name
            original = git(path)
            if script.exists():
                assert script.read_bytes() == original
            else:
                script.write_bytes(original)
            directory = folder / (('arm64-' if arch == 'arm64' else '') + fixture + '-originals')
            stdout = folder / (('arm64-dxvk-umd-' + fixture + '-test.exe.stdout.txt') if arch == 'arm64' else fixture + '-test.txt')
            proof = OUT / ('root-' + arch + '-' + script.stem + '-verified.json')
            args = ['--originals', str(directory)] if fixture == 'cube-array-targets' else [str(directory)]
            if wants_stdout:
                args += ['--stdout', str(stdout)]
            if fixture != 'cube-srv-mips':
                args += ['--output', str(proof)]
            command = [sys.executable, '-B', str(script)] + args
            result = subprocess.run(command, capture_output=True, timeout=40)
            prefix = OUT / ('root-' + arch + '-' + script.stem)
            Path(str(prefix) + '.stdout.raw').write_bytes(result.stdout)
            Path(str(prefix) + '.stderr.raw').write_bytes(result.stderr)
            assert result.returncode == 0, (arch, fixture, result.stderr.decode())
            if fixture == 'cube-srv-mips':
                proof.write_bytes(result.stdout)
            reviews.append(dict(architecture=arch, source=pin(script), argv=command, exit_code=0, original_review=pin(proof)))
    volume_path = ROOT / 'reference/codes/dxvk-umd-so-volume-integration-20261008/.planning/native-so-volume-integration-20261008/verify-volume-original-words-01.py'
    assert pin(volume_path)['sha256'] == 'b0c463995f012c5a9c5cf78cf52b233a8641a8170998fc65f2265f08cca5fb4d'
    spec = importlib.util.spec_from_file_location('ci90_independent_volume', volume_path)
    volume = importlib.util.module_from_spec(spec); spec.loader.exec_module(volume)
    volumes = {}
    for arch in ['arm64', 'x64', 'x86']:
        folder = arm if arch == 'arm64' else folders['dxvk-umd-backend-' + arch + '-' + SOURCE]
        directory = folder / ('arm64-texture3d-originals' if arch == 'arm64' else 'texture3d-originals')
        volumes[arch] = volume.verify(directory)
    result = dict(verified=True, source_commit=SOURCE, ci_run=RUN, original_archives=5,
        original_ZIP_files=count, raw_Git_root_blobs=966, gitlinks=5, canonical_receipts=4,
        closed_original_fixture_children=children, independent_cube_reviews=reviews,
        independent_volume_reviews=volumes, cores=cores, collection=pin(OUT / 'original-CI-collection-01.json'),
        original_reviewer=pin(Path(__file__)), native_reference_admitted=True,
        target_hardware_acceptance=False, ordinary_runtime_admission=False,
        historical_whole_compiler_attestation=False)
    with output.open('x') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    print(json.dumps(dict(verified=True, original_ZIP_files=count, native_fixtures=120,
        cube_reference_reviews=18, volume_reference_reviews=3, proof=pin(output))))


if __name__ == '__main__':
    main()
