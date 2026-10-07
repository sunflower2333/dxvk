#!/usr/bin/env python3
"""Reopen one closed USER phase; never advance a phase from runner booleans."""
import argparse
import hashlib
import importlib.util
import json
import re
import tarfile
import xml.etree.ElementTree as ET
from pathlib import Path, PurePosixPath

PROBE_SOURCE = '5c420e4daddc39effb2c8e8a28bd07ec7407c402'
CORE_SOURCE = 'd7e5c7d46b8ce889e993bfab66a3b78b076c49d1'
RUN = 37648387721
SID = 'S-1-5-21-362894365-441372107-2852668596-1000'
RAW = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
SYS = 'd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a'
PRIOR = {'enumerate': 'names', 'offscreen': 'enumerate', 'present': 'offscreen'}
PAYLOADS = {
    'core': (5488640, '7be8cbb9850407ccc304528911a6fbd71b01971fbeb4a86550cc8dcc2f346a3f', 'viogpudxvk.dll'),
    'loader': (677888, 'd459f2d09080865cc3d591b498c02d38305a26963b401152f8230dc60c5ad7e7', 'viogpu_gl_loader_x86.dll'),
    'icd': (14300672, '2b549889816163433faabe6f2c1d2a61d6c106078d08e30031b74c0a66cd7f5c', 'viogpu_gl_vk_x86.dll'),
    'icd-json': (145, '74d7d5d6ae9432cde2d802507ed59bbe4c2f2b95d01e7ac3c2b56e9691932c80', 'freedreno_icd.json')}
HELPERS = {'run-native-d3d8-system-phase.ps1', 'invoke-native-d3d8-system-phase.ps1', 'collect-native-d3d8-system-phase.ps1',
           'owned-raw-process-f4bf37f-02.cs', 'inspect-process-token.ps1', 'inspect-viogpu-readiness-fast-02.ps1', 'verify-registration-originals-09.py'}


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_json(data):
    return json.loads(data.decode('utf-8-sig'))


def closed(row, stdout, stderr, deadline_ms=30000):
    require(row['Pid'] > 0 and row['ProcessHandle'] != 0, 'original PID/OS handle missing')
    require(row['Exited'] and row['ExitCodeAvailable'] and row['PipesDrained'], 'original exit/drain incomplete')
    require(not row['TimedOut'] and not row['ChildStillRunning'] and not row['Failure'] and row['ExitCode'] == 0, 'original phase failed')
    require(row['StdoutBytes'] == len(stdout) and row['StderrBytes'] == len(stderr), 'raw pipe length mismatch')
    require(0 <= row['Seconds'] < deadline_ms / 1000 + 30 and row['StartUtc'], 'original bounded process timing missing')


def user(token):
    require(token['user'] == r'DROIDVM\USER' and token['sid'] == SID and token['session_id'] == 1,
            'original USER/session1 mismatch')
    require(token['elevated'] is False and token['elevation_type'] == 3 and token['integrity_rid'] == 8192,
            'original limited token mismatch')


def lines(text):
    return text.replace('\r\n', '\n').splitlines()


def single(rows, prefix):
    found = [row for row in rows if row.startswith(prefix)]
    require(len(found) == 1, 'expected one ' + prefix)
    return found[0]


def match_identity(rows, luid, source):
    require(single(rows, 'D3D8_USER_GATE ') == f'D3D8_USER_GATE session=1 elevation=0 elevation_type=3 integrity_rid=8192 sid={SID}', 'native child USER token mismatch')
    found = re.fullmatch(r'D3D8_KMT_MATCH adapter=([1-9]\d*) source=(\d+) luid=([0-9a-f]{16}) software=0 render=1 no_device=1', single(rows, 'D3D8_KMT_MATCH '))
    require(found and found.group(2, 3) == (str(source), luid), 'actual I386 KMT adapter/source mismatch')
    require(rows.count('D3D8_KMT_CLOSED status=00000000') == 1, 'original KMT adapter not closed')


