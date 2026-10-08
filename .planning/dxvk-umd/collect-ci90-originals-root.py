#!/usr/bin/env python3
"""Collect immutable GitHub CI90 archives and logs; no target-device access."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import time
import zipfile

ROOT = Path('/home/sunf/droidvm-repos')
REPO = ROOT / 'dxvk-umd-ci'
SOURCE = '90fb093ce2a85af12de9028076517f1045c36e61'
RUN = 37709079286
OUT = ROOT / 'artifacts/dxvk-trunk-integration-20261008/successful-original-ci-90fb093-01'


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def command(name, argv, output):
    start = time.monotonic()
    error = OUT / (name + '.stderr.raw')
    with output.open('xb') as stdout, error.open('xb') as stderr:
        process = subprocess.Popen(argv, stdout=stdout, stderr=stderr)
        timeout = False
        try:
            process.wait(timeout=450)
        except subprocess.TimeoutExpired:
            timeout = True
            process.kill()
            process.wait(timeout=10)
    row = dict(argv=argv, pid=process.pid, exit_code=process.returncode,
        completed=True, timed_out=timeout, seconds=time.monotonic()-start,
        stdout=pin(output), stderr=pin(error))
    (OUT / (name + '.process-original.json')).write_text(json.dumps(row, indent=2) + '\n')
    assert not timeout and process.returncode == 0, name
    return row


def main():
    assert os.environ['PWF_PLAN_ROOT'] == str(REPO) and not OUT.exists()
    OUT.mkdir(parents=True)
    (OUT / '_archives').mkdir()
    (OUT / 'executed-collector-original.py').write_bytes(Path(__file__).read_bytes())
    api = f'repos/sunflower2333/dxvk/actions/runs/{RUN}'
    for name, suffix in [('ci-run', ''), ('ci-jobs', '/jobs?per_page=100'), ('ci-artifacts', '/artifacts?per_page=100')]:
        command(name, ['gh', 'api', api + suffix], OUT / (name + '.api.json'))
    read = lambda path: json.loads(path.read_text(encoding='utf-8-sig'))
    run = read(OUT / 'ci-run.api.json')
    jobs = read(OUT / 'ci-jobs.api.json')['jobs']
    artifacts = read(OUT / 'ci-artifacts.api.json')['artifacts']
    assert run['head_sha'] == SOURCE and run['id'] == RUN and run['conclusion'] == 'success' and run['status'] == 'completed'
    assert len(jobs) == 6 and all(row['run_id'] == RUN and row['head_sha'] == SOURCE and row['conclusion'] == 'success' and row['status'] == 'completed' for row in jobs)
    assert {row['name'] for row in jobs} == {'identity', 'shader-cpu', 'backend (arm64)', 'backend (x64)', 'backend (x86)', 'arm64-runtime'}
    names = {'dxvk-native-contracts-' + SOURCE, 'dxvk-native-runtime-arm64-validation-' + SOURCE}
    names |= {'dxvk-umd-backend-' + arch + '-' + SOURCE for arch in ['arm64', 'x64', 'x86']}
    assert len(artifacts) == 5 and {row['name'] for row in artifacts} == names

    def archive(row):
        assert not row['expired'] and row['workflow_run']['id'] == RUN and row['workflow_run']['head_sha'] == SOURCE
        path = OUT / '_archives' / (row['name'] + '.zip')
        receipt = command('archive-' + str(row['id']), ['gh', 'api', f"repos/sunflower2333/dxvk/actions/artifacts/{row['id']}/zip"], path)
        identity = pin(path)
        assert identity['bytes'] == row['size_in_bytes'] and 'sha256:' + identity['sha256'] == row['digest']
        folder = OUT / row['name']
        folder.mkdir()
        with zipfile.ZipFile(path) as stream:
            assert stream.testzip() is None
            files = [member for member in stream.infolist() if not member.is_dir()]
            assert len(files) == len({member.filename for member in files})
            for member in files:
                relative = PurePosixPath(member.filename)
                assert not relative.is_absolute() and '..' not in relative.parts and '\\' not in member.filename
                assert (member.external_attr >> 16) & 0xf000 != 0xa000
                target = folder / member.filename
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(stream.read(member))
        return dict(api_id=row['id'], name=row['name'], original_archive=identity, original_members=len(files), host_receipt=receipt)

    with ThreadPoolExecutor(max_workers=3) as pool:
        archives = list(pool.map(archive, artifacts))

    def log(row):
        path = OUT / ('job-' + str(row['id']) + '.log')
        receipt = command('job-' + str(row['id']), ['gh', 'api', f"repos/sunflower2333/dxvk/actions/jobs/{row['id']}/logs"], path)
        return dict(id=row['id'], name=row['name'], original_log=pin(path), host_receipt=receipt)

    with ThreadPoolExecutor(max_workers=3) as pool:
        logs = list(pool.map(log, jobs))
    proof = dict(collected=True, source_commit=SOURCE, ci_run=RUN, original_archives=archives,
        original_job_logs=logs, target_calls=0, runtime_or_hardware_admission=False,
        independent_raw_review='pending', source=pin(Path(__file__)))
    path = OUT / 'original-CI-collection-01.json'
    with path.open('x') as stream:
        json.dump(proof, stream, indent=2); stream.write('\n')
    print(json.dumps(dict(collected=True, original_archives=5, original_job_logs=6,
        original_members=sum(row['original_members'] for row in archives), proof=pin(path))))


if __name__ == '__main__':
    main()
