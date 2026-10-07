#!/usr/bin/env python3
"""Retain the failed names attempt and check original ownership/driver continuity."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tarfile
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
WORKSPACE = Path('/home/sunf/droidvm-repos')
BASE = WORKSPACE / 'artifacts/dxvk-native-d3d8-system-device-20261008'
RUN = BASE / 'guest-limited-user-i386-names-native05-01'
PREPARED = BASE / 'limited-user-i386-names-handoff-native05-03'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def check(row):
    path = Path(row['path'])
    assert all(pin(path)[k] == row[k] for k in ('bytes', 'sha256')), path
    return path


def process(row, stdout, stderr, code):
    assert row['Pid'] > 0 and row['ProcessHandle'] > 0
    assert row['Exited'] and row['ExitCodeAvailable'] and row['ExitCode'] == code
    assert row['PipesDrained'] and not row['TimedOut'] and not row['ChildStillRunning'] and not row['Failure']
    assert row['StdoutBytes'] == len(stdout) and row['StderrBytes'] == len(stderr)


def main():
    release_path = RUN / 'target-names-release-05.json'
    assert pin(release_path)['sha256'] == '90252e9d1dbb5d35bd96fc355abc0764046df774348f0131105b1becf9cce740'
    release = read(release_path)
    assert release['released'] and release['released_to'] == '/root'
    assert release['target_owner_released'] == '/root/verify_ewdk_build'
    assert release['pending_owned_processes'] == release['pending_owned_transports'] == 0
    assert release['task_removed'] and not release['names_passed']
    for key in ('valid_KMT_queries', 'factory_calls', 'GPU_runs'):
        assert release[key] == 0
    assert not release['core_loaded'] and not release['continuation'] and not release['retry']
    proof = read(check(release['original_review']))
    assert proof['verified_failure_retention'] and not proof['names_admitted'] and not proof['ready']
    check(proof['reviewer'])
    handoff = read(check(release['closed_host_handoff']))
    assert handoff['task_removed'] and handoff['owned_child_closed'] and handoff['pending_owned_processes'] == 0
    archive = check(release['original_archive'])
    assert pin(archive)['sha256'] == '390a9fd7d167144893fc902addbfe8f3fa88d184d50750a68c3214bf70ac778c'
    collection = read(Path(str(archive) + '.collection-original.json'))
    assert collection['completed'] and collection['unchanged'] and not collection['failure']
    assert collection['archive_bytes'] == archive.stat().st_size == 1961779
    assert collection['archive_sha256'] == pin(archive)['sha256']
    members = {}
    expected = {row['name']: row for row in collection['members']}
    assert len(expected) == len(collection['members']) == 38
    with tarfile.open(archive) as stream:
        for member in stream:
            name = member.name.removeprefix('./')
            assert not Path(name).is_absolute() and '..' not in Path(name).parts
            assert member.isdir() or member.isfile()
            if member.isdir():
                continue
            assert name not in members and name in expected
            data = stream.extractfile(member).read()
            assert len(data) == expected[name]['bytes'] and hashlib.sha256(data).hexdigest() == expected[name]['sha256']
            members[name] = data
    assert set(members) == set(expected)
    decoded = lambda name: json.loads(members[name].decode('utf-8-sig'))
    assert members['manifest-original.json'] == members['output/manifest-original.json'] == (PREPARED / 'phase-inputs-admitted-original.json').read_bytes()
    assert members['authorization-original.json'] == members['output/authorization-original.json'] == check(release['authorization']).read_bytes()
    result = decoded('output/result-original.json')
    assert result['completed'] and not result['process_passed'] and result['failure']
    assert result['probe_source'] == '5c420e4daddc39effb2c8e8a28bd07ec7407c402'
    assert result['manifest_sha256'] == '85e6f71d6ceaaa7325166c3486bd266277973aa5ba2c7004dc3908fa6c8ca8e6'
    assert result['authorization_sha256'] == '42bb1b24b47e09f31c9915098e8ae297eb04e242613b335161f0852fcd23edb1'
    token = result['process_token']
    assert token['user'] == 'DROIDVM\\USER' and token['sid'] == 'S-1-5-21-362894365-441372107-2852668596-1000'
    assert token['session_id'] == 1 and token['elevated'] is False and token['elevation_type'] == 3 and token['integrity_rid'] == 8192
    stdout = members['output/probe.stdout.raw']
    stderr = members['output/probe.stderr.raw']
    assert stdout == b'D3D8_USER_GATE session=1 elevation=0 elevation_type=3 integrity_rid=8192 sid=S-1-5-21-362894365-441372107-2852668596-1000\nD3D8_ERROR operation=explicit-process-machine-APIs win32=127\nD3D8_FAILED reason=explicit-process-machine-APIs\n'
    assert not stderr and result['registered_I386_filename'] is None and not result['independent_admission']
    assert result['child'] == release['native_child'] == decoded('output/process-original.json')
    process(result['child'], stdout, stderr, 1)
    assert result['inputs_before'] == result['inputs_after']
    assert result['inputs_before'][0]['sha256'] == '2cef9cfadf240b859355742199d4e710a3ebee5b9cead2604e8a1d5940be56b0'
    assert result['inputs_before'][0]['bytes'] == 631808
    command = decoded('output/command-original.json')
    assert command == result['command'] and command['arguments'] == '--kmt-names ec6b000000000000 0'
    assert command['deadline_ms'] == 30000 and command['runner_sha256'] == 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
    assert result['system_before'] == result['system_after'] and len(result['system_before']) == 4
    system_names = {'d3d8.dll': 'd3d8.dll', 'd3d8thk.dll': 'd3d8thk.dll',
                    'gdi32.dll': 'gdi32.dll', 'd3d9.dll': 'system32-d3d9.dll'}
    for row in result['system_before']:
        data = members['output/system-originals/' + system_names[row['path'].split('\\')[-1]]]
        assert len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256']
    before, after = decoded('output/readiness-before.json'), decoded('output/readiness-after.json')
    assert before == result['readiness_before'] and after == result['readiness_after']
    assert before['desktop'] == after['desktop']
    fields = ('device_id', 'status', 'binary', 'installed_package', 'driver_binding',
              'service', 'service_state', 'pnp_error', 'pnp_status', 'key')
    assert all(before['active_device'][k] == after['active_device'][k] for k in fields)
    # Resolve the original static reader from the exact prepared input row.
    prepared = read(PREPARED / 'prepared-i386-names-01.json')
    reader_path = next(Path(row['path']) for row in prepared['local_inputs'] if Path(row['path']).name == 'verify-registration-originals-09.py')
    assert pin(reader_path)['sha256'] == '28f29b0e69e06adb78fc2d6da10d3cd3d1709cfbb7da4619c47b0f90cbbbe051'
    assert members['helpers/verify-registration-originals-09.py'] == reader_path.read_bytes()
    spec = importlib.util.spec_from_file_location('original_static_registration', reader_path)
    reader = importlib.util.module_from_spec(spec); spec.loader.exec_module(reader)
    static = reader.verify_registration(before, after)
    assert static['verified'] and len(static['protected_static_value_names']) == 34
    config = decoded('task-config-original.json')
    for row in config['helpers']:
        assert members['helpers/' + row['name']] == members['output/helpers/' + row['name']]
        assert len(members['helpers/' + row['name']]) == row['bytes']
        assert hashlib.sha256(members['helpers/' + row['name']]).hexdigest() == row['sha256']
    closure = decoded('task-collection-original.json')
    assert closure['task_removed'] and closure['child_closed'] and not closure['unresolved']
    assert closure['task_exit'] == 1 and closure['state'] != 'Running'
    namespace = {'t': 'http://schemas.microsoft.com/windows/2004/02/mit/task'}
    for name in ('task-definition-original.xml', 'task-definition-before-start.xml', 'task-definition-after.xml'):
        data = members[name]
        assert hashlib.sha256(data).hexdigest() == config['task_definition_sha256']
        principal = ET.fromstring(data.decode('utf-8-sig')).findall('t:Principals/t:Principal', namespace)
        assert len(principal) == 1
        assert principal[0].findtext('t:UserId', namespaces=namespace).casefold() in {token['user'].casefold(), token['sid'].casefold()}
        assert principal[0].findtext('t:LogonType', namespaces=namespace) == 'InteractiveToken'
        assert principal[0].findtext('t:RunLevel', namespaces=namespace) in (None, 'LeastPrivilege')
    assert collection['process'] == release['native_collector']
    process(collection['process'], Path(str(archive) + '.stdout.raw').read_bytes(), Path(str(archive) + '.stderr.raw').read_bytes(), 0)
    operations = read(RUN / 'host-operations-original.json')
    assert operations == release['host_operations'] and len(operations) == 7
    for row in operations:
        assert row['pid'] > 0 and row['completed'] and not row['timeout']
        assert row['exit'] == (1 if row['name'] == 'result-command' else 0)
        for channel in ('stdout', 'stderr'):
            data = (RUN / (row['name'] + '.' + channel + '.raw')).read_bytes()
            assert len(data) == row[channel + '_bytes'] and hashlib.sha256(data).hexdigest() == row[channel + '_sha256']
    assert release['outer']['pid'] > 0 and release['outer']['exited'] and release['outer']['exit_code'] == 1
    result = dict(schema='root-DX8-names05-failure-direct-review-v1', verified=True,
                  release_accepted=True, release=pin(release_path), archive=pin(archive),
                  original_files=38, actual_failed_child=release['native_child'],
                  actual_collector=release['native_collector'], host_operations=7,
                  limited_USER_session=1, original_helpers=7, original_system_files=4,
                  protected_driver_fields=10, protected_static_values=34,
                  failure='explicit-process-machine-APIs GetProcAddress / win32=127',
                  names_admitted=False, registered_I386_filename=None,
                  KMT_queries=0, factories=0, GPU_runs=0, target_calls=False)
    path = RUN / 'root-failed-I386-names-originals-direct-review-01.json'
    with path.open('x') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    print(json.dumps(dict(verified=True, release_accepted=True, original_files=38,
                          failure_retained=True, names_admitted=False, target_calls=False)))


if __name__ == '__main__':
    main()
