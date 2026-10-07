#!/usr/bin/env python3
"""Root joins the retained native query originals; preserves the failed task."""
from pathlib import Path
import hashlib
import json
import struct
import tarfile

WS = Path('/home/sunf/droidvm-repos')
BASE = WS / 'reference/codes/dxvk-umd-dx10-kmt-probe-20261007/artifacts/dx10-kmt-user-gate-20261007/guest-dx10-private160-native-d7e5c7d-02/native-execution-02'
OUT = WS / 'artifacts/dx10-hardware-probe-01/root-private160-native02-actual-direct-review-01'


def ref(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def joined(row):
    assert ref(Path(row['path'])) == row, row['path']
    return row


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def main():
    proofs = {
        'native-private160-originals-joined-03.json': '48335cd24dd5adeaa3026e318cb84f94a8fd35765081eb881db5b33f03cbc055',
        'native-private160-query-originals-verified-02.json': '48ae71a9e2e851d2d0624a70ce8bce1e9dec6841e5e8d00e55743690d9a935ac',
        'target-cpu-release-private16002.json': 'b3cafadd303150d6d9582f962c8296dac2b835186e531bd6e674825b3e2c987d',
    }
    for name, sha in proofs.items():
        assert ref(BASE / name)['sha256'] == sha
    review = read(BASE / 'native-private160-originals-joined-03.json')
    decoder = read(BASE / 'native-private160-query-originals-verified-02.json')
    release = read(BASE / 'target-cpu-release-private16002.json')
    assert review['verified'] and review['readonly_query_passed']
    assert not review['full_native_attempt_passed'] and review['full_registry_rejection_preserved']
    assert release['released'] and release['owner_receiving'] == '/root'
    assert release['pending_target_children'] == release['pending_host_transports'] == 0
    assert release['owned_Limited_USER_task_removed']
    archive = joined(review['archive'])
    assert archive['sha256'] == '4c87c0fb8fbb23c8b39c9baf79c18eca318ea856a08e6ff7e07bb2656db08041'
    paths = {str(Path(row['path']).relative_to(BASE / 'originals')): joined(row)
             for row in review['archive_original_joins']}
    with tarfile.open(archive['path'], 'r:gz') as tar:
        members = [m for m in tar.getmembers() if m.isfile()]
        assert len(members) == len(paths) == review['archive_file_count'] == 134
        assert len({m.name.removeprefix('./') for m in members}) == 134
        for member in members:
            name = member.name.removeprefix('./')
            assert name in paths and not name.startswith('/') and '..' not in Path(name).parts
            data = tar.extractfile(member).read()
            assert data == (BASE / 'originals' / name).read_bytes()
            assert len(data) == paths[name]['bytes']
            assert hashlib.sha256(data).hexdigest() == paths[name]['sha256']
    assert len(review['original_collection_joins']) == review['original_collection_count'] == 133
    for row in review['original_collection_joins']:
        joined(row)
    assert len(review['source_joins']) == review['source_original_count'] == 37
    for row in review['source_joins']:
        frozen, actual = joined(row['frozen']), joined(row['actual'])
        assert (frozen['bytes'], frozen['sha256']) == (actual['bytes'], actual['sha256'])
        assert Path(frozen['path']).read_bytes() == Path(actual['path']).read_bytes()
    joined(review['master_original'])
    assert review['master_original']['sha256'] == 'fad9fef3980a29ea0d9b9bd7618fe2d2bd47565166c6e8ca6e012afc504a8432'
    for row in review['native_actual_processes']:
        for key in ('process', 'stdout', 'stderr'):
            joined(row[key])
        actual = read(Path(row['process']['path']))
        assert actual == row['actual']
        assert actual['Pid'] > 0 and actual['ProcessHandle'] > 0
        assert actual['Exited'] and actual['ExitCodeAvailable'] and actual['PipesDrained']
        assert not actual['ChildStillRunning'] and not actual['TimedOut']
        assert actual['StdoutBytes'] == row['stdout']['bytes']
        assert actual['StderrBytes'] == row['stderr']['bytes']
    assert len(review['native_actual_processes']) == review['native_actual_process_count'] == 17
    for row in review['host_actual_transports']:
        original = read(Path(joined(row['original'])['path']))
        assert original['local_owned_transport_pid'] == row['actual_pid'] > 0
        assert original['exit_code'] == row['exit_code']
        assert original['timed_out'] == row['timed_out']
        for key in ('stdout', 'stderr'):
            joined(row[key])
            assert original[key] == row[key]
    assert len(review['host_actual_transports']) == review['host_actual_transport_count'] == 15
    capture_path = Path(joined(decoder['result'])['path'])
    capture = read(capture_path)
    assert capture['completed'] and capture['passed'] and capture['failure'] is None
    assert all(capture[k] is False for k in ('ready', 'hardware_authorized', 'core_loaded', 'GPU_executed'))
    token = capture['process_token']
    assert token['user'] == r'DROIDVM\USER' and token['sid'] == 'S-1-5-21-362894365-441372107-2852668596-1000'
    assert token['session_id'] == 1 and not token['elevated']
    assert token['elevation_type'] == 3 and token['integrity_rid'] == 8192
    obs = capture['observation']
    assert obs['ExpectedLuid'] == obs['Luid'] == 'ec6b000000000000'
    assert obs['Source'] == 0 and obs['OwnedAdapter'] == 1073741824
    assert (obs['PointerBytes'], obs['OpenBytes'], obs['QueryBytes'], obs['CloseBytes']) == (8, 24, 24, 4)
    assert obs['OpenStatus'] == obs['CloseStatus'] == '00000000' and obs['Failure'] is None
    assert obs['CloseCalled'] and obs['ReleaseDcCalled'] and obs['ReleaseDc'] == 1
    for name, query_type, size in [('AdapterType', 15, 4), ('Private160', 0, 160), ('DriverVersion', 13, 4)]:
        query = obs[name]
        assert query['Status'] == '00000000' and query['Failure'] is None and query['Called']
        assert query['Type'] == query['ReturnedType'] == query_type
        assert query['RequestedBytes'] == query['ReturnedBytes'] == size
        assert query['ReturnedAdapter'] == obs['OwnedAdapter']
        assert query['OwnedDataPointer'] == query['ReturnedDataPointer']
        assert query['QueryFieldsRetained'] and query['GuardsRetained']
        for field, suffix in [('Before', 'before'), ('Raw', 'reply'), ('PrefixBefore', 'PrefixBefore'), ('PrefixAfter', 'PrefixAfter'), ('SuffixBefore', 'SuffixBefore'), ('SuffixAfter', 'SuffixAfter')]:
            assert (capture_path.parent / (name + '.' + suffix + '.bin')).read_bytes() == bytes(query[field])
        assert bytes(query['PrefixBefore']) == bytes(query['PrefixAfter']) == b'\xa5' * 64
        assert bytes(query['SuffixBefore']) == bytes(query['SuffixAfter']) == b'\x5a' * 64
    for row in decoder['original_byte_joins']:
        joined(row)
    assert len(decoder['original_byte_joins']) == 18
    raw = (capture_path.parent / 'Private160.reply.bin').read_bytes()
    assert len(raw) == 160 and raw[:4] == b'DVMP' and raw[128:132] == b'VLID'
    assert struct.unpack_from('<Q', raw, 24)[0] == 2
    assert struct.unpack_from('<Q', raw, 112)[0] == 0
    assert raw[144:152].hex() == obs['Luid']
    assert obs['Identity']['Valid'] and obs['Identity']['GenerationDecimal'] == '2'
    assert obs['Identity']['CapabilitiesDecimal'] == '0'
    assert struct.unpack('<I', bytes(obs['AdapterType']['Raw']))[0] == 259
    assert struct.unpack('<I', bytes(obs['DriverVersion']['Raw']))[0] == 2200
    assert obs['RenderSupported'] and not obs['SoftwareDevice']
    protected = []
    for row in review['same_attempt_protected_state_reviews']:
        proof = read(Path(joined(row)['path']))
        before = read(Path(joined(proof['original_before'])['path']))
        after = read(Path(joined(proof['original_after'])['path']))
        assert set(before) == set(after)
        assert before['schema'] == after['schema'] and before['metadata_source'] == after['metadata_source']
        assert before['desktop'] == after['desktop'] and before['desktop_processes_complete'] == after['desktop_processes_complete']
        left, right = before['active_device'], after['active_device']
        assert set(left) == set(right)
        assert {k:v for k,v in left.items() if k != 'values'} == {k:v for k,v in right.items() if k != 'values'}
        assert {k:v for k,v in left['values'].items() if not k.startswith('Native')} == {k:v for k,v in right['values'].items() if not k.startswith('Native')}
        changed = [k for k in left['values'] if left['values'][k] != right['values'][k]]
        assert len(changed) == proof['active_device_runtime_diagnostic_changes']
        assert all(k.startswith('Native') for k in changed)
        assert len(before['driver_registry']) == len(after['driver_registry']) == 2
        for a, b in zip(before['driver_registry'], after['driver_registry']):
            assert set(a) == set(b) == {'key', 'values'} and a['key'] == b['key']
            assert set(a['values']) == set(b['values'])
            assert {k:v for k,v in a['values'].items() if not k.startswith('Native')} == {k:v for k,v in b['values'].items() if not k.startswith('Native')}
        assert before['driver_registry'] != after['driver_registry']
        assert proof['protected_state_equal'] and not proof['full_registry_equal']
        protected.append(dict(proof=row, before=proof['original_before'], after=proof['original_after'], observed_active_Native_differences=len(changed), full_registry_equal=False, protected_equal=True))
    task = read(BASE / 'originals/task/task-result-original.json')
    final = read(BASE / 'originals/attempt-finalization-original.json')
    cleanup = read(BASE / 'originals/task-finalization-original.json')
    assert task['completed'] and not task['passed'] and task['query_passed']
    assert task['finalization_errors'] == ['Actual Limited USER driver/desktop continuity changed']
    assert final['completed'] and not final['passed'] and final['actual_children_finalized']
    assert not final['retention']['active_device'] and not final['retention']['driver_registry']
    assert all(final['retention'][k] for k in ('desktop','tools','packet_originals','owned_task_absent'))
    assert cleanup['completed'] and cleanup['task_removed'] and cleanup['actual_child_receipts_finalized'] and cleanup['task_exit'] == 1
    output = dict(schema='root-private160-native02-originals-direct-review-v1', verified=True, passed=True,
                  scope='Readonly actual Limited USER adapter query only; full native attempt failed its whole-registry guard',
                  archive=archive, archive_members_joined=134, collection_members_joined=133,
                  source_originals_joined=37, master_original=review['master_original'],
                  native_processes_finalized=17, host_transports_finalized=15,
                  decoder_original=ref(BASE / 'native-private160-query-originals-verified-02.json'),
                  peer_original_review=ref(BASE / 'native-private160-originals-joined-03.json'),
                  release_original=ref(BASE / 'target-cpu-release-private16002.json'),
                  actual_query_original=ref(capture_path), process_token=token,
                  adapter_luid=obs['Luid'], source=0, generation=2, capabilities=0, wddm_version=2200,
                  render_supported=True, software_device=False, canary_originals_joined=18,
                  protected_readiness_reviews=protected, full_native_attempt_passed=False,
                  full_registry_rejection_preserved=True, task_removed=True,
                  hardware_authorized=False, ready=False, core_loaded=False, GPU_executed=False,
                  reviewer_source=ref(Path(__file__)))
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / 'root-actual-private160-originals-verified-01.json').open('x') as f:
        json.dump(output, f, indent=2); f.write('\n')
    print(json.dumps(ref(OUT / 'root-actual-private160-originals-verified-01.json')))


if __name__ == '__main__':
    main()
