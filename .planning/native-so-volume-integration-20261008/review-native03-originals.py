#!/usr/bin/env python3
"""Join fresh original native source, processes and readbacks without target calls."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import tarfile

WS = Path('/home/sunf/droidvm-repos')
REPO = WS / 'reference/codes/dxvk-umd-so-volume-integration-20261008'
DEP = WS / 'dxvk-umd-ci/subprojects/dxbc-spirv'
ROOT = WS / 'artifacts/dxvk-so-volume-integration-20261008/native-warp-2489b0f-03'
ORIGINAL = ROOT / 'originals'
FROZEN = ROOT.parent / 'native-warp-2489b0f-02'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def local(path):
    text = re.sub('/+', '/', path.replace('\\', '/'))
    prefix = 'C:/Users/Public/DxvkNativeWarp-2489b0f-02/'
    assert text.casefold().startswith(prefix.casefold()), text
    return ORIGINAL / text[len(prefix):]


def main():
    result = read(ORIGINAL / 'build-result.json')
    assert result['passed'] and result['fixtures_passed'] and not result['finalization_errors']
    assert result['failure'] is None and result['collection_failure'] is None
    assert result['source_commit'] == '2489b0fd944ac2d3df18648c705d27e26ca3abc6'
    assert result['runner_source_sha256'] == 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
    rows = read(ROOT / 'native-warp-2489b0f-02-evidence.members.json')
    by = {row['path']: row for row in rows}
    assert len(by) == len(rows) == 611
    archive = ROOT / 'native-warp-2489b0f-02-evidence.tar.gz'
    with tarfile.open(archive, 'r:gz') as tar:
        files = [m for m in tar.getmembers() if m.isfile()]
        assert len(files) == 611
        for member in files:
            parts = Path(member.name).parts
            assert parts[0] == 'DxvkNativeWarp-2489b0f-02' and '..' not in parts
            rel = '/'.join(parts[1:])
            data = tar.extractfile(member).read()
            assert (ORIGINAL / rel).read_bytes() == data
            assert (len(data), hashlib.sha256(data).hexdigest()) == (by[rel]['bytes'], by[rel]['sha256'])
    manifest = read(FROZEN / 'native-warp-source-2489b0f-02.json')
    assert len(manifest['sources']) == len(result['sources_before']) == 93
    for row in manifest['sources']:
        git = subprocess.check_output(['git', 'show', row['git_commit'] + ':' + row['git_path']],
                                      cwd=DEP if row['submodule'] else REPO)
        actual = ORIGINAL / 'source' / row['path']
        assert git == actual.read_bytes()
        assert (len(git), hashlib.sha256(git).hexdigest()) == (row['bytes'], row['sha256'])
    retained = [k for k in result if k.endswith('_retained')]
    assert len(retained) == 11 and all(result[k] is True for k in retained)
    pairs = [('sources_before','sources_after'), ('header_inputs','header_inputs_after'),
             ('library_inputs','library_inputs_after'), ('continuity_before','continuity_after'),
             ('system_libraries_before','system_libraries_after'), ('system_d3d9_before','system_d3d9_after'),
             ('compiler','compiler_after'), ('linker','linker_after'), ('candidate_before','candidate_after'),
             ('caps_candidate_before','caps_candidate_after'), ('device_flags_candidate_before','device_flags_candidate_after')]
    for before, after in pairs:
        assert result[before] == result[after], before
    assert len(result['header_inputs']) == 21 and len(result['library_inputs']) == 7
    assert len(result['stages']) == 12
    children = []
    dependency_diagnostics = []
    for row in result['stages']:
        assert row['exited'] and row['exit_code_available'] and row['exit_code'] == 0
        assert row['pipes_drained'] and row['retained_process_handle'] > 0
        assert not row['timed_out'] and not row['child_still_running'] and row['failure'] is None
        stdout, stderr = local(row['stdout']), local(row['stderr'])
        assert stdout.stat().st_size == row['stdout_bytes'] and stderr.stat().st_size == row['stderr_bytes']
        receipt = stdout.with_name(stdout.name.replace('.stdout.txt', '.process-result.json'))
        assert read(receipt) == row
        if row['name'] in ['compile', 'compile-link']:
            diagnostics = [line for line in (stdout.read_text(errors='replace') + stderr.read_text(errors='replace')).splitlines()
                           if re.search(r'\b(?:warning|error)\s+[A-Z]+\d+\b', line, re.I)]
            response = (stdout.parent / ('compile.rsp' if row['name'] == 'compile' else 'compile-link.rsp')).read_text(encoding='utf-8-sig')
            if '/WX' in response:
                assert '/W4' in response and not diagnostics
            else:
                assert row['name'] == 'compile' and '/W3' in response
                assert all('\\subprojects\\dxbc-spirv\\' in line and ': warning ' in line for line in diagnostics)
                dependency_diagnostics.extend(diagnostics)
        children.append(dict(receipt=pin(receipt), pid=row['pid'], handle=row['retained_process_handle'],
                             exit=0, stdout=pin(stdout), stderr=pin(stderr)))
    expected_checks = dict(zip(['stream-output','d3d11-device','volume-policy','texture3d'], [1252,3733,385547,12025]))
    for fixture in result['fixtures']:
        path = local(fixture['executable']['path'])
        assert (path.stat().st_size, pin(path)['sha256']) == (fixture['executable']['bytes'], fixture['executable']['sha256'])
        data = path.read_bytes()
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        assert data[pe:pe+4] == b'PE\0\0' and struct.unpack_from('<H', data, pe+4)[0] == 0xaa64
        assert fixture['checks'] == expected_checks[fixture['name']]
        text = (path.parent / 'fixture-run.stdout.txt').read_text(encoding='utf-8-sig')
        assert all(marker in text for marker in fixture['required_markers'])
    volume = read(ROOT / 'root-complete-volume-original-words-verified-03.json')
    assert volume['passed'] and volume['original_files'] == 309
    assert volume['independently_calculated_voxels'] == 9138 and volume['independently_calculated_sampled_pixels'] == 945
    public = sorted((ORIGINAL / 'output/texture3d').glob('public-volume-*'))
    assert len(public) == 108
    observations = []
    for actual in public:
        if not actual.name.endswith('.actual.u32.bin'):
            continue
        expected = actual.with_name(actual.name.replace('.actual.', '.expected.'))
        a, e = actual.read_bytes(), expected.read_bytes()
        assert len(a) == len(e) and len(a) % 4 == 0
        aw, ew = struct.unpack('<'+'I'*(len(a)//4),a), struct.unpack('<'+'I'*(len(e)//4),e)
        observations.append(dict(name=actual.name, words=len(aw), mismatches=sum(x!=y for x,y in zip(aw,ew)), actual=pin(actual)))
    assert len(observations) == 36
    assert sorted(r['mismatches'] for r in observations) == [0]*30 + [1]*3 + [8]*3
    collector = read(ROOT / 'native-warp-2489b0f-02-evidence.process-result.json')
    assert collector['exit_code'] == 0 and collector['exited'] and collector['exit_code_available'] and collector['pipes_drained']
    assert not collector['timed_out'] and not collector['child_still_running'] and collector['retained_process_handle'] > 0
    hosts = sorted(ROOT.glob('*.original-command.json'))
    assert len(hosts) == 7
    for path in hosts:
        row = read(path)
        assert row['exit_code'] == 0 and row['timed_out'] is False
        for stream in ['stdout','stderr']:
            assert pin(Path(row[stream]['path'])) == row[stream]
    proof = dict(schema='root-native-volume03-original-direct-review-v1', passed=True,
                 source_commit=result['source_commit'], archive=pin(archive), directly_joined_members=611,
                 raw_Git_sources_joined=93, original_header_inputs=21, original_library_inputs=7,
                 all11retention_pairs_equal=True, native_build_children=children, collector=collector,
                 first_party_strict_W4_WX_diagnostics=0, unchanged_dependency_W3_warning_lines=len(dependency_diagnostics),
                 host_transports_finalized=7, native_children_finalized=13,
                 actual_fixture_checks=expected_checks, complete_volume_oracle=pin(ROOT / 'root-complete-volume-original-words-verified-03.json'),
                 separate_public_files=108, public_observations=observations,
                 raw_outputs_replaced_with_expected=False, production_scope='native WARP DDI resource/SO/SM5 controls',
                 hardware_acceptance=False, ordinary_runtime_admission=False, target_calls=False, source=pin(Path(__file__)))
    with (ROOT / 'root-native03-originals-direct-review.json').open('x') as out:
        json.dump(proof, out, indent=2)
        out.write('\n')
    print('ROOT_NATIVE03_ORIGINALS_PASS source93 members611 native13 host7 volume309 public108')


if __name__ == '__main__':
    main()
