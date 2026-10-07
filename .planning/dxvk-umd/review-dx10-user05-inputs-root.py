#!/usr/bin/env python3
"""Join the frozen DX10 USER05 originals before sealing the runtime packet."""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile

WORKSPACE = Path('/home/sunf/droidvm-repos')
BASE = WORKSPACE / 'reference/codes/dxvk-umd-dx10-kmt-probe-20261007/artifacts/dx10-kmt-user-gate-20261007/prepared-user05-dual-profile-d7e5c7d-01'

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def sha(data):
    return hashlib.sha256(data).hexdigest()

def pin(row):
    path = Path(row['path'])
    data = path.read_bytes()
    assert len(data) == row['bytes'] and sha(data) == row['sha256'], path
    return path

def archive_original(path, name):
    name = name.removeprefix('./')
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            names = [n for n in archive.namelist() if n == name or n.endswith('/' + name)]
            assert len(names) == 1, (path, name, names)
            return archive.read(names[0])
    with tarfile.open(path) as archive:
        members = [m for m in archive.getmembers() if m.isfile() and (m.name.removeprefix('./') == name or m.name.endswith('/' + name))]
        assert len(members) == 1, (path, name, len(members))
        return archive.extractfile(members[0]).read()

def main():
    manifest = read(BASE / 'manifest-role71-d7e5c7d.pending.json')
    request = read(BASE / 'root-input-review-71-joins.request.json')
    transfers = read(BASE / 'transfer-map-71-actual-inputs.json')
    assert manifest['ready'] is False and manifest['hardware_authorized'] is False
    assert manifest['profiles'] == ['10_0', '10_1']
    known = {r['role']: r for r in manifest['files'] if r['role'] != 'root-input-review'}
    assert len(known) == len(transfers) == 71
    originals = {}
    joins = []
    for transfer in transfers:
        role = transfer['role']
        row = known[role]
        original, staged = (pin(transfer[k]) for k in ('original', 'staged'))
        assert original.read_bytes() == staged.read_bytes()
        assert staged == BASE / 'stage/payload' / row['destination']
        assert transfer['windows_path'] == row['path'] and transfer['destination'] == row['destination']
        assert transfer['original']['bytes'] == row['bytes'] and transfer['original']['sha256'] == row['sha256']
        originals[role] = original
        joins.append(dict(role=role, bytes=original.stat().st_size, sha256=sha(original.read_bytes())))
    assert joins == request['file_joins']
    archive_joins = 0
    for role, row in known.items():
        if row.get('archive_role'):
            assert archive_original(originals[row['archive_role']], row['archive_member']) == originals[role].read_bytes(), role
            archive_joins += 1
    sources = manifest['probe_source_inputs'] + manifest['core_header_joins']
    assert len(sources) == 25
    for row in sources:
        data = subprocess.run(['git', '-C', str(WORKSPACE / 'dxvk-umd-ci'), 'show', row['source_commit'] + ':' + row['git_path']], check=True, capture_output=True).stdout
        assert data == originals[row['file_role']].read_bytes() and sha(data) == row['git_sha256']
    prefix = read(pin(request['native_prefix_original']))
    accepted = read(pin(request['native_prefix_root_review']))
    assert accepted['verified'] and accepted['AST_helpers'] == 8 and accepted['AST_errors'] == 0
    assert prefix['passed'] and prefix['raw_add_type'] and prefix['pending_inputs_rejected'] and not prefix['core_loaded'] and not prefix['gpu_execution']
    ps = {r['name']: r for r in prefix['scripts']}
    assert len(ps) == 8 and all(r['errors'] == 0 for r in ps.values())
    for helper in manifest['helpers']:
        data = (BASE / 'stage/helpers' / helper['name']).read_bytes()
        assert len(data) == helper['bytes'] and sha(data) == helper['sha256']
        if helper['name'].endswith('.ps1'):
            assert ps[helper['name']]['sha256'] == helper['sha256']
    assert len(manifest['helpers']) == 9
    assert prefix['service_state'] == 'Running' and prefix['failure'] is None
    entries = read(BASE / 'stage-original-files-80.json')['files']
    stage = BASE / 'user05-role71-stage-d7e5c7d-01.tar.gz'
    with tarfile.open(stage) as archive:
        members = {m.name.removeprefix('./'): m for m in archive.getmembers() if m.isfile()}
        assert len(members) == len(entries) == 80
        for row in entries:
            data = archive.extractfile(members[row['path']]).read()
            assert len(data) == row['bytes'] and sha(data) == row['sha256']
            assert data == (BASE / 'stage' / row['path']).read_bytes()
    assert request['core_source_commit'] == manifest['core_source_commit'] == 'd7e5c7d46b8ce889e993bfab66a3b78b076c49d1'
    assert request['ci_run'] == manifest['ci']['run_id'] == 37648387721
    assert request['core_sha256'] == known['core']['sha256'] == 'b0fcbc3afe74a22ced7c4b35231b8064d6acc6b83f5d3b50671b408f2129f347'
    assert request['probe_sha256'] == known['probe']['sha256'] == '907475ede5bbc5a79cd17585c0f301c38c8fc52581af0b5e7600cec53dc03bce'
    assert request['adapter_luid'] == manifest['adapter_luid'] == 'ec6b000000000000'
    proof = dict(request)
    proof.update(verified=True, passed=True, template_not_acceptance=False, file_joins=joins,
                 raw_Git_inputs=25, original_archive_member_joins=archive_joins,
                 independently_joined_stage_files=80, unchanged_helpers=9,
                 native_helpers_originals_accepted=True, target_calls=False,
                 ordinary_runtime_admission=False, hardware_execution=False,
                 hardware_authorization=False)
    destination = BASE / 'root-input-review-71-joins.actual-01.json'
    with destination.open('x') as stream:
        json.dump(proof, stream, indent=2); stream.write('\n')
    print(json.dumps(dict(passed=True, original_roles=len(joins), raw_Git_inputs=25,
                          original_archive_member_joins=archive_joins, stage_files=80,
                          output=str(destination), sha256=sha(destination.read_bytes()))))

if __name__ == '__main__':
    main()
