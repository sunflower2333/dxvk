#!/usr/bin/env python3
"""Rejoin frozen f96 CPU02 inputs; compilation and hardware remain pending."""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile

W = Path('/home/sunf/droidvm-repos')
BASE = W / 'artifacts/dx11-so-volume-probes-20261008'
P = BASE / 'native-cpu-f96f512-02'
OLD = BASE / 'native-cpu-f96f512-01'
R = W / 'reference/codes/dxvk-umd-dx11-so-volume-probe-20261008'
C = 'f96f512bc5ee1133a15e8eb86db8fedf3a2be89c'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def pin(value):
    data = Path(value['path']).read_bytes()
    assert len(data) == value['bytes'] and sha(data) == value['sha256'], value['path']
    return data


def main():
    descriptor = P / 'prepared-native-cpu-packet.json'
    prepared = load(descriptor)
    assert prepared['source_commit'] == C and prepared['source_inputs'] == 25
    assert prepared['ready'] is False and prepared['hardware_authorized'] is False
    assert prepared['full_compiler_attestation'] is False
    assert len(prepared['files']) == 13
    for value in prepared['files']:
        pin(value)
    manifest = load(prepared['source_manifest']['path'])
    previous = load(OLD / 'source-manifest.json')
    for key in manifest:
        if key != 'receipts':
            assert manifest[key] == previous[key], key
    assert pin(prepared['source_archive']) == (OLD / 'source.tar.gz').read_bytes()
    with tarfile.open(prepared['source_archive']['path']) as archive:
        members = archive.getmembers()
        assert len(members) == 25 and all(m.isfile() for m in members)
        assert len({m.name for m in members}) == 25
        originals = {m.name: archive.extractfile(m).read() for m in members}
    assert set(originals) == {v['path'] for v in manifest['sources']}
    for value in manifest['sources']:
        data = originals[value['path']]
        assert len(data) == value['bytes'] and sha(data) == value['sha256']
        assert data == subprocess.check_output(['git', '-C', str(R), 'show', C + ':' + value['path']])
        assert data == (R / value['path']).read_bytes()
    for name, value in manifest['receipts'].items():
        assert pin(value) == (P / name).read_bytes()
    for name in ['build-native-so-volume-cpu.ps1', 'collect-native-so-volume-cpu.ps1']:
        assert (P / name).read_text().replace('f96f512-02', 'f96f512-01') == (OLD / name).read_text()
    libs = load(P / 'library-inputs.json')['libraries']
    assert len(libs) == 8 and libs[:7] == load(OLD / 'library-inputs.json')['libraries']
    assert libs[-1]['name'] == 'uuid.lib' and (P / libs[-1]['packet_path']).read_bytes() == pin(prepared['original_uuid_payload'])
    bundle = P / 'frozen-prepared-native-packet.tar.gz'
    names = {Path(v['path']).name for v in prepared['files']} | {descriptor.name}
    with tarfile.open(bundle) as archive:
        assert len(archive.getmembers()) == len(names) == 14
        assert all(m.isfile() for m in archive.getmembers())
        assert {m.name for m in archive.getmembers()} == names
        for name in names:
            assert archive.extractfile(name).read() == (P / name).read_bytes()
    packages = {}
    checked = 0
    try:
        tool_receipt = load(P / 'tool-inputs.json')
        package = Path(tool_receipt['source_package'])
        assert sha(package.read_bytes()) == tool_receipt['source_package_sha256']
        packages[package] = zipfile.ZipFile(package)
        for value in tool_receipt['tools']:
            data = packages[package].read(value['member'])
            assert len(data) == value['bytes'] and sha(data) == value['sha256']
            checked += 1
        for receipt, key, count in [('header-inputs.json', 'headers', 22), ('library-inputs.json', 'libraries', 8)]:
            values = load(P / receipt)[key]
            assert len(values) == count
            for value in values:
                package = Path(value['source_package']['path'])
                if package not in packages:
                    assert sha(package.read_bytes()) == value['source_package']['sha256']
                    packages[package] = zipfile.ZipFile(package)
                data = packages[package].read(value['member'])
                assert len(data) == value['bytes'] and sha(data) == value['sha256']
                if 'packet_path' in value:
                    assert (P / value['packet_path']).read_bytes() == data
                checked += 1
    finally:
        for package in packages.values():
            package.close()
    assert checked == 34
    builder = (P / 'build-native-so-volume-cpu.ps1').read_text()
    assert "'/W4','/WX'" in builder and "'/MT'" in builder and "'/O1'" in builder
    assert prepared['expected_native_build_children'] == 16
    assert 'Register-ScheduledTask' not in builder and 'Start-ScheduledTask' not in builder
    assert sha((P / 'owned-raw-process-f4bf37f-02.cs').read_bytes()) == 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
    for key in ['local_strict_proof', 'optimized_workload_proof', 'actual_directives_original_proof']:
        pin(prepared[key])
    proof = dict(verified=True, source_commit=C, descriptor_sha256=sha(descriptor.read_bytes()),
                 exact_Git_source_inputs=25, source_archive_unchanged=True, old_seven_libraries_unchanged=True,
                 original_UUID_sha256=prepared['original_uuid_payload']['sha256'], package_member_originals=34,
                 scoped_tool_executable_files=4, scoped_SDK_headers=22, library_files=8,
                 full_compiler_backend_and_include_attestation=False, bundle_originals=14,
                 strict_flags_and_CPU_oracles_unchanged=True, expected_build_children=16,
                 native_execution_pending=True, owned_host_controller_pending=True,
                 target_calls=False, core_loaded=False, probe_executed=False, hardware_acceptance=False)
    output = P / 'root-prepared-native-so-volume-CPU02-review-01.json'
    with output.open('x') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(path=str(output), bytes=output.stat().st_size, sha256=sha(output.read_bytes()))))


if __name__ == '__main__':
    main()