def verify_names(text, luid, source):
    rows = lines(text)
    match_identity(rows, luid, source)
    machines = single(rows, 'D3D8_PROCESS_MACHINE ')
    require(re.fullmatch(r'D3D8_PROCESS_MACHINE process=014c native=aa64 effective=014c pointer_bytes=4 legacy_status=[01] legacy_wow=[01]', machines),
            'actual I386 process on ARM64 machine evidence required')
    directory = single(rows, 'D3D8_SYSTEM_DIRECTORY ')
    selected = re.fullmatch(r'D3D8_SYSTEM_DIRECTORY machine=014c api=GetSystemWow64Directory2W path=(.+)', directory)
    require(selected and selected[1].casefold() == r'C:\Windows\SysWOW64'.casefold(),
            'explicit I386 system directory selection required')
    gdi = single(rows, 'D3D8_SYSTEM_GDI32 ')
    paths = re.fullmatch(r'D3D8_SYSTEM_GDI32 actual=(.+) expected=(.+) machine=014c pointer_bytes=4', gdi)
    require(paths and paths[1].casefold() == paths[2].casefold() == r'C:\Windows\SysWOW64\gdi32.dll'.casefold(),
            'actual and expected genuine I386 GDI32 paths must match')
    require(rows.index(machines) < rows.index(directory) < rows.index(gdi) < rows.index(single(rows, 'D3D8_KMT_MATCH ')),
            'machine/path evidence must precede KMT matching')
    name = re.fullmatch(r'D3D8_KMT_NAME version=0 status=00000000 terminated=1 name=(.+) pointer_bytes=4 raw_bytes=524', single(rows, 'D3D8_KMT_NAME '))
    require(name, 'successful original I386 legacy name query required')
    words = []
    for row in rows:
        if not row.startswith('D3D8_KMT_NAME_WORD '):
            continue
        match = re.fullmatch(r'D3D8_KMT_NAME_WORD index=(\d+) value=([0-9a-f]{4})', row)
        require(match and int(match[1]) == len(words), 'duplicate, malformed or reordered original UTF-16 word')
        words.append(int(match[2], 16))
    require(len(words) == 260 and 0 in words, 'actual MAX_PATH260 original words required')
    raw = b''.join(word.to_bytes(2, 'little') for word in words)
    end = words.index(0)
    actual = raw[:end * 2].decode('utf-16le', errors='strict')
    require(actual == name[1] and 3 < len(actual) < 260, 'printed name differs from original words')
    require(re.fullmatch(r'[A-Za-z]:\\[^\x00\r\n"]+\.dll', actual, re.I) and '..' not in actual.split('\\'), 'bounded actual absolute registered filename required')
    require(rows.count('D3D8_KMT_NAMES_COMPLETE system_runtime_calls=0 create_device=0 core_loads=0 registry_writes=0') == 1, 'names phase scope mismatch')
    allowed = ('D3D8_USER_GATE ', 'D3D8_PROCESS_MACHINE ', 'D3D8_SYSTEM_DIRECTORY ', 'D3D8_SYSTEM_GDI32 ',
               'D3D8_KMT_MATCH ', 'D3D8_KMT_NAME ', 'D3D8_KMT_NAME_WORD ', 'D3D8_KMT_CLOSED ', 'D3D8_KMT_NAMES_COMPLETE ')
    require(all(row.startswith(allowed) for row in rows), 'names phase emitted factory/core/unexpected output')
    return {'registered_I386_filename': actual, 'name_words': 260, 'query_bytes': 524,
            'process_machine': '014c', 'native_machine': 'aa64', 'system_directory_api': 'GetSystemWow64Directory2W',
            'actual_I386_GDI32_path': paths[1], 'expected_I386_GDI32_path': paths[2],
            'create_device': False, 'core_loaded': False}


