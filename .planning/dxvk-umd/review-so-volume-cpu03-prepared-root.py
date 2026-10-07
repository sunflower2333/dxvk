#!/usr/bin/env python3
"""Join CPU03 original inputs and library directives; native linking is pending."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tarfile
import zipfile

W = Path('/home/sunf/droidvm-repos')
BASE = W / 'artifacts/dx11-so-volume-probes-20261008'
P = BASE / 'native-cpu-f96f512-03'
OLD = BASE / 'native-cpu-f96f512-02'
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


def defaults(raw):
    names = set()
    for quoted, plain in re.findall(r'/defaultlib:(?:"([^"]+)"|([^\s]+))', raw, re.I):
        name = (quoted or plain).lower()
        names.add(name if name.endswith('.lib') else name + '.lib')
    return names


def main():
    descriptor = P / 'prepared-native-cpu-packet.json'
    prepared = load(descriptor)
    assert sha(descriptor.read_bytes()) == 'd579884e691cb8c8be3279eb8d0eff3c9972a71f1dcad607804959fcb2e71806'
    assert prepared['source_commit'] == C and prepared['source_inputs'] == 25
    assert not prepared['ready'] and not prepared['hardware_authorized']
    assert not prepared['full_compiler_attestation'] and prepared['actual_native_link_completeness_pending']
    assert len(prepared['files']) == 16
    for value in prepared['files']:
        pin(value)
    manifest = load(prepared['source_manifest']['path'])
    pin(prepared['source_manifest'])
    previous = load(OLD / 'source-manifest.json')
    assert {k: v for k, v in manifest.items() if k != 'receipts'} == {
        k: v for k, v in previous.items() if k != 'receipts'}
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
    for name, value in manifest['receipts'].items():
        assert pin(value) == (P / name).read_bytes()
    for name in ['build-native-so-volume-cpu.ps1', 'collect-native-so-volume-cpu.ps1']:
        assert (P / name).read_text().replace('f96f512-03', 'f96f512-02') == (OLD / name).read_text()
    parser = (P / 'parse-native-so-volume-cpu.ps1').read_text().replace('f96f512-03', 'f96f512-02')
    for name in ['build-native-so-volume-cpu.ps1', 'collect-native-so-volume-cpu.ps1']:
        parser = parser.replace(sha((P / name).read_bytes()), sha((OLD / name).read_bytes()))
    assert parser == (OLD / 'parse-native-so-volume-cpu.ps1').read_text()
    libs = load(P / 'library-inputs.json')['libraries']
    assert len(libs) == 11 and libs[:8] == load(OLD / 'library-inputs.json')['libraries']
    assert {v['name'] for v in libs[8:]} == {'advapi32.lib', 'runtimeobject.lib', 'synchronization.lib'}
    bundle = P / 'frozen-prepared-native-packet.tar.gz'
    assert sha(bundle.read_bytes()) == 'b58d44956db82b13ad87178875e27ab932ef1b19e7fa474af1b34482de5bfe46'
    names = {Path(v['path']).name for v in prepared['files']} | {descriptor.name}
    with tarfile.open(bundle) as archive:
        assert len(archive.getmembers()) == len(names) == 17
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
        for receipt, key, count in [('header-inputs.json', 'headers', 22), ('library-inputs.json', 'libraries', 11)]:
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
    assert checked == 37
    closure = json.loads(pin(prepared['release_library_default_closure_proof']).decode('utf-8-sig'))
    assert closure['passed'] and not closure['missing'] and closure['release_archives_only']
    assert not closure['full_linker_completeness'] and not closure['library_member_selection_by_native_linker_verified']
    assert len(closure['processes']) == 11
    represented = {v['name'].lower() for v in libs}
    requested = set()
    for row in closure['processes']:
        assert row['actual_exit'] == 0 and not row['timed_out'] and row['pipes_drained']
        data = pin(row['input'])
        assert pin(row['input_after']) == data
        assert not pin(row['stderr'])
        raw = pin(row['stdout']).decode('utf-8-sig')
        names = defaults(raw)
        assert names == set(row['defaults'])
        requested |= names
    objects = json.loads(pin(prepared['actual_directives_original_proof']).decode('utf-8-sig'))
    assert objects['passed'] and len(objects['objects']) == 4 and objects['source_commit'] == C
    for row in objects['objects']:
        assert row['actual_exit'] == 0 and not row['timed_out'] and row['pipes_drained']
        assert pin(row['object_before']) == pin(row['object_after'])
        assert not pin(row['stderr'])
        requested |= defaults(pin(row['stdout']).decode('utf-8-sig'))
    requested -= set(prepared['explicit_nodefaultlib'])
    assert requested == set(closure['requested']) == set(prepared['default_library_names_represented'])
    assert requested <= represented == set(closure['available'])
    assert prepared['explicit_nodefaultlib'] == closure['explicit_nodefaultlib'] == ['oldnames.lib']
    builder = (P / 'build-native-so-volume-cpu.ps1').read_text()
    assert "'/W4','/WX'" in builder and "'/MT'" in builder and "'/O1'" in builder
    assert prepared['expected_native_build_children'] == 16
    assert 'Register-ScheduledTask' not in builder and 'Start-ScheduledTask' not in builder
    assert sha((P / 'owned-raw-process-f4bf37f-02.cs').read_bytes()) == 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
    for key in ['local_strict_proof', 'optimized_workload_proof', 'actual_directives_original_proof']:
        pin(prepared[key])
    proof = dict(verified=True, source_commit=C, descriptor_sha256=sha(descriptor.read_bytes()),
                 exact_Git_source_inputs=25, source_archive_unchanged=True, previous_eight_libraries_unchanged=True,
                 package_member_originals=37, scoped_tool_executable_files=4, scoped_SDK_headers=22,
                 library_files=11, actual_library_inspections=11, local_x86_x64_object_inspections=4,
                 requested_release_libraries=sorted(requested),
                 full_compiler_backend_and_include_attestation=False, native_link_completeness_pending=True,
                 bundle_originals=17, strict_flags_and_CPU_oracles_unchanged=True, expected_build_children=16,
                 native_execution_pending=True, owned_host_controller_review_pending=True,
                 target_calls=False, core_loaded=False, probe_executed=False, hardware_acceptance=False)
    output = P / 'root-prepared-native-so-volume-CPU03-review-01.json'
    with output.open('x') as stream:
        json.dump(proof, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(path=str(output), bytes=output.stat().st_size, sha256=sha(output.read_bytes()))))


if __name__ == '__main__':
    main()
