#!/usr/bin/env python3
"""Direct original joins for the complete release-library cube follow-on."""
import ast
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile

W = Path('/home/sunf/droidvm-repos')
B = W / 'artifacts/native-cube-integration-20261008'
P = B / 'prepared-native-8528d91-03'
OLD = B / 'prepared-native-8528d91-02'
C = '8528d91357255fe8f31138d5438e7313e2367fec'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(row):
    data = Path(row['path']).read_bytes()
    assert sha(data) == row['sha256']
    if 'bytes' in row:
        assert len(data) == row['bytes']
    return data


def main():
    descriptor = P / 'prepared-native-cube-integration-8528d91-03.json'
    prepared = load(descriptor)
    assert sha(descriptor.read_bytes()) == 'e2d0e52df3d322c6a4b3af65f4a0172741d8dab6b0705d9333f4342e44bf95d6'
    assert prepared['source_commit'] == C and prepared['source_inputs'] == 160 and not prepared['ready']
    for key in ('hardware_acceptance', 'ordinary_runtime_admission', 'driver_installation', 'target_calls'):
        assert not prepared[key]
    for row in prepared['local_inputs'] + prepared['target_payload']:
        pin(row)
    for row in prepared['inherited_inputs']:
        assert pin(row['source']) == pin(row['copy'])
    archive = pin(prepared['source_archive'])
    assert archive == (OLD / 'cube-integration-source-8528d91-01.tar.gz').read_bytes()
    manifest = json.loads(pin(prepared['source_receipt']).decode('utf-8-sig'))
    assert manifest == load(OLD / 'cube-integration-source-8528d91-01.json')
    assert manifest['source_commit'] == C and manifest['input_count'] == 160
    with tarfile.open(prepared['source_archive']['path']) as tar:
        members = tar.getmembers()
        assert len(members) == 160 and all(m.isfile() for m in members)
        original = {m.name: tar.extractfile(m).read() for m in members}
    assert set(original) == {row['path'] for row in manifest['sources']}
    for row in manifest['sources']:
        data = original[row['path']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        repo = W / 'dxvk-umd-ci'
        if row['submodule']:
            repo /= row['submodule']
        assert data == subprocess.check_output(['git', '-C', str(repo), 'show', row['git_commit'] + ':' + row['git_path']])
    delta = load(P / 'local-library-followon-delta-verified-03.json')
    for row in delta['untouched_copies']:
        assert pin(row['original']) == pin(row['copy'])
    changes = delta['changed_helpers']
    renames = [(Path(v['copy']['path']).name, Path(v['original']['path']).name) for v in changes]
    hashes = [(v['copy']['sha256'], v['original']['sha256']) for v in changes]
    hashes += [(sha((P / 'native-cube-integration-libraries-03.json').read_bytes()),
                sha((OLD / 'native-cube-integration-libraries-02.json').read_bytes()))]
    normalized = 0
    for row in changes:
        old, current = pin(row['original']).decode(), pin(row['copy']).decode()
        if not row['copy']['path'].endswith('.ps1'):
            ast.parse(current)
            continue
        for new, previous in renames + hashes:
            current = current.replace(new, previous)
        current = current.replace('native-cube-integration-libraries-03.json', 'native-cube-integration-libraries-02.json')
        current = current.replace('8528d91-03', '8528d91-02')
        current = current.replace('$result.library_inputs.Count -eq 11', '$result.library_inputs.Count -eq 8')
        assert current == old, row['copy']['path']
        normalized += 1
    assert normalized == 6
    driver = P / 'deferred-native-cube-integration-8528d91-04.py'
    driver_text = driver.read_text()
    for new, previous in renames:
        driver_text = driver_text.replace(new, previous)
    driver_text = driver_text.replace('8528d91-03', '8528d91-02')
    assert driver_text == (OLD / 'deferred-native-cube-integration-8528d91-03.py').read_text()
    libraries = load(P / 'native-cube-integration-libraries-03.json')['libraries']
    previous = load(OLD / 'native-cube-integration-libraries-02.json')['libraries']
    assert len(libraries) == 11 and libraries[:7] == previous[:7]
    relocated_uuid = dict(libraries[7])
    relocated_uuid['guest_path'] = relocated_uuid['guest_path'].replace('8528d91-03', '8528d91-02')
    assert relocated_uuid == previous[7]
    assert {row['name'] for row in libraries[8:]} == {'advapi32.lib', 'runtimeobject.lib', 'synchronization.lib'}
    packages = {}
    count = 0
    try:
        for name, key, expected in [('native-cube-msvc-tool-inputs-01.json', 'tools', 369),
                                    ('native-warp-sdk-headers-f4bf37f-01.json', 'headers', 21),
                                    ('native-cube-integration-libraries-03.json', 'libraries', 11)]:
            rows = load(P / name)[key]
            assert len(rows) == expected
            for row in rows:
                package = row['source_package']
                path = Path(package['path'])
                if path not in packages:
                    pin(package)
                    packages[path] = zipfile.ZipFile(path)
                data = packages[path].read(row['member'])
                assert len(data) == row['bytes'] and sha(data) == row['sha256']
                count += 1
        for row in prepared['new_original_libraries']:
            assert pin(row['copy']) == pin(row['original_payload'])
            assert packages[Path(row['row']['source_package']['path'])].read(row['row']['member']) == pin(row['copy'])
    finally:
        for package in packages.values():
            package.close()
    assert count == 401 and len(prepared['target_payload']) == 17
    assert pin(prepared['independent_volume_reader']['original']) == pin(prepared['independent_volume_reader']['copy'])
    for row in prepared['independent_cube_readers']:
        ast.parse(pin(row))
    closure = json.loads(pin(prepared['causal_original_review']).decode('utf-8-sig'))
    assert closure['verified'] and not closure['missing_default_libraries'] and closure['no_debug_library_added']
    assert closure['explicit_NODEFAULTLIB'] == ['OLDNAMES.lib'] or closure['explicit_NODEFAULTLIB'] == ['oldnames.lib']
    assert not closure['actual_native_link_success'] and not closure['original_library_archives_transformed']
    assert {v.lower() for v in closure['effective_release_defaults']} <= {v['name'].lower() for v in libraries}
    pin(prepared['root_previous_failure_review'])
    pin(prepared['prior_failed_attempt'])
    proof = dict(verified=True, source_commit=C, prepared_sha256=sha(descriptor.read_bytes()),
                 source_archive_unchanged=True, source_Git_inputs=160, original_package_members=401,
                 compiler_files=369, SDK_headers=21, release_libraries=11, target_payloads=17,
                 old_eight_library_content_unchanged=True, six_native_helpers_normalize_to02=True,
                 host_driver_normalizes_to02=True, strict_source_flags_oracles_unchanged=True,
                 expected_COFF=23, expected_PE=8, native_helper_AST_pending=6, native_CI_AST_pending=2,
                 expected_cube_originals=2086, actual_native_link_pending=True,
                 hardware_acceptance=False, ordinary_runtime_admission=False, target_calls=False)
    output = P / 'root-prepared-native-cube-direct-review-03.json'
    with output.open('x') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(path=str(output), bytes=output.stat().st_size, sha256=sha(output.read_bytes()))))


if __name__ == '__main__':
    main()