def verify_enumeration(text):
    rows = lines(text)
    require(not re.search(r'^(?:D3D8_(?:ERROR|FAILED|UNAVAILABLE)\b|SYSTEM_D3D8_(?:CREATE|DEVICE|LIFETIME)(?:_|\b))', text, re.M), 'enumeration attempted device or reported failure')
    require(single(rows, 'D3D8_RUNTIME ') == r'D3D8_RUNTIME path=C:\Windows\SysWOW64\d3d8.dll machine=014c pointer_bytes=4 sdk_version=220 caps_bytes=212', 'genuine system8 runtime identity required')
    adapters = re.findall(r'^D3D8_ADAPTER index=\d+ identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=([0-9a-f]{8}) vs=fffe0101 ps=ffff0104 constants=96$', text, re.M)
    require(len(adapters) == 1 and int(adapters[0], 16) & 0x90000 == 0x90000, 'actual bounded Interface8 HAL caps required')
    opens = re.findall(r'^SYSTEM_D3D8_OPEN_BEGIN interface=8 version=\d+ runtime=\S+ caller=C:\\Windows\\SysWOW64\\d3d8\.dll pointer_bytes=4 readonly=1$', text, re.M | re.I)
    require(opens, 'genuine Microsoft caller did not open Interface8')
    ends = re.findall(r'^SYSTEM_D3D8_OPEN_END hr=00000000 interface=8 driver_version=12 adapter=\S+ core=(.+) expected_ci_source_commit=([0-9a-f]{40}) machine=014c core_create_calls=0$', text, re.M)
    require(len(ends) == len(opens) and all(commit == CORE_SOURCE for _, commit in ends), 'actual typed adapter admission failed')
    expected_core = r'C:\Users\Public\DxvkD3D8Candidate-d7e5c7d-37648387721\viogpudxvk.dll'
    require(all(path == expected_core for path, _ in ends), 'actual adapter core path mismatch')
    require(single(rows, 'SYSTEM_D3D8_CORE_PIN ') == f'SYSTEM_D3D8_CORE_PIN path={expected_core} sha256={PAYLOADS["core"][1]} expected_ci_source_commit={CORE_SOURCE} machine=014c file_locked=1 core_unchanged=1', 'original locked I386 core pin mismatch')
    caps = re.findall(r'^SYSTEM_D3D8_CAPS12 id=(\d+) bytes=212 device_type=1 devcaps=([0-9a-f]{8}) caps2=[0-9a-f]{8} primitive=[0-9a-f]{8} vs=fffe0101 constants=96 ps=ffff0104$', text, re.M)
    require(caps, 'actual CAPS12 forwarding missing')
    for ident, devcaps in caps:
        require(int(devcaps, 16) & 0x90000 == 0x90000, 'HAL caps projection mismatch')
        words = re.findall(rf'^SYSTEM_D3D8_CAPS12_WORD id={ident} index=(\d+) value=([0-9a-f]{{8}})$', text, re.M)
        require([int(index) for index, _ in words] == list(range(53)), 'all53 CAPS12 words required')
        require(int(words[0][1], 16) == 1 and int(words[49][1], 16) == 0xfffe0101 and int(words[50][1], 16) == 96 and int(words[51][1], 16) == 0xffff0104, 'original CAPS12 output words mismatch')
    caps_ends = re.findall(r'^SYSTEM_D3D8_CAPS_END id=\d+ type=\d+ bytes=\d+ hr=([0-9a-f]{8}) caps_modified=0$', text, re.M)
    require(caps_ends and all(value == '00000000' for value in caps_ends), 'caps request failure retained')
    begins = re.findall(r'^SYSTEM_D3D8_CAPS_BEGIN id=(\d+) interface=8 type=(\d+) bytes=(\d+) info=[01] adapter=\S+$', text, re.M)
    completed = re.findall(r'^SYSTEM_D3D8_CAPS_END id=(\d+) type=(\d+) bytes=(\d+) hr=00000000 caps_modified=0$', text, re.M)
    require(sorted(begins) == sorted(completed) and len({row[0] for row in begins}) == len(begins), 'actual caps call/return pair mismatch')
    require(not any(row.startswith(('D3D8_PIXEL ', 'D3D8_SCREEN_PIXEL ', 'D3D8_PRIVATE_MODULE ')) for row in rows), 'enumeration emitted device pixels/private GPU modules')
    require(len(re.findall(r'^D3D8_SELECTOR installed=1 machine=014c pointer_bytes=4 slot_rva=[0-9a-f]+ registry_writes=0$', text, re.M)) == 1, 'exact owned system8 selector missing')
    restored = re.fullmatch(r'D3D8_SELECTOR restored=1 protection_restored=1 substitutions=([1-9]\d*) queries=([1-9]\d*)', single(rows, 'D3D8_SELECTOR restored='))
    require(restored and int(restored[2]) >= int(restored[1]), 'original system IAT restoration failed')
    closed_adapters = re.findall(r'^SYSTEM_D3D8_CLOSE adapter=\S+ runtime=\S+ hr=00000000 remaining=\d+ live_devices=0$', text, re.M)
    require(len(closed_adapters) == len(opens) and 'remaining=0 live_devices=0' in closed_adapters[-1], 'adapter owner remained live')
    require(re.fullmatch(r'D3D8_COMPLETE mode=front-enumerate adapters=[1-9]\d* create_device=0 presents=0 registry_writes=0', single(rows, 'D3D8_COMPLETE ')), 'enumeration-only scope mismatch')
    return {'HAL_caps': True, 'interface': 8, 'caps_bytes': 212, 'core_loaded': True, 'create_device': False}


