#!/usr/bin/env python3
"""Join the typed cube targets packet to Git and original SDK libraries."""
import ast
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile

W = Path('/home/sunf/droidvm-repos')
P = W / 'artifacts/cube-array-targets-20261008/native-reference-5624e76-01'
C = '5624e76deac810ff1694734e8c1dccdf63121706'
A = '086fe3645e1cb9893ffca2fbbdce6c8b162d4c0f'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def pin(row):
    data = Path(row['path']).read_bytes()
    assert sha(data) == row['sha256']
    if 'bytes' in row:
        assert len(data) == row['bytes']
    return data


def main():
    descriptor = P / 'prepared-native-cube-array-targets-reference-01.json'
    assert sha(descriptor.read_bytes()) == 'e3790326387aa4fe265e80822d81db02bd903285ec450ba9018973f72c050832'
    prepared = load(descriptor)
    assert prepared['source_commit'] == C and prepared['supplemental_source'] == A
    assert not prepared['ready'] and prepared['authorization'] is None
    assert not prepared['hardware_acceptance'] and not prepared['ordinary_runtime_admission']
    for row in prepared['files']:
        pin(row)
    source = json.loads(pin(prepared['source_manifest']).decode('utf-8-sig'))
    assert source['source_commit'] == C and source['input_count'] == 160
    pin(prepared['source_archive'])
    with tarfile.open(prepared['source_archive']['path']) as archive:
        members = archive.getmembers()
        assert len(members) == 160 and all(m.isfile() for m in members)
        assert len({m.name for m in members}) == 160
        original = {m.name: archive.extractfile(m).read() for m in members}
    assert set(original) == {r['path'] for r in source['sources']}
    for row in source['sources']:
        data = original[row['path']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        repo = W / 'dxvk-umd-ci'
        if row['submodule']:
            repo /= row['submodule']
        assert data == subprocess.check_output(['git', '-C', str(repo), 'show', row['git_commit'] + ':' + row['git_path']])
    supplement = json.loads(pin(prepared['supplemental_AST_only']).decode('utf-8-sig'))
    assert len(supplement['inputs']) == 2 and supplement['scope'] == 'AST-only'
    for row in supplement['inputs']:
        assert row['source_commit'] == A
        assert pin(row['file']) == subprocess.check_output([
            'git', '-C', str(W / 'dxvk-umd-ci'), 'show', A + ':' + row['original_git_path']])
        assert pin(row['file']) == (P / row['name']).read_bytes()
    packages = {}
    count = 0
    try:
        for name, key, expected in [('native-cube-msvc-tool-inputs-01.json', 'tools', 369),
                                    ('native-warp-sdk-headers-f4bf37f-01.json', 'headers', 21),
                                    ('native-cube-target-libraries-5624e76-01.json', 'libraries', 12)]:
            rows = load(P / name)[key]
            assert len(rows) == expected
            for row in rows:
                path = Path(row['source_package']['path'])
                if path not in packages:
                    pin(row['source_package'])
                    packages[path] = zipfile.ZipFile(path)
                data = packages[path].read(row['member'])
                assert len(data) == row['bytes'] and sha(data) == row['sha256']
                if 'packet_path' in row:
                    assert data == (P / row['packet_path']).read_bytes()
                count += 1
    finally:
        for package in packages.values():
            package.close()
    assert count == 402
    bundle = P / 'frozen-native-cube-targets-packet.tar.gz'
    assert sha(bundle.read_bytes()) == '3cc31fddd2e24ef3b69b877828f80143c3605d30c9307e4c5942a977cb51d772'
    names = {Path(r['path']).name for r in prepared['files']} | {descriptor.name}
    with tarfile.open(bundle) as archive:
        members = archive.getmembers()
        assert len(members) == len(names) and all(m.isfile() for m in members)
        assert {m.name for m in members} == names
        for name in names:
            assert archive.extractfile(name).read() == (P / name).read_bytes()
    helper = (P / 'build-native-cube-targets-reference-5624e76-01.ps1').read_text()
    for flag in ["'/W4'", "'/WX'", "'/MT'", "'/Zc:preprocessor'", "'/NODEFAULTLIB:oldnames.lib'"]:
        assert flag in helper
    assert source['groups'] == prepared['build_groups']
    assert source['fixtures'] == [prepared['fixture']]
    assert source['cube_oracle'] == dict(views=24, snapshots=30, native_words=35700,
                                       public_words=35700, callbacks=46, original_files=138)
    assert (P / 'verify-cube-array-target-originals.py').read_bytes() == subprocess.check_output([
        'git', '-C', str(W / 'dxvk-umd-ci'), 'show', C + ':tests/verify-cube-array-target-originals.py'])
    ast.parse((P / 'verify-cube-array-target-originals.py').read_text())
    assert sha((P / 'owned-raw-process-f4bf37f-02.cs').read_bytes()) == 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
    proof = dict(verified=True, compiled_source=C, prepared_sha256=sha(descriptor.read_bytes()),
                 exact_Git_source_inputs=160, original_package_members=402, libraries=12,
                 supplemental_AST_only_source=A, supplemental_AST_only_inputs=2,
                 frozen_bundle_members=len(names), separately_joined_Git_original_reader=1,
                 compiler_tool_scope=369, SDK_header_scope=21,
                 native_parse_files=7, manual_build_parser_native_owner_review=True,
                 expected_COFF=16, expected_PE=1, expected_readback_files=138,
                 oracle_native_words=35700, oracle_public_words=35700, oracle_callbacks=46,
                 full_compiler_attestation=False, native_AST_pending=True, native_reference_pending=True,
                 host_driver_review_pending=True, target_calls=0, hardware_acceptance=False)
    output = P / 'root-prepared-cube-targets-reference-01.json'
    with output.open('x') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(path=str(output), bytes=output.stat().st_size, sha256=sha(output.read_bytes()))))


if __name__ == '__main__':
    main()
