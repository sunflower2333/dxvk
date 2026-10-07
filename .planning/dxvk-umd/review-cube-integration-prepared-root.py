#!/usr/bin/env python3
"""Rejoin the frozen combined cube source and original Microsoft packages."""
import ast
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile

WORKSPACE = Path('/home/sunf/droidvm-repos')
BASE = WORKSPACE / 'artifacts/native-cube-integration-20261008/prepared-native-8528d91-01'
SOURCE = '8528d91357255fe8f31138d5438e7313e2367fec'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(row):
    path = Path(row['path'])
    data = path.read_bytes()
    assert sha(data) == row['sha256']
    if 'bytes' in row:
        assert len(data) == row['bytes']
    return data


def main():
    descriptor = BASE / 'prepared-native-cube-integration-8528d91-01.json'
    prepared = read(descriptor)
    assert prepared['source_commit'] == SOURCE and not prepared['ready']
    assert prepared['source_inputs'] == 160
    for key in ('hardware_acceptance', 'ordinary_runtime_admission', 'driver_installation', 'target_calls'):
        assert not prepared[key]
    for row in prepared['local_inputs'] + prepared['target_payload']:
        pin(row)
    for row in prepared['inherited_inputs']:
        assert pin(row['source']) == pin(row['copy'])
    assert pin(prepared['independent_volume_reader']['original']) == pin(prepared['independent_volume_reader']['copy'])
    assert prepared['independent_volume_reader']['copy']['sha256'] == 'b0c463995f012c5a9c5cf78cf52b233a8641a8170998fc65f2265f08cca5fb4d'
    pin(prepared['old_packet_preserved'])
    receipt = read(Path(prepared['source_receipt']['path']))
    assert receipt['source_commit'] == SOURCE and receipt['input_count'] == 160
    assert receipt['git_object_inputs'] and not receipt['worktree_bytes_used']
    archive = Path(prepared['source_archive']['path'])
    pin(prepared['source_archive'])
    assert receipt['archive_sha256'] == sha(archive.read_bytes())
    with tarfile.open(archive) as tar:
        members = [m for m in tar.getmembers() if m.isfile()]
        assert len(members) == 160 and len({m.name for m in members}) == 160
        data = {m.name: tar.extractfile(m).read() for m in members}
    assert set(data) == {r['path'] for r in receipt['sources']}
    dependency_count = 0
    for row in receipt['sources']:
        original = data[row['path']]
        assert len(original) == row['bytes'] and sha(original) == row['sha256']
        repo = WORKSPACE / 'dxvk-umd-ci'
        if row['submodule']:
            repo /= row['submodule']
            dependency_count += 1
        raw = subprocess.run(['git', '-C', str(repo), 'show', row['git_commit'] + ':' + row['git_path']],
                             capture_output=True, check=True).stdout
        assert raw == original
        blob = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
        assert blob == row['git_blob']
    fixtures = receipt['fixtures']
    assert fixtures == prepared['fixtures'] and len(fixtures) == 8
    units = [unit for group in receipt['groups'] for unit in group['units']]
    assert len(units) == len(set(units)) == 15
    assert len(units) + len(fixtures) == receipt['expected_original_coff'] == prepared['expected_original_coff'] == 23
    assert receipt['expected_original_pe'] == prepared['expected_original_pe'] == 8
    assert receipt['expected_owned_build_children'] == prepared['owned_build_children'] == 29
    assert receipt['cube_original_files'] == 2086
    assert set(receipt['ci_ast_scripts']) == {'scripts/build-native-umd.ps1', 'scripts/test-native-arm64.ps1'}
    assert prepared['native_helper_AST_scripts'] == 6 and prepared['native_CI_AST_scripts'] == 2
    readers = prepared['independent_cube_readers']
    assert len(readers) == 4
    for row in readers:
        original = pin(row)
        assert original == next(value for name, value in data.items() if Path(name).name == Path(row['path']).name)
        ast.parse(original)
    driver = BASE / 'deferred-native-cube-integration-8528d91-02.py'
    ast.parse(driver.read_bytes())
    # The six pinned Microsoft packages contain the actual tool/SDK/library bytes.
    packages = {}
    members_checked = 0
    try:
        for filename, rows_key, expected in [('native-cube-msvc-tool-inputs-01.json', 'tools', 369),
                                             ('native-warp-sdk-headers-f4bf37f-01.json', 'headers', 21),
                                             ('native-warp-libraries-f4bf37f-01.json', 'libraries', 7)]:
            provenance = read(BASE / filename)
            rows = provenance[rows_key]
            assert len(rows) == expected
            for row in rows:
                package = row['source_package']
                path = Path(package['path'])
                if path not in packages:
                    pin(package)
                    packages[path] = zipfile.ZipFile(path)
                original = packages[path].read(row['member'])
                assert len(original) == row['bytes'] and sha(original) == row['sha256']
                members_checked += 1
    finally:
        for package in packages.values():
            package.close()
    assert members_checked == 397
    builder = (BASE / 'build-native-cube-integration-01.ps1').read_text()
    ci_start = builder.index("$ciStage=Invoke-Logged 'parse-ci'")
    assert ci_start < builder.index('foreach ($group in $manifest.groups)')
    assert "'/W4','/WX'" in builder and "$compileArgs=@($common|Where-Object" in builder
    assert "'Strict first-party compiler/linker warnings'" in builder
    assert "if (Test-Path -LiteralPath $Root) {throw 'Refusing previous native cube root'}" in builder
    assert prepared['raw_runner_sha256'] == 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
    proof = dict(schema='root-combined-cube-prepared-direct-review-v1', verified=True,
                 source_commit=SOURCE, descriptor_sha256=sha(descriptor.read_bytes()),
                 source_archive_sha256=sha(archive.read_bytes()), source_Git_inputs=160,
                 dependency_Git_inputs=dependency_count, original_package_members=397,
                 native_tools=369, SDK_headers=21, link_libraries=7,
                 common_compile_units=15, fixture_compile_units=8, expected_COFF=23, expected_PE=8,
                 native_helper_AST_pending=6, native_CI_AST_pending=2, expected_owned_stages=29,
                 cube_raw_files_expected=2086, independent_cube_oracles=4,
                 old_volume_oracle_unchanged=True, deferred_driver02_sha256=sha(driver.read_bytes()),
                 hardware_acceptance=False, ordinary_runtime_admission=False, target_calls=False)
    output = BASE / 'root-prepared-native-cube-direct-review-01.json'
    with output.open('x') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps(proof, sort_keys=True))


if __name__ == '__main__':
    main()