def verify_archive(archive, collection_path, manifest_path):
    collection = read_json(collection_path.read_bytes())
    raw_archive = archive.read_bytes()
    require(collection['schema'] == 'system-d3d8-collection-v1' and collection['completed'] and collection['unchanged'] and not collection['failure'], 'original collector failed')
    require(collection['archive_sha256'] == digest(raw_archive) and collection['archive_bytes'] == len(raw_archive), 'original archive hash/length mismatch')
    closed(collection['process'], Path(str(archive) + '.stdout.raw').read_bytes(), Path(str(archive) + '.stderr.raw').read_bytes(), 60000)
    require(collection['runner_sha256'] == RAW and collection['command']['deadline_ms'] == 60000, 'original collector owner/deadline mismatch')
    contents = {}
    with tarfile.open(archive, 'r:gz') as tar:
        for member in tar:
            if member.isdir():
                continue
            path = PurePosixPath(member.name.removeprefix('./'))
            require(member.isfile() and not path.is_absolute() and '..' not in path.parts and str(path) not in contents, 'unsafe or duplicate original archive member')
            contents[str(path)] = tar.extractfile(member).read()
    expected = {row['name']: row for row in collection['members']}
    require(len(expected) == len(collection['members']) and expected.keys() == contents.keys(), 'original member set mismatch')
    for name, data in contents.items():
        require(expected[name]['sha256'] == digest(data) and expected[name]['bytes'] == len(data), 'original member hash mismatch: ' + name)
    manifest_bytes = manifest_path.read_bytes()
    require(contents['manifest-original.json'] == contents['output/manifest-original.json'] == manifest_bytes, 'independently pinned actual manifest mismatch')
    manifest = read_json(manifest_bytes)
    require(manifest['schema'] == 'system-d3d8-phase-inputs-v1' and manifest['ready'] and manifest['native_cpu']['accepted'], 'native inputs still pending')
    require(manifest['probe_source'] == PROBE_SOURCE and manifest['core_source'] == CORE_SOURCE and manifest['core_ci_run'] == RUN, 'source/CI identity mismatch')
    require(manifest['loader_source'] == '6a6878c614c8c6dbe81ee7a9f1176bdb52dc7dd7' and manifest['icd_source'] == '8443c71a5ab32b9d58b904fa51f4bf2f9089db8d', 'distinct original loader/ICD source mismatch')
    require(manifest['adapter_luid'] == 'ec6b000000000000' and manifest['source_id'] == 0, 'fresh selected adapter identity mismatch')
    require(len(manifest['helpers']) == len(HELPERS) and {row['name'] for row in manifest['helpers']} == HELPERS, 'exact frozen helper set required')
    native = manifest['native_cpu']
    native_proof_bytes = Path(native['original_proof_path']).read_bytes()
    native_archive_bytes = Path(native['original_archive_path']).read_bytes()
    require(digest(native_proof_bytes) == native['original_proof_sha256'] and digest(native_archive_bytes) == native['original_archive_sha256'], 'accepted original native CPU receipt/archive changed')
    native_proof = read_json(native_proof_bytes)
    require(native_proof['verified'] and native_proof['source_commit'] == PROBE_SOURCE and native_proof['archive_sha256'] == native['original_archive_sha256'], 'independent strict native CPU acceptance mismatch')
    config = read_json(contents['task-config-original.json'])
    task = read_json(contents['task-result-original.json'])
    finalized = read_json(contents['task-collection-original.json'])
    result = read_json(contents['output/result-original.json'])
    phase = result['phase']
    require(phase in ('names', *PRIOR) and config['phase'] == task['phase'] == finalized['phase'] == phase, 'original phase mismatch')
    require(task['completed'] and task['process_passed'] and not task['failure'] and result['completed'] and result['process_passed'] and not result['failure'], 'original runtime attempt failed')
    require(finalized['child_closed'] and finalized['task_removed'] and not finalized['unresolved'] and finalized['task_exit'] == 0 and finalized['task_definition_retained'] and finalized['state'] != 'Running', 'owned task/child finalization incomplete')
    user(result['process_token'])
    require(result['manifest_sha256'] == config['manifest_sha256'] == digest(manifest_bytes), 'original manifest identity mismatch')
    start = read_json(contents['start-original.json'])
    require(start['config_sha256'] == digest(contents['task-config-original.json']), 'actual task configuration changed after start')
    auth_bytes = contents['authorization-original.json']
    require(auth_bytes == contents['output/authorization-original.json'] and start['authorization_sha256'] == result['authorization_sha256'] == digest(auth_bytes), 'original explicit phase authorization mismatch')
    auth = read_json(auth_bytes)
    require(auth['authorized'] and auth['owner'] in ('/root', '/root/verify_ewdk_build') and auth['phase'] == phase and auth['manifest_sha256'] == digest(manifest_bytes) and auth['output'] == config['output'], 'authorization was for another phase/output')
    definitions = [contents[name] for name in ('task-definition-original.xml', 'task-definition-before-start.xml', 'task-definition-after.xml')]
    require(all(data == definitions[0] for data in definitions) and digest(definitions[0]) == config['task_definition_sha256'], 'original task definition changed')
    xml = ET.fromstring(definitions[0].decode('utf-8-sig'))
    ns = {'t': 'http://schemas.microsoft.com/windows/2004/02/mit/task'}
    principal = xml.find('t:Principals/t:Principal', ns)
    require(principal is not None and principal.findtext('t:UserId', namespaces=ns) in (SID, r'DROIDVM\USER') and principal.findtext('t:LogonType', namespaces=ns) == 'InteractiveToken' and principal.findtext('t:RunLevel', default='LeastPrivilege', namespaces=ns) == 'LeastPrivilege', 'original interactive Limited task principal required')
    require(xml.findtext('t:Settings/t:ExecutionTimeLimit', namespaces=ns) in ('PT90S', 'PT1M30S'), 'actual task deadline changed')
    action = xml.find('t:Actions/t:Exec', ns)
    require(action is not None and action.findtext('t:Command', namespaces=ns) == r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe', 'actual native management interpreter changed')
    expected_task_args = f'-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "{config["output"].rsplit(chr(92), 1)[0]}\\helpers\\invoke-native-d3d8-system-phase.ps1" -Action Run -Phase {phase} -RunId {config["run_id"]}'
    require(action.findtext('t:Arguments', namespaces=ns) == expected_task_args, 'original scheduled action changed')
    for row in manifest['helpers']:
        require(digest(contents['helpers/' + row['name']]) == row['sha256'] and contents['helpers/' + row['name']] == contents['output/helpers/' + row['name']], 'original executed helper mismatch')
    require(digest(contents['helpers/owned-raw-process-f4bf37f-02.cs']) == RAW and read_json(contents['output/raw-type-original.json'])['compiled'], 'actual raw owner/type compile missing')
    command = read_json(contents['output/command-original.json'])
    process = read_json(contents['output/process-original.json'])
    stdout, stderr = contents['output/probe.stdout.raw'], contents['output/probe.stderr.raw']
    require(process == result['child'] and command == result['command'] and command['deadline_ms'] == 30000 and command['runner_sha256'] == RAW, 'original owned process/command mismatch')
    closed(process, stdout, stderr)
    require(not stderr, 'actual diagnostic stderr requires review')
    require(result['inputs_before'] == result['inputs_after'] and result['system_before'] == result['system_after'], 'original files changed during phase')
    system = {row['path'].lower(): row for row in result['system_before']}
    require(set(system) == {r'c:\windows\syswow64\d3d8.dll', r'c:\windows\syswow64\d3d8thk.dll', r'c:\windows\syswow64\gdi32.dll', r'c:\windows\system32\d3d9.dll'}, 'original system file set mismatch')
    for path, row in system.items():
        leaf = 'system32-d3d9.dll' if path.endswith(r'system32\d3d9.dll') else path.rsplit(chr(92), 1)[1]
        data = contents['output/system-originals/' + leaf]
        require(digest(data) == row['sha256'] and len(data) == row['bytes'], 'original system library bytes missing')
        offset = int.from_bytes(data[0x3c:0x40], 'little')
        require(data[:2] == b'MZ' and data[offset:offset+4] == b'PE\0\0' and int.from_bytes(data[offset+4:offset+6], 'little') == (0xaa64 if leaf == 'system32-d3d9.dll' else 0x14c), 'actual system DLL architecture mismatch')
    require(system[r'c:\windows\syswow64\d3d8.dll']['sha256'] == '65d8980c469e45d862c68ad046731fd85c19401d3436fd46c65c19db1182dad8', 'original genuine Microsoft system8 bytes changed')
    before, after = result['readiness_before'], result['readiness_after']
    require(before == read_json(contents['output/readiness-before.json']) and after == read_json(contents['output/readiness-after.json']), 'original readiness receipt mismatch')
    policy = contents['helpers/verify-registration-originals-09.py']
    require(digest(policy) == '28f29b0e69e06adb78fc2d6da10d3cd3d1709cfbb7da4619c47b0f90cbbbe051', 'original static34 registration policy changed')
    frozen = {'__name__': 'frozen_static_registration', '__file__': 'originals/verify-registration-originals-09.py'}
    exec(compile(policy, frozen['__file__'], 'exec'), frozen)
    registration = frozen['verify_registration'](before, after)
    for key in before['active_device']:
        if key != 'values':
            require(before['active_device'][key] == after['active_device'][key], 'original SYS/binding/package/service/PnP changed: ' + key)
    require(before['desktop'] == after['desktop'], 'original desktop changed')
    require(before['active_device']['binary']['sha256'] == SYS and before['active_device']['binary']['bytes'] == 411040 and before['active_device']['pnp_error'] == 0 and before['active_device']['service_state'][0]['State'] == 'Running', 'installed signed SYS/PnP/service mismatch')
    require(sorted(row['Id'] for row in before['desktop']) == [1864, 4464], 'original desktop processes changed')
    for key in ('independent_admission', 'registry_driver_writes', 'installation', 'VM_changes'):
        require(result[key] is False, 'unexpected runner admission or persistent mutation')
    files = {row['role']: row for row in manifest['files'] if phase in row['phases']}
    with tarfile.open(Path(native['original_archive_path']), 'r:gz') as original:
        result_bytes = original.extractfile('result.json').read()
        native_result = read_json(result_bytes)
        require(native_result['status'] == 'PASS' and native_result['source_commit'] == PROBE_SOURCE, 'accepted native original build result mismatch')
        for role, path in (('probe', 'probe/d3d8-runtime-probe.exe'), ('frontend', 'front/viogpu-d3d8-runtime-front.dll')):
            data = original.extractfile(path).read()
            pin = next(row for row in manifest['files'] if row['role'] == role)
            require(digest(data) == pin['sha256'] and len(data) == pin['bytes'], 'actual native I386 output chain mismatch')
            row = [item for item in native_result['outputs'] if item['path'] == pin['path']]
            require(len(row) == 1 and row[0]['sha256'] == pin['sha256'] and row[0]['machine'] == 0x14c, 'native PE/probe output receipt mismatch')
    require(len(manifest['files']) == 6 and {row['role'] for row in manifest['files']} == {'probe', 'frontend', *PAYLOADS}, 'exact six phase input definitions required')
    require(files.keys() == ({'probe'} if phase == 'names' else {'probe', 'frontend', *PAYLOADS}), 'unexpected selected phase input')
    for row in manifest['files']:
        if row['role'] in PAYLOADS:
            size, sha, name = PAYLOADS[row['role']]
            folder = r'C:\Users\Public\DxvkD3D8Candidate-d7e5c7d-37648387721'
            require(row['bytes'] == size and row['sha256'] == sha and row['path'] == folder + chr(92) + name, 'original I386 payload/config pin mismatch')
    require({row['role'] for row in result['inputs_before']} == files.keys(), 'exact phase payload set mismatch')
    for row in result['inputs_before']:
        pin = files[row['role']]
        require(all(row[key] == pin[key] for key in ('path', 'sha256', 'bytes')), 'original input pin mismatch')
    require(command['executable'] == files['probe']['path'] and command['working_directory'] == config['output'], 'original executable/CWD mismatch')
    luid, source = manifest['adapter_luid'], manifest['source_id']
    text = stdout.decode('utf-8', errors='strict').replace('\r\n', '\n')
    if phase == 'names':
        require(command['arguments'] == f'--kmt-names {luid} {source}' and 'prior-admission-original.json' not in contents, 'names command/scope mismatch')
        detail = verify_names(text, luid, source)
        registered = detail['registered_I386_filename']
    else:
        prior_bytes = contents['prior-admission-original.json']
        require(prior_bytes == contents['output/prior-admission-original.json'] and digest(prior_bytes) == config['prior_admission_sha256'], 'independent previous-phase proof changed')
        prior = read_json(prior_bytes)
        require(prior['verified'] and prior['phase'] == PRIOR[phase] and prior['manifest_sha256'] == digest(manifest_bytes) and prior['probe_source'] == PROBE_SOURCE and prior['core_source'] == CORE_SOURCE and prior['luid'] == luid and prior['source'] == source and prior['sid'] == SID, 'previous original admission mismatch')
        registered = prior['registered_I386_filename']
        mode = {'enumerate': 'front-enumerate', 'offscreen': 'front-offscreen', 'present': 'front-present'}[phase]
        arguments = f'--{mode} "{files["frontend"]["path"]}" "{registered}" "{files["core"]["path"]}" {files["core"]["sha256"]} {CORE_SOURCE}'
        if phase != 'enumerate':
            arguments += f' {luid} {source}'
        require(command['arguments'] == arguments, 'exact selected runtime command mismatch')
        if phase == 'enumerate':
            detail = verify_enumeration(text)
        else:
            spec = importlib.util.spec_from_file_location('pixels', Path(__file__).with_name('verify-native-d3d8-system-device.py'))
            pixels = importlib.util.module_from_spec(spec); spec.loader.exec_module(pixels)
            detail = pixels.verify(text, mode, luid, source)
    return {'schema': 'system-d3d8-phase-admission-v1', 'verified': True, 'phase': phase, 'manifest_sha256': digest(manifest_bytes),
            'probe_source': PROBE_SOURCE, 'core_source': CORE_SOURCE, 'core_ci_run': RUN, 'luid': luid, 'source': source, 'sid': SID,
            'registered_I386_filename': registered, 'original_archive_sha256': digest(raw_archive), 'original_archive_bytes': len(raw_archive),
            'original_files': len(contents), 'stdout_sha256': digest(stdout), 'process_sha256': digest(contents['output/process-original.json']),
            'collection_sha256': digest(collection_path.read_bytes()), 'details': detail, 'registration': registration,
            'scope': 'one original closed genuine system8 USER phase; later phases require a separate explicit target handoff'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path); parser.add_argument('--collection', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True); parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'preserve old proof; use a fresh output')
    proof = verify_archive(args.archive, args.collection, args.manifest)
    args.output.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps(proof, indent=2))
