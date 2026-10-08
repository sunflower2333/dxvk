#!/usr/bin/env python3
"""Join CPU03 failed originals and its final native/host closure locally."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tarfile

W = Path('/home/sunf/droidvm-repos')
P = W / 'artifacts/dx11-so-volume-probes-20261008/native-cpu-f96f512-03-owned-02'
PACKET = P.parent / 'native-cpu-f96f512-03'
BASE = 'DxvkNativeSoVolumeCpu-f96f512-03/'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def pin(row):
    data = Path(row['path']).read_bytes()
    assert len(data) == row['bytes'] and sha(data) == row['sha256']
    return data


def main():
    release_path = P / 'target-cpu-release.json'
    assert sha(release_path.read_bytes()) == '0175b8ff96bd41a09ab5d547872b7c4d8f898167e776d95580e648cf989ae69b'
    release = load(release_path)
    proof = json.loads(pin(release['owned_original_review']).decode('utf-8-sig'))
    assert proof['passed'] and proof['ownership_finalized'] and not proof['native_suite_passed']
    archive_bytes = pin(proof['archive'])
    assert sha(archive_bytes) == '191e5e8dc52bcfd1ef1d558537a554a7ededed66641420b86c5089339b4424f4'
    with tarfile.open(proof['archive']['path']) as archive:
        members = [m for m in archive.getmembers() if m.isfile()]
        assert len(members) == 156 and len({m.name for m in members}) == 156
        originals = {m.name: archive.extractfile(m).read() for m in members}
    local = {sha(x.read_bytes()): x.read_bytes() for x in P.iterdir() if x.is_file()}
    local.update({sha(x.read_bytes()): x.read_bytes()
                  for x in (P / 'native-final-ownership-observation').iterdir() if x.is_file()})

    def native_data(row):
        prefix = 'C:\\Users\\Public\\'
        if isinstance(row, str):
            assert row.startswith(prefix), row
            return originals[row[len(prefix):].replace('\\', '/')]
        assert row['path'].startswith(prefix), row['path']
        name = row['path'][len(prefix):].replace('\\', '/')
        data = originals[name] if name in originals else local[row['sha256']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        return data

    def closed(row):
        assert row['pid'] > 0 and row['retained_process_handle'] and row['exited'] and row['exit_code_available']
        assert row['pipes_drained'] and not row['timed_out'] and not row['child_still_running'] and not row['failure']
        for stream in ('stdout', 'stderr'):
            assert len(native_data(row[stream])) == row[stream + '_bytes']

    build = json.loads(originals[BASE + 'build-result.json'].decode('utf-8-sig'))
    assert build['source_commit'] == 'f96f512bc5ee1133a15e8eb86db8fedf3a2be89c'
    assert not build['passed'] and not build['core_loaded'] and not build['probe_executed']
    assert not build['finalization_errors'] and build['failure']['message'] == 'Actual native child exit 2: compile-link'
    manifest = load(PACKET / 'source-manifest.json')
    assert len(manifest['sources']) == 25
    for row in manifest['sources']:
        data = originals[BASE + 'source/' + row['path']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        assert data == subprocess.check_output(['git', '-C', str(W / 'dxvk-umd-ci'), 'show',
                                               build['source_commit'] + ':' + row['path']])
    for group, count in [('sources', 25), ('tools', 4), ('headers', 22), ('libraries', 11)]:
        assert build[group + '_before'] == build[group + '_after']
        assert len(build[group + '_before']) == count
        if group != 'sources':
            expected = load(PACKET / {'tools': 'tool-inputs.json', 'headers': 'header-inputs.json',
                                       'libraries': 'library-inputs.json'}[group])[group]
            assert {(r['bytes'], r['sha256']) for r in expected} == {
                (r['bytes'], r['sha256']) for r in build[group + '_before']}
    for row in load(PACKET / 'prepared-native-cpu-packet.json')['files']:
        assert originals['DxvkSoVolumeCpuPacket-f96f512-03/' + Path(row['path']).name] == pin(row)
    assert len(build['stages']) == 11
    for index, row in enumerate(build['stages']):
        closed(row)
        assert row['exit_code'] == (2 if index == 10 else 0)
    failed = build['stages'][-1]
    assert b'D9002' in native_data(failed['stderr']) and b'OLDNAMES.lib' in native_data(failed['stdout'])
    counts = {'COFF': 0, 'PE': 0}
    for name, data in originals.items():
        if not name.startswith(BASE + 'output/') or not name.endswith(('.obj', '.exe')):
            continue
        if data[:2] == b'MZ':
            offset = struct.unpack_from('<I', data, 60)[0]
            assert data[offset:offset + 4] == b'PE\0\0'
            assert struct.unpack_from('<H', data, offset + 4)[0] == 0xaa64
            counts['PE'] += 1
        else:
            assert struct.unpack_from('<H', data, 6 if data[:4] == b'\0\0\xff\xff' else 0)[0] == 0xaa64
            counts['COFF'] += 1
    assert counts == {'COFF': 3, 'PE': 2}
    observation = load(P / 'native-final-ownership-observation/owned-child-closure-observation.json')
    assert observation['passed'] and observation['recorded_owned_children_absent'] and len(observation['rows']) == 17
    for row in observation['rows']:
        assert not row['pid_present']
        child = json.loads(native_data(row['receipt']).decode('utf-8-sig'))
        closed(child)
        assert child['pid'] == row['pid']
    observed = load(P / 'native-final-ownership-observation/observe-phase-original.json')
    assert observed['passed'] and observed['controller_inputs_before'] == observed['controller_inputs_after']
    assert len(observed['children']) == 1
    closed(observed['children'][0])
    assert observed['children'][0]['exit_code'] == 0
    for row in proof['host_transports']:
        host = json.loads(pin(row['receipt']).decode('utf-8-sig'))
        assert not host['timed_out'] and host['exit_code'] == row['actual_exit'] in (0, 1)
        assert not Path('/proc', str(host['local_owned_transport_pid'])).exists()
        pin(host['stdout'])
        pin(host['stderr'])
    assert len(proof['host_transports']) == 9
    assert release['pending_owned_children'] == release['pending_host_transports'] == 0
    root = dict(verified=True, source_commit=build['source_commit'], archive_sha256=sha(archive_bytes),
                original_files=156, exact_Git_sources=25, scoped_official_native_inputs=37,
                native_build_children_closed=11, final_observed_closed_receipts=17,
                final_retained_observation_child_closed=1, host_transports_closed=9,
                strict_probe_COFF=2, strict_probe_PE=2, retained_partial_oracle_COFF=1, CPU_oracles_executed=0,
                native_suite_passed=False, failure='CPU oracle cl response misroutes linker flags; D9002 and LNK1104 OLDNAMES.lib',
                release_sha256=sha(release_path.read_bytes()), target_released_to_ROOT=True,
                hardware_acceptance=False, target_calls=0)
    output = P / 'root-native-CPU03-failure-release-direct-review-01.json'
    with output.open('x') as stream:
        json.dump(root, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(path=str(output), bytes=output.stat().st_size, sha256=sha(output.read_bytes()))))


if __name__ == '__main__':
    main()
