#!/usr/bin/env python3
"""Direct original joins for the failed combined-cube native WARP reference and release."""
import ast
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tarfile

WS = Path('/home/sunf/droidvm-repos')
BASE = WS / 'artifacts/native-cube-integration-20261008/guest-native-8528d91-03'
PREP = BASE.parent / 'prepared-native-8528d91-03'
SOURCE = '8528d91357255fe8f31138d5438e7313e2367fec'
ROOT = 'DxvkNativeCubeIntegration-8528d91-03/'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(row):
    data = Path(row['path']).read_bytes()
    assert len(data) == row['bytes'] and sha(data) == row['sha256']
    return data


def closed(row, expected):
    assert row['pid'] > 0 and row['retained_process_handle'] and row['exited'] and row['exit_code_available']
    assert row['exit_code'] == expected and row['pipes_drained']
    assert not row['timed_out'] and not row['child_still_running'] and not row['failure']


def main():
    release_path = BASE / 'target-cpu-release-03.json'
    assert release_path.stat().st_size == 19420
    assert sha(release_path.read_bytes()) == 'b3c9278e5f3082b09745b03e521764e904a21a0276cd785b527f13270c0f5610'
    release = read(release_path)
    assert release['released'] and release['next_owner'] == '/root'
    pin(release['authorization'])
    for value in release['original_joins']:
        pin(value)
    pin(release['original_archive'])
    archive = Path(release['original_archive']['path'])
    assert archive.stat().st_size == 49893545 and sha(archive.read_bytes()) == '07584ebd79967e01336818d7a28e5effb4c0ee82e7f9384e846b1fa76e76f6fb'
    with tarfile.open(archive) as tar:
        entries = [m for m in tar.getmembers() if m.isfile()]
        assert len(entries) == 391 and len({m.name.casefold() for m in entries}) == 391
        original = {m.name: tar.extractfile(m).read() for m in entries}
    members = read(BASE / 'native-cube-integration-8528d91-03-evidence.members.json')
    assert {r['path']: (r['bytes'], r['sha256']) for r in members} == {
        name: (len(data), sha(data)) for name, data in original.items()}
    result = json.loads(original[ROOT + 'build-result.json'].decode('utf-8-sig'))
    assert result['source_commit'] == SOURCE and result['architecture'] == 'arm64'
    assert result['failure'] == 'Actual child failed: fixture-run exit=-1073741819'
    assert not result['passed'] and not result['fixtures_passed'] and not result['fixtures']
    assert not result['hardware_acceptance'] and not result['ordinary_runtime_admission']
    assert not result['installation'] and not result['registration'] and not result['finalization_errors']
    for key in ('sources_retained', 'tools_retained', 'headers_retained', 'libraries_retained',
                'continuity_retained', 'system_libraries_retained'):
        assert result[key] and release['retention'][key]
    for first, after, count in [('sources_before', 'sources_after', 160), ('tool_inputs_before', 'tool_inputs_after', 369),
                                ('header_inputs', 'header_inputs_after', 21), ('library_inputs', 'library_inputs_after', 11)]:
        assert result[first] == result[after] and len(result[first]) == count
    assert result['system_libraries_before'] == result['system_libraries_after']
    source = read(PREP / 'cube-integration-source-8528d91-01.json')
    assert {(r['path'], r['bytes'], r['sha256']) for r in source['sources']} == {
        (r['path'], r['bytes'], r['sha256']) for r in result['sources_before']}
    for row in source['sources']:
        data = original[ROOT + 'source/' + row['path']]
        assert sha(data) == row['sha256'] and len(data) == row['bytes']
        repo = WS / 'dxvk-umd-ci'
        if row['submodule']:
            repo /= row['submodule']
        raw = subprocess.run(['git', '-C', str(repo), 'show', row['git_commit'] + ':' + row['git_path']],
                             capture_output=True, check=True).stdout
        assert raw == data
    tools = read(PREP / 'native-cube-msvc-tool-inputs-01.json')
    assert {(r['path'], r['bytes'], r['sha256']) for r in tools['tools']} == {
        (r['path'], r['bytes'], r['sha256']) for r in result['tool_inputs_before']}
    for filename, rows_key, input_key, folder in [('native-warp-sdk-headers-f4bf37f-01.json', 'headers', 'header_inputs', 'headers'),
                                               ('native-cube-integration-libraries-03.json', 'libraries', 'library_inputs', 'libraries')]:
        rows = read(PREP / filename)[rows_key]
        assert {(r['name'], r['bytes'], r['sha256']) for r in rows} == {
            (r['name'], r['bytes'], r['sha256']) for r in result[input_key]}
        for row in rows:
            data = original[ROOT + folder + '/' + row['name']]
            assert len(data) == row['bytes'] and sha(data) == row['sha256']
    policy = ast.parse((PREP / 'verify-registration-originals-09.py').read_text())
    protected = next(ast.literal_eval(n.value) for n in policy.body if isinstance(n, ast.Assign)
                     and any(isinstance(t, ast.Name) and t.id == 'PROTECTED' for t in n.targets))
    assert len(protected) == 34

    def critical(snapshot):
        def values(raw):
            return {name: (name in raw, raw.get(name)) for name in protected}
        active = dict(snapshot['active_device'])
        active['values'] = values(active['values'])
        return dict(active_device=active, driver_registry={f['key']: values(f['values']) for f in snapshot['driver_registry']},
                    desktop=snapshot['desktop'])

    assert critical(result['continuity_before']) == critical(result['continuity_after'])
    assert result['ci_ast_passed'] and result['ci_ast_receipt']['passed'] and result['ci_ast_receipt']['parse_errors'] == 0
    assert result['ci_ast_receipt']['source_before'] == result['ci_ast_receipt']['source_after']
    assert len(result['ci_ast_receipt']['scripts']) == 2 and not result['ci_ast_receipt']['script_execution']
    parsed = json.loads(original['DxvkNativeCubeIntegrationParser-8528d91-03/parser-result.json'].decode('utf-8-sig'))
    assert parsed['passed'] and parsed['parse_errors'] == 0 and parsed['process_owner_compiled']
    assert len(parsed['scripts']) == 6 and parsed['source_before'] == parsed['source_after']
    prefix = 'C:\\Users\\Public\\DxvkNativeCubeIntegration-8528d91-03\\'

    def native_path(path):
        assert path.startswith(prefix)
        return ROOT + path[len(prefix):].replace('\\', '/')

    assert len(result['stages']) == 8
    for index, row in enumerate(result['stages']):
        closed(row, -1073741819 if index == 7 else 0)
        for stream in ('stdout', 'stderr'):
            assert len(original[native_path(row[stream])]) == row[stream + '_bytes']
    failed = result['stages'][-1]
    raw = original[native_path(failed['stdout'])] + original[native_path(failed['stderr'])]
    assert b'subresource=4' in raw and b'actual=ff040001' in raw and b'expected=ff000000' in raw
    assert result['shader_dependency_warnings'] == result['ddi_dependency_warnings'] == 0
    assert result['parser_dependency_warnings'] == 65
    outputs = result['original_outputs']
    assert len(outputs) == 17
    for row in outputs:
        data = original[native_path(row['path'])]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        if data[:2] == b'MZ':
            pe = struct.unpack_from('<I', data, 60)[0]
            assert data[pe:pe + 4] == b'PE\0\0' and struct.unpack_from('<H', data, pe + 4)[0] == 0xaa64
        else:
            assert struct.unpack_from('<H', data, 6 if data[:4] == b'\0\0\xff\xff' else 0)[0] == 0xaa64
    native = []
    for mode in ('parse', 'build', 'collect'):
        stem = 'native-cube-integration-8528d91-03-' + mode
        row = read(BASE / (stem + '.process-result.json'))
        closed(row, 1 if mode == 'build' else 0)
        for stream in ('stdout', 'stderr'):
            assert (BASE / (stem + '.' + stream + '.raw')).stat().st_size == row[stream + '_bytes']
        native.append(row)
    collection = read(BASE / 'native-originals-transfer-join-01.json')['original_collection']['process']
    closed(collection, 0)
    native += result['stages'] + [collection]
    assert {(r['pid'], r['retained_process_handle'], r['exit_code']) for r in native} == {
        (r['pid'], r['retained_process_handle'], r['exit_code']) for r in release['native_owned_processes']}
    assert len(native) == release['native_owned_process_count'] == 12
    host = read(BASE / 'host-attempt-finalization-01.json')['transports']
    outer_pin = next(v for v in release['original_joins']
                     if Path(v['path']).name == 'owned-host-driver-process-original-03.json')
    outer = json.loads(pin(outer_pin).decode('utf-8-sig'))
    offline = read(BASE / 'offline-original-reviews-processes-03.json')
    continuity = read(BASE / 'independent-failed-source-continuity-03/failed-source-continuity-originals-verified-03.json')
    assert len(host) == 8 and len(offline) == 2 and len(continuity['git_owned_processes']) == 2
    host = host + [outer] + offline + continuity['git_owned_processes']
    assert len(host) == release['host_owned_process_count'] == 13
    for row in host:
        assert row['exited'] and not row['timed_out']
        if 'pipes_drained' in row:
            assert row['pipes_drained']
        else:
            assert row == outer and row['streams_closed'] and row['stdout_drained'] and row['stderr_drained']
        assert row['exit_code'] in (0, 1) and not Path('/proc', str(row['pid'])).exists()
        assert not row.get('child_still_running', False)
        for stream in ('stdout', 'stderr'):
            pin(row[stream])
    assert {r['pid'] for r in host} == set(release['host_owned_process_ids'])
    assert not release['pending_native_children'] and not release['pending_host_transports']
    assert release['retained_AA64_COFF'] == 16 and release['retained_AA64_PE'] == 1
    assert not release['full_reference_suite_passed'] and not release['retry_performed']
    assert not release['source_or_frozen_helpers_modified']
    words = original[ROOT + 'output/texturecube/cube-readback-3-face-0-mip-4.words']
    metadata = original[ROOT + 'output/texturecube/cube-readback-3-face-0-mip-4.metadata']
    assert struct.unpack('<6I', metadata) == (3, 4, 1, 1, 4, 16)
    assert struct.unpack('<I', words)[0] == 0xff040001
    stdout = original[native_path(failed['stdout'])].decode('utf-8-sig')
    assert 'BACKEND=WARP' in stdout and 'VIOGPU=false' in stdout
    assert stdout.count('PASS TextureCube case') == 3
    prepared_root = PREP / 'root-prepared-native-cube-direct-review-03.json'
    assert sha(prepared_root.read_bytes()) == 'bde0c0189b88ac22c1dc802750b1e583ac026314efc024756bad565840397151'
    proof = dict(schema='root-combined-cube-native03-failure-originals-v1', verified=True, source_commit=SOURCE,
                 archive_sha256=sha(archive.read_bytes()), original_files=391, source_Git_inputs=160,
                 compiler_files=369, SDK_headers=21, libraries=11, ARM64_COFF=16, ARM64_PE=1,
                 native_helper_AST=6, native_CI_AST=2, strict_first_party_warnings=0, pinned_dependency_warnings=65,
                 native_children_closed=12, host_transports_closed=8, outer_and_offline_closed=5,
                 static_registration_names=34, driver_desktop_unchanged=True,
                 failure='scoped mip readback subresource4 ff040001 versus ff000000', reference_fixture_cases_passed=3,
                 full_reference_suite_passed=False, original_failed_readback_joined=True,
                 prepared_package_originals_ROOT_review_sha256=sha(prepared_root.read_bytes()),
                 release_sha256=sha(release_path.read_bytes()), target_released_to_ROOT=True,
                 hardware_acceptance=False, ordinary_runtime_admission=False, target_calls=False)
    with (BASE / 'root-native03-failure-and-release-direct-review-01.json').open('x') as output:
        json.dump(proof, output, indent=2)
        output.write('\n')
    print(json.dumps(proof, sort_keys=True))


if __name__ == '__main__':
    main()
