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

PROBE_SOURCE = 'e3ac12646a1b55742d575109063ae78507af5cec'
SETUP_SOURCE = '37b8a2dde5bfe7022ccc676594c1f57621cc8ff9'
LIFETIME_SOURCE = 'fbd7afdbdd277d9b2327a13efda5a0aca3905b25'
LIFETIME_READER = 'fa6dff4f0f0faf33475ba1200de534fdeddb37a5654a3963d037435ef1b39253'
OWNED_ICD_SHA = 'f50169e3e0efc6dea34fe0ce109228c79ce1df817a5508fb13a759c71d780ff3'
CORE_SOURCE = 'de72dc2e97bd8e4ea70c5bf89c26918d06065723'
RUN = 37711793677
SID = 'S-1-5-21-362894365-441372107-2852668596-1000'
RAW = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
SYS = 'd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a'
PRIOR = {'enumerate': 'names', 'offscreen': 'enumerate', 'present': 'offscreen'}
PAYLOADS = {
    'core': (5517312, 'ba60b53fe43c8901da3e37d8407958e8e83c05d973e48c94240a3d048bc09cba', 'viogpudxvk.dll'),
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


def closed(row, stdout, stderr, deadline_ms=30000, expected_exit=0):
    require(row['Pid'] > 0 and row['ProcessHandle'] != 0, 'original PID/OS handle missing')
    require(row['Exited'] and row['ExitCodeAvailable'] and row['PipesDrained'], 'original exit/drain incomplete')
    require(not row['TimedOut'] and not row['ChildStillRunning'] and not row['Failure'] and row['ExitCode'] == expected_exit, 'original phase failed')
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


IDENTITY_PREFIXES = ('D3D8_SYSTEM_API_LOOKUP ', 'D3D8_SYSTEM_PROCESS_ID ', 'D3D8_SYSTEM_PE ',
                     'D3D8_SYSTEM_FILE_ID ', 'D3D8_SYSTEM_FILE_CLOSE ', 'D3D8_SYSTEM_MODULE_ID ',
                     'D3D8_SYSTEM_DIRECTORY ')
SYSTEM_DIRECTORY = r'C:\Windows\SysWOW64'
SYSTEM_MODULES = {'kernel32.dll', 'kernelbase.dll', 'gdi32.dll', 'd3d8.dll', 'd3d8thk.dll',
                  'd3d9.dll', 'dxgi.dll', 'd3d11.dll', 'd3d9on12.dll'}


def verify_module_identities(rows):
    """Rejoin the actual common helper's raw records; never trust identity=1 alone."""
    selected = [(index, row) for index, row in enumerate(rows) if row.startswith(IDENTITY_PREFIXES)]
    groups, machines, directories, index = [], [], [], 0
    pending = None
    directory = None

    def take(pattern, message):
        nonlocal index
        require(index < len(selected), message)
        position, text = selected[index]
        match = re.fullmatch(pattern, text)
        require(match, message + ': ' + text)
        index += 1
        return position, match

    def pe(name, view):
        position, value = take(r'D3D8_SYSTEM_PE name=(\S+) view=(\S+) machine=(014c) magic=(010b) '
            r'sections=(\d+) pe=([0-9a-f]{8}) timestamp=([0-9a-f]{8}) image_bytes=(\d+) '
            r'header_bytes=(\d+) checksum=([0-9a-f]{8}) entry=([0-9a-f]{8})', 'complete selected I386 PE record required')
        require(value[1].casefold() == name and value[2] == view, 'selected PE name/view reordered')
        fields = tuple(value.group(i) for i in range(3, 12))
        require(int(value[5]) > 0 and int(value[6], 16) >= 64
            and int(value[8]) >= int(value[9]) >= int(value[6], 16) + 24 + 68
            and int(value[11], 16) < int(value[8]), 'selected PE size/offset/entry invalid')
        return position, fields

    def file(name, view):
        _, header = pe(name, view)
        _, value = take(r'D3D8_SYSTEM_FILE_ID name=(\S+) view=(\S+) input=(.+) final_nt=(.+) '
            r'volume=([0-9a-f]{8}) index_high=([0-9a-f]{8}) index_low=([0-9a-f]{8}) bytes=(\d+) '
            r'machine=014c sha256=([0-9a-f]{64}) unchanged=1 locked=1', 'complete unchanged readonly file identity required')
        require(value[1].casefold() == name and value[2] == view, 'file identity name/view reordered')
        size = int(value[8])
        require(0 < size <= 32 * 1024 * 1024 and size >= int(header[6]), 'file length/header boundary invalid')
        require(int(value[6], 16) or int(value[7], 16), 'complete original file index required')
        return {'input': value[3], 'final_nt': value[4], 'file_key': value.group(5, 6, 7, 8),
                'sha256': value[9], 'selected_PE': header}

    while index < len(selected):
        text = selected[index][1]
        if text.startswith('D3D8_SYSTEM_API_LOOKUP '):
            require(pending is None, 'duplicate or incomplete system API invocation')
            _, lookup = take(r'D3D8_SYSTEM_API_LOOKUP provider=kernel32\.dll process_present=1 process_error=0 '
                r'directory_present=([01]) directory_error=(\d+)', 'exact Kernel32 Process2/Directory2 lookup required')
            if lookup[1] == '0':
                require(lookup[2] == '127', 'alternate Directory2 provider requires original error127')
                take(r'D3D8_SYSTEM_API_LOOKUP provider=kernelbase\.dll directory_present=1 directory_error=0 exact_API_only=1',
                     'exact named KernelBase Directory2 provider required')
            else:
                require(lookup[2] == '0', 'successful original Directory2 lookup has wrong status')
            pending = {'machine': None, 'providers': []}
        elif text.startswith('D3D8_SYSTEM_PROCESS_ID '):
            require(pending is not None and pending['machine'] is None, 'process evidence lacks exact preceding API lookups')
            position, _ = take(r'D3D8_SYSTEM_PROCESS_ID process=014c native=aa64 pointer_bytes=4 api=IsWow64Process2 identity=1',
                               'exact actual I386/ARM64 process evidence required')
            pending['machine'] = position
            machines.append(position)
        elif text.startswith('D3D8_SYSTEM_PE '):
            name = re.fullmatch(r'D3D8_SYSTEM_PE name=(\S+) view=loaded .+', text)
            require(name and name[1].casefold() in SYSTEM_MODULES, 'exact known loaded system module required')
            name = name[1].casefold()
            require(machines and (pending is None or pending['machine'] is not None), 'mapped identity before actual process evidence')
            start, loaded = pe(name, 'loaded')
            logical, explicit = file(name, 'logical'), file(name, 'explicit-I386')
            require(logical['selected_PE'] == explicit['selected_PE'] == loaded, 'loaded/disk selected PE mismatch')
            require(logical['file_key'] == explicit['file_key'] and logical['sha256'] == explicit['sha256'],
                    'readonly file ID/length/hash mismatch')
            handles = []
            for view in ('explicit-I386', 'logical'):
                _, closed = take(r'D3D8_SYSTEM_FILE_CLOSE name=(\S+) view=(\S+) handle=((?:0x)?[0-9a-fA-F]+) status=1',
                                 'both explicit readonly handles must close before identity publication')
                require(closed[1].casefold() == name and closed[2] == view, 'original file close name/view reordered')
                handle = int(closed[3], 16)
                require(handle > 0 and handle not in handles, 'missing or duplicate simultaneous file handle')
                handles.append(handle)
            end, module = take(r'D3D8_SYSTEM_MODULE_ID name=(\S+) logical=(.+) explicit=(.+) mapped_nt=(.+) identity=1 machine=014c readonly=1',
                               'complete actual mapped module identity required')
            require(module[1].casefold() == name and module[2] == logical['input'] and module[3] == explicit['input'],
                    'loaded module and actual file input records differ')
            require(module[3].casefold() == (SYSTEM_DIRECTORY + chr(92) + name).casefold(),
                    'exact explicit I386 directory file view required')
            require(module[4].casefold() == logical['final_nt'].casefold() == explicit['final_nt'].casefold(),
                    'mapped NT filename differs from held file final NT filename')
            entry = {'name': name, 'logical': module[2], 'explicit': module[3], 'mapped_nt': module[4],
                     'file_key': logical['file_key'], 'sha256': logical['sha256'], 'selected_PE': loaded,
                     'closed_handles': handles, 'start_row': start, 'end_row': end}
            groups.append(entry)
            if pending is not None:
                require(name in ('kernel32.dll', 'kernelbase.dll') and name not in pending['providers'],
                        'API provider identity duplicated or unexpected')
                pending['providers'].append(name)
            else:
                require(directory is not None, 'non-provider identity before verified directory selection')
        elif text.startswith('D3D8_SYSTEM_DIRECTORY '):
            require(pending is not None and pending['machine'] is not None
                and pending['providers'] == ['kernel32.dll', 'kernelbase.dll'], 'actual API provider identities incomplete')
            position, value = take(r'D3D8_SYSTEM_DIRECTORY machine=014c api=GetSystemWow64Directory2W path=(.+)',
                                   'documented explicit I386 directory selection required')
            require(value[1].casefold() == SYSTEM_DIRECTORY.casefold(), 'original explicit I386 directory mismatch')
            directory = value[1]
            directories.append(position)
            pending = None
        else:
            require(False, 'unknown or reordered physical system identity record')
    require(pending is None and machines and directories and groups, 'complete system process/provider/module originals required')
    return {'modules': groups, 'process_rows': machines, 'directory_rows': directories,
            'module_count': len(groups), 'readonly_file_pairs': len(groups), 'closed_file_handles': len(groups) * 2}


def verify_names(text, luid, source):
    rows = lines(text)
    match_identity(rows, luid, source)
    identities = verify_module_identities(rows)
    require([row['name'] for row in identities['modules']] == ['kernel32.dll', 'kernelbase.dll', 'gdi32.dll']
        and len(identities['process_rows']) == len(identities['directory_rows']) == 1,
        'names phase requires exactly three actual loaded system module joins')
    gdi = single(rows, 'D3D8_SYSTEM_GDI32 ')
    paths = re.fullmatch(r'D3D8_SYSTEM_GDI32 actual=(.+) expected=(.+) machine=014c pointer_bytes=4 file_identity=1', gdi)
    loaded = identities['modules'][-1]
    require(paths and paths[1] == loaded['logical'] and paths[2] == loaded['explicit'],
            'GDI32 trace must match independently rejoined logical/explicit file identities')
    require(identities['process_rows'][0] < identities['directory_rows'][0] < loaded['end_row']
        < rows.index(gdi) < rows.index(single(rows, 'D3D8_KMT_MATCH ')), 'actual module identities must precede KMT matching')
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
    allowed = (*IDENTITY_PREFIXES, 'D3D8_USER_GATE ', 'D3D8_SYSTEM_GDI32 ',
               'D3D8_KMT_MATCH ', 'D3D8_KMT_NAME ', 'D3D8_KMT_NAME_WORD ', 'D3D8_KMT_CLOSED ', 'D3D8_KMT_NAMES_COMPLETE ')
    require(all(row.startswith(allowed) for row in rows), 'names phase emitted factory/core/unexpected output')
    return {'registered_I386_filename': actual, 'name_words': 260, 'query_bytes': 524,
            'process_machine': '014c', 'native_machine': 'aa64', 'system_directory_api': 'GetSystemWow64Directory2W',
            'actual_I386_GDI32_path': paths[1], 'expected_I386_GDI32_path': paths[2],
            'physical_module_identities': identities, 'create_device': False, 'core_loaded': False}


def verify_runtime_module_callers(rows):
    identities = verify_module_identities(rows)
    runtime_modules = [row for row in identities['modules'] if row['name'] == 'd3d8.dll']
    require(runtime_modules, 'actual genuine D3D8 mapped/file identity required')
    callers = []
    for position, row in enumerate(rows):
        if not row.startswith('SYSTEM_D3D8_CALLER_PATH '):
            continue
        caller = re.fullmatch(r'SYSTEM_D3D8_CALLER_PATH actual=(.+) expected=(.+) machine=014c pointer_bytes=4 '
                              r'directory_api=GetSystemWow64Directory2W file_identity=1', row)
        require(caller, 'complete actual Microsoft D3D8 caller file identity required')
        earlier = [value for value in runtime_modules if value['end_row'] < position]
        require(earlier and caller[1] == earlier[-1]['logical'] and caller[2] == earlier[-1]['explicit'],
                'actual caller differs from preceding complete mapped/file identity')
        callers.append({'row': position, 'logical': caller[1], 'explicit': caller[2],
                        'file_identity': earlier[-1]})
    require(callers, 'actual Microsoft D3D8 caller identity records absent')
    opens = [(position, re.fullmatch(r'SYSTEM_D3D8_OPEN_BEGIN interface=8 version=\d+ runtime=\S+ caller=(.+) '
             r'pointer_bytes=4 readonly=([01])', row)) for position, row in enumerate(rows)
             if row.startswith('SYSTEM_D3D8_OPEN_BEGIN ')]
    require(len(opens) == len(callers) and all(match for _, match in opens), 'complete actual Interface8 open/caller pairs required')
    for index, ((position, opened), caller) in enumerate(zip(opens, callers)):
        require(caller['row'] < position and opened[1] == caller['logical']
            and (index == 0 or opens[index - 1][0] < caller['row']), 'actual Interface8 caller/open identity reordered')
    return {'module_identities': identities, 'callers': callers, 'opens': [(position, opened[2]) for position, opened in opens]}


def verify_enumeration(text, luid='ec6b000000000000', source=0, core_identity=None, owned_icd_setup=False):
    """Actual HAL enumeration requires an internal driver device on system D3D8.

    This predicate permits real backend construction/initialization, including
    its reported Render callbacks. Public API device creation, runtime workload
    entry points and Present remain forbidden. It is not a pixel/hardware gate.
    """
    rows = lines(text)
    runtime_identity = verify_runtime_module_callers(rows)
    require(all(readonly == '0' for _, readonly in runtime_identity['opens']),
            'enumeration must use the immutable constructor permission, not old readonly mode')
    require(not re.search(r'^(?:D3D8_(?:ERROR|FAILED|UNAVAILABLE)\b|SYSTEM_D3D8_(?:CREATE_BLOCKED|DEVICE_DESTROY_FAILED|ENUMERATION_FORBIDDEN|ENUMERATION_CONTRACT_REJECTED|ENUMERATION_PAYLOADS_FAILED)\b)', text, re.M),
            'enumeration reported a failed or forbidden operation')
    match_identity(rows, luid, source)
    runtime = re.fullmatch(r'D3D8_RUNTIME path=(.+) machine=014c pointer_bytes=4 sdk_version=220 caps_bytes=212',
                           single(rows, 'D3D8_RUNTIME '))
    modules = [row for row in runtime_identity['module_identities']['modules'] if row['name'] == 'd3d8.dll']
    require(runtime and all(runtime[1].casefold() == row['explicit'].casefold() for row in modules),
            'genuine system8 runtime path must match rejoined explicit I386 files')
    require(single(rows, 'D3D8_ENUMERATION_CONSTRUCTION ')
        == 'D3D8_ENUMERATION_CONSTRUCTION allowed=1 public_create_device=0 draw=0 presents=0 core_entry=OpenAdapter',
        'explicit constructor-only scope missing')
    adapters = re.findall(r'^D3D8_ADAPTER index=\d+ identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=([0-9a-f]{8}) vs=fffe0101 ps=ffff0104 constants=96$', text, re.M)
    require(len(adapters) == 1 and int(adapters[0], 16) & 0x90000 == 0x90000,
            'actual bounded Interface8 HAL caps required')
    core_identity = core_identity or {'source': CORE_SOURCE, 'run': RUN, 'sha256': PAYLOADS['core'][1]}
    require(set(core_identity) == {'source', 'run', 'sha256'}
        and re.fullmatch(r'[0-9a-f]{40}', core_identity['source'])
        and re.fullmatch(r'[0-9a-f]{64}', core_identity['sha256'])
        and type(core_identity['run']) is int and core_identity['run'] > 0, 'exact independently admitted core tuple required')
    core_source, core_run, core_sha = (core_identity[key] for key in ('source', 'run', 'sha256'))
    require(type(owned_icd_setup) is bool, 'explicit owned ICD setup scope required')
    suffix = '-icd02' if owned_icd_setup else ''
    expected_core = rf'C:\Users\Public\DxvkD3D8Candidate-{core_source[:7]}-{core_run}{suffix}\viogpudxvk.dll'
    opened = re.findall(r'^SYSTEM_D3D8_OPEN_END hr=00000000 interface=8 driver_version=12 adapter=(\S+) core=(.+) expected_ci_source_commit=([0-9a-f]{40}) machine=014c core_create_calls=0$', text, re.M)
    require(len(opened) == len(runtime_identity['opens']) and all(commit == core_source and path == expected_core for _, path, commit in opened),
            'actual standard typed adapter admission failed')
    modes = re.findall(r'^SYSTEM_D3D8_ENUMERATION_MODE adapter=(\S+) version=(\d+) captured_mode=3 core_entry=OpenAdapter render_permission=0$', text, re.M)
    require(len(modes) == len(opened) and {row[0] for row in modes} == {row[0] for row in opened},
            'immutable enumeration adapter modes missing')
    versions = dict(modes)
    require(len(versions) == len(modes), 'duplicate live adapter identity')
    require(single(rows, 'SYSTEM_D3D8_CORE_PIN ') == f'SYSTEM_D3D8_CORE_PIN path={expected_core} sha256={core_sha} expected_ci_source_commit={core_source} machine=014c file_locked=1 core_unchanged=1',
            'original locked I386 core pin mismatch')
    folder = expected_core.rsplit(chr(92), 1)[0] + chr(92)
    pins = re.findall(r'^D3D8_PAYLOAD_PIN path=(.+) sha256=([0-9a-f]{64}) machine=(014c|json) locked=1 original_bytes=1$', text, re.M)
    expected_pins = {(folder + name, core_sha if name == 'viogpudxvk.dll' else digest_value,
                      'json' if name.endswith('.json') else '014c') for _, digest_value, name in PAYLOADS.values()}
    require(len(pins) == 4 and set(pins) == expected_pins, 'exact four original I386 private payload pins required')
    derived = [row for row in rows if row.startswith('D3D8_DERIVED_ICD_PIN ')]
    if owned_icd_setup:
        expected_derived = (f'D3D8_DERIVED_ICD_PIN path={folder}freedreno_icd_owned_x86.json '
            f'sha256={OWNED_ICD_SHA} original_sha256={PAYLOADS["icd-json"][1]} locked=1 original_bytes=0 '
            'library_path=.' + chr(92) + 'viogpu_gl_vk_x86.dll only_library_path_changed=1')
        require(derived == [expected_derived], 'exact separately derived module-relative ICD pin required')
        require(rows.index(expected_derived) < min(position for position, _ in runtime_identity['opens']),
                'derived ICD must be locked before internal backend construction')
    else:
        require(not derived, 'derived ICD requires an explicitly admitted setup')
    private = re.findall(r'^D3D8_PRIVATE_MODULE name=(\S+) path=(.+) machine=014c$', text, re.M)
    expected_private = {(name, folder + name) for name in
        ('viogpudxvk.dll', 'viogpu_gl_loader_x86.dll', 'viogpu_gl_vk_x86.dll')}
    require(set(private) == expected_private, 'actual three private I386 modules missing')
    require(single(rows, 'D3D8_HARDWARE_SOURCE ') == f'D3D8_HARDWARE_SOURCE core_commit={core_source} ci_run={core_run} loader_source=6a6878c614c8c6dbe81ee7a9f1176bdb52dc7dd7 icd_source=8443c71a5ab32b9d58b904fa51f4bf2f9089db8d icd_ci_run=37453381660 driver_selection=owned-json raw_architecture=014c',
        'actual independently pinned core/loader/ICD sources differ')
    contracts = re.findall(r'^SYSTEM_D3D8_CREATE_CONTRACT adapter=(\S+) runtime=(\S+) interface=8 version=(\d+) flags=00000000 callbacks=1 functions=1 command=\d+ allocation_list=\d+ patch_list=\d+ captured_mode=3$', text, re.M)
    require(len(contracts) == sum(row.startswith('SYSTEM_D3D8_CREATE_CONTRACT ') for row in rows)
        and contracts and all(adapter in versions and version == versions[adapter] for adapter, _, version in contracts),
            'internal CreateDevice differs from captured Interface8/version/flags0 contract')
    created = re.findall(r'^SYSTEM_D3D8_CREATE_RETURN runtime=(\S+) driver=(\S+) hr=00000000 interface=8 core_create_calls=1$', text, re.M)
    require(len(created) == sum(row.startswith('SYSTEM_D3D8_CREATE_RETURN ') for row in rows)
        and len(created) == len(contracts) and [row[0] for row in created] == [row[1] for row in contracts]
        and len(set(created)) == len(created) and all(value not in ('0', '00000000', '(nil)') for row in created for value in row),
        'actual internal core construction failed or duplicate identities reported')
    require(len(private) == 3 * len(created) and all(private.count(value) == len(created) for value in expected_private),
        'actual private module observations must be captured for every live internal device')
    private_owners = re.findall(r'^SYSTEM_D3D8_ENUMERATION_PRIVATE_PAYLOADS device=(\S+) checked_after_create=1 machine=014c forbidden_loader=0$', text, re.M)
    require(private_owners == [row[1] for row in created], 'live internal device private module identity missing')
    tables = re.findall(r'^SYSTEM_D3D8_CALLBACK_TABLE runtime=(\S+) adapter_runtime=\S+ original=(\S+) wrapped=(\S+) bytes=88 owned_snapshot=1 borrowed_table_reread=0$', text, re.M)
    require(len(tables) == len(created) and [row[0] for row in tables] == [row[0] for row in created]
        and all(original != wrapped and original not in ('0', '00000000', '(nil)') and wrapped not in ('0', '00000000', '(nil)') for _, original, wrapped in tables),
        'actual immutable callback owner snapshot missing')
    presence = re.findall(r'^SYSTEM_D3D8_CALLBACK_PRESENCE runtime=(\S+) allocate=1 deallocate=1 lock=1 unlock=1 create_context=1 destroy_context=1 escape=1 render=1 present=[01] residency=[01]$', text, re.M)
    require(presence == [row[0] for row in created], 'required original runtime callbacks missing')
    require(rows.count('SYSTEM_D3D8_DEVICE_FUNCTIONS bytes=396 interface=12 published=1') == len(created),
            'actual protected Vista99 function publication missing')
    boundary = re.findall(r'^SYSTEM_D3D8_ENUMERATION_BOUNDARY device=(\S+) blocked_mask=([0-9a-f]{8}) bytes=396 public_create_device=0 draw_forwarding=0 present_forwarding=0$', text, re.M)
    require(len(boundary) == len(created) and [row[0] for row in boundary] == [row[1] for row in created]
        and all(int(mask, 16) & 0x4203 == 0x4203 and not int(mask, 16) & ~0x7fff for _, mask in boundary),
        'typed runtime draw/Clear/Present deny boundary missing')
    caps = re.findall(r'^SYSTEM_D3D8_CAPS12 id=(\d+) bytes=212 device_type=1 devcaps=([0-9a-f]{8}) caps2=[0-9a-f]{8} primitive=([0-9a-f]{8}) vs=fffe0101 constants=96 ps=ffff0104$', text, re.M)
    require(caps, 'actual unchanged CAPS12 forwarding missing')
    for ident, devcaps, primitive in caps:
        require(int(devcaps, 16) & 0x90000 == 0x90000 and int(primitive, 16) & 0x2000,
                'HAL/legacy-fog original projection mismatch')
        words = re.findall(rf'^SYSTEM_D3D8_CAPS12_WORD id={ident} index=(\d+) value=([0-9a-f]{{8}})$', text, re.M)
        require([int(index) for index, _ in words] == list(range(53)), 'all53 CAPS12 words required')
        require(int(words[0][1], 16) == 1 and int(words[7][1], 16) == int(devcaps, 16)
            and int(words[8][1], 16) == int(primitive, 16) and int(words[49][1], 16) == 0xfffe0101
            and int(words[50][1], 16) == 96 and int(words[51][1], 16) == 0xffff0104,
            'original CAPS12 output words mismatch')
    begins = re.findall(r'^SYSTEM_D3D8_CAPS_BEGIN id=(\d+) interface=8 type=(\d+) bytes=(\d+) info=[01] adapter=\S+$', text, re.M)
    completed = re.findall(r'^SYSTEM_D3D8_CAPS_END id=(\d+) type=(\d+) bytes=(\d+) hr=00000000 caps_modified=0$', text, re.M)
    require(sorted(begins) == sorted(completed) and len({row[0] for row in begins}) == len(begins),
            'actual unchanged caps call/return pairs missing or failed')
    lifetimes = re.findall(r'^SYSTEM_D3D8_LIFETIME phase=destroyed runtime=(\S+) allocate=(\d+) deallocate=(\d+) lock=(\d+) unlock=(\d+) create_context=(\d+) destroy_context=(\d+) render=(\d+) present=0 residency=(\d+) live_allocations=0 live_locks=0 live_contexts=0 tracking_errors=0 callback_failures=0$', text, re.M)
    require(len(lifetimes) == len(created) and sorted(row[0] for row in lifetimes) == sorted(row[0] for row in created),
            'actual internal device callback teardown incomplete')
    render_callbacks = 0
    for _, allocate, deallocate, locked, unlocked, context, destroyed, render, _ in lifetimes:
        require(allocate == deallocate and locked == unlocked and context == destroyed and int(context) > 0,
                'actual internal callback owners unbalanced')
        render_callbacks += int(render)
    destruction_begin = re.findall(r'^SYSTEM_D3D8_DEVICE_DESTROY_BEGIN device=(\S+) callback_owner_live=1$', text, re.M)
    destruction_end = re.findall(r'^SYSTEM_D3D8_DEVICE_DESTROY device=(\S+) hr=00000000 remaining=\d+ callback_owner_released=1$', text, re.M)
    require(sorted(destruction_begin) == sorted(destruction_end) == sorted(row[1] for row in created),
            'actual core teardown/callback lifetime pair mismatch')
    require(not any(row.startswith(('D3D8_PIXEL ', 'D3D8_SCREEN_PIXEL ', 'D3D8_SELECTED_OFFSCREEN ',
        'SYSTEM_D3D8_PRESENT_BEGIN ', 'SYSTEM_D3D8_PRESENT_END ', 'D3D8_API operation=CreateDevice')) for row in rows),
        'enumeration emitted a public device/render/Present workload')
    require([row for row in rows if row.startswith('D3D8_API ')]
        == ['D3D8_API operation=Direct3DCreate8 object=1'],
            'actual genuine system factory missing')
    require(len(re.findall(r'^D3D8_SELECTOR installed=1 machine=014c pointer_bytes=4 slot_rva=[0-9a-f]+ registry_writes=0$', text, re.M)) == 1,
            'exact owned system8 selector missing')
    restored = re.fullmatch(r'D3D8_SELECTOR restored=1 protection_restored=1 substitutions=([1-9]\d*) queries=([1-9]\d*)', single(rows, 'D3D8_SELECTOR restored='))
    require(restored and int(restored[2]) >= int(restored[1]), 'original system IAT restoration failed')
    closed_adapters = re.findall(r'^SYSTEM_D3D8_CLOSE adapter=\S+ runtime=\S+ hr=00000000 remaining=\d+ live_devices=0$', text, re.M)
    require(len(closed_adapters) == len(opened) and 'remaining=0 live_devices=0' in closed_adapters[-1],
            'adapter owner remained live')
    require(re.fullmatch(r'D3D8_COMPLETE mode=front-enumerate adapters=[1-9]\d* create_device=0 presents=0 registry_writes=0', single(rows, 'D3D8_COMPLETE ')),
            'enumeration public API scope mismatch')
    require(single(rows, 'D3D8_ENUMERATION_COMPLETE ')
        == 'D3D8_ENUMERATION_COMPLETE internal_driver_construction=1 public_create_device=0 draw=0 presents=0 selector_restored=1 environment_restored=1',
        'closed internal-construction enumeration completion missing')
    return {'HAL_caps': True, 'interface': 8, 'caps_bytes': 212, 'core_loaded': True,
            'public_API_CreateDevice': False, 'runtime_workload_forwarded': False, 'Present': False,
            'internal_core_devices': len(created), 'actual_backend_initialization_Render_callbacks': render_callbacks,
            'callback_owner_teardowns': len(destruction_end), 'physical_runtime_identities': runtime_identity}


def verify_native_identity_cpu(result, members, completion_members=None, posthash=None):
    """Join the actual closed CPU09 build and separately executed remaining CPU fixtures."""
    require(result.get('schema') == 'native-system-d3d8-device-x86-build-v1' and result.get('status') == 'FAIL'
        and result.get('source_commit') == PROBE_SOURCE and result.get('core_reference_commit') == CORE_SOURCE,
        'exact original CPU09 build and current source required')
    require(completion_members is not None and posthash is not None,
            'separate actual completion originals and original readonly posthash required')
    require(digest(members['result.json']) == '585aa34424eafd4eada70ae5628cfd62686f2e30b37d8e57a05c94bd8c2788b2'
        and read_json(members['result.json']) == result, 'exact immutable actual CPU09 build receipt required')
    require(result['error'] == 'Exact original I386 API-provider diagnostic missing: (?m)^D3D8_PROCESS_MACHINE process=014c native=aa64 effective=014c pointer_bytes=4 ',
            'CPU09 failure was not the isolated obsolete marker')
    c = read_json(completion_members['result.json'])
    require(c['schema'] == 'native-system-d3d8-device-x86-completion-v1' and c['status'] == 'PASS'
        and c['source_commit'] == PROBE_SOURCE and c['marker_helper_commit'] == 'e5bfaf2100380151be9d5d974ebb3692f2b4ed95'
        and not c.get('error') and not c.get('final_capture_error'), 'actual current-source CPU completion failed')
    require(completion_members['original-failed-result.json'] == members['result.json']
        and read_json(completion_members['original-posthash.json']) == posthash
        and digest(completion_members['original-posthash.json']) == 'd9e37a95907f0274ca4b962abba0e6b5052fcd96ea60a69e3a781ba0f74584eb',
        'actual original build/posthash chain differs')
    require(posthash['status'] == 'PASS' and posthash['original_result_sha256'] == digest(members['result.json'])
        and c['original_result_after']['sha256'] == digest(members['result.json']), 'failed original was mutated or not retained')
    require(result['before'] == result['failure_state'] == posthash['state_after'] == c['before'] == c['after'],
            'actual protected native state changed')

    def facts(rows):
        return [(row['path'].replace('\\\\', '\\').casefold(), row['bytes'], row['sha256']) for row in rows]

    for old, observed, new, count in [('source_before', 'source_after', 'source', 35),
            ('compiler_full_before', 'compiler_after', 'compiler', 575), ('sdk', 'sdk_after', 'sdk', 13),
            ('libraries', 'libraries_after', 'libraries', 9)]:
        base = facts(result[old])
        require(len(base) == count and len({row[0] for row in base}) == count
            and base == facts(posthash[observed]) == facts(c[new + '_before']) == facts(c[new + '_after']),
            'actual CPU09 source/tool/SDK/library continuity differs: ' + new)
    source_rows = {row['input_path']: row for row in result['source_before']}
    require(len(source_rows) == 35, 'complete exact CPU09 input set required')
    for path, row in source_rows.items():
        name = PurePosixPath(path)
        require(not name.is_absolute() and '..' not in name.parts, 'unsafe CPU09 input member')
        data = members['source/' + str(name)]
        require(len(data) == row['bytes'] and digest(data) == row['sha256'], 'actual CPU09 retained source differs')
    for key, prefix in [('sdk', 'original-sdk-headers/'), ('libraries', 'original-link-libraries/')]:
        seen = set()
        for row in result[key]:
            name = row['path'].rsplit(chr(92), 1)[-1]
            require(name not in seen, 'duplicate actual CPU09 native dependency')
            seen.add(name)
            data = members[prefix + name]
            require(len(data) == row['bytes'] and digest(data) == row['sha256'], 'actual selected native dependency differs')
    require(digest(members['original-compiler-provenance.json']) ==
        digest(completion_members['original-compiler-provenance.json']) ==
        result['compiler_provenance']['sha256'] == c['compiler_ready_before']['sha256'] ==
        'c6333c67b4f3725f513e82c488b844054859b28456bccb0131eb59b805a5db48', 'actual575 original compiler readiness differs')
    ready = read_json(members['original-compiler-provenance.json'])
    require(ready['ready'] and ready['file_count'] == len(ready['files']) == 575
        and facts(ready['files']) == facts(result['compiler_full_before']), 'complete575 actual owned compiler files differ')
    for native in (result, c):
        require(not native['installation'] and native['gpu_runs'] == native['system_runtime_calls'] == native['selector_calls'] == 0,
                'CPU scope cannot grant runtime/hardware admission')
        require(native['raw_process_source']['sha256'] == RAW, 'actual native owned runner differs')
    require(c['compiler_launches'] == c['readonly_diagnostic_replays'] == c['valid_runtime_modes_executed'] == 0
        and not c['production_core_loaded'] and c['originals_unchanged']
        and c['raw_process_source_after']['sha256'] == RAW
        and digest(completion_members['executed-raw-process-source.cs']) == RAW
        and digest(members['executed-raw-process-source.cs']) == RAW, 'completion replay/permission/runner differs')
    require(len(result['outputs']) == 9 and facts(result['outputs']) == facts(c['outputs_before']) == facts(c['outputs_after']),
            'exact original five COFF/four PE continuity differs')
    objects = pes = 0
    for row in result['outputs']:
        path = row['path'].replace('\\\\', '\\')
        relative = path.split('DxvkD3D8Runtime-e3ac126-09' + chr(92), 1)[1].replace(chr(92), '/')
        data = members[relative]
        require(data == completion_members['original-reused-outputs/' + relative.rsplit('/', 1)[-1]]
            and len(data) == row['bytes'] and digest(data) == row['sha256'] and row['machine'] == 0x14c,
            'original reused compiled output differs')
        if relative.endswith('.obj'):
            machine = int.from_bytes(data[6:8], 'little') if data[:4] == b'\0\0\xff\xff' else int.from_bytes(data[:2], 'little')
            objects += 1
        else:
            offset = int.from_bytes(data[60:64], 'little')
            require(data[:2] == b'MZ' and data[offset:offset + 4] == b'PE\0\0', 'original native PE header differs')
            machine = int.from_bytes(data[offset + 4:offset + 6], 'little'); pes += 1
        require(machine == 0x14c, 'actual compiled CPU09 output is not I386')
    require((objects, pes) == (5, 4), 'five original COFF/four original PE required')
    expected = ['archive-list', 'extract']
    for group, units in [('front', ['tests_umd-d3d8-runtime-front.cpp']),
            ('probe', ['tests_umd-d3d8-runtime-probe.cpp', 'tests_umd-d3d8-runtime-guard.cpp']),
            ('policy', ['tests_umd-d3d8-runtime-policy.cpp']), ('callbacks', ['tests_umd-d3d8-runtime-callbacks.cpp'])]:
        expected += [group + '-' + unit + '-compile' for unit in units] + [group + '-link', group + '-headers']
    expected += ['process-api-diagnostics']
    require([row['name'] for row in result['commands']] == expected, 'exact16 original construction children required')
    fixture_names = ['policy-positive', 'callbacks-positive', 'frontend-null-invalid-guard'] + ['invalid-cli-' + str(i) for i in range(1, 33)]
    require([row['name'] for row in c['commands']] == fixture_names and c['policy_checks'] == 503
        and c['malformed_cli_guards'] == 32, 'exact35 unexecuted CPU fixtures required')
    cli = ['', '--unknown', '--enumerate extra', '--offscreen extra', '--front-guard', '--front-guard one two',
        '--front-guard one two three', '--front-enumerate', '--front-enumerate missing', '--front-enumerate one',
        '--front-enumerate one two extra', '--front-enumerate one two three four',
        '--front-enumerate one two three four five', '--front-enumerate one two three four five extra',
        '--kmt-names', '--kmt-names extra', '--kmt-names ec6b000000000000', '--kmt-names not-luid 0',
        '--kmt-names ec6b000000000000 bad-source', '--kmt-names ec6b000000000000 0 extra',
        '--front-offscreen', '--front-offscreen missing', '--front-offscreen one two three four five six',
        '--front-offscreen one two three four five six seven', '--front-offscreen one two three four five six seven extra',
        '--front-present', '--front-present missing', '--front-present one two three four five six',
        '--front-present one two three four five six seven', '--front-present one two three four five six seven extra',
        '--process-api-diagnostics extra', '--process-api-diagnostics one two']
    for receipt, contents in ((result, members), (c, completion_members)):
        for command in receipt['commands']:
            name = command['name']; stdout = contents[name + '.stdout.txt']; stderr = contents[name + '.stderr.txt']
            require(command['raw_pipe_bytes'] and command['stdout'].rsplit(chr(92), 1)[-1] == name + '.stdout.txt'
                and command['stderr'].rsplit(chr(92), 1)[-1] == name + '.stderr.txt', 'actual owned native pipe member differs')
            raw = {'Pid': command['pid'], 'ProcessHandle': command['retained_process_handle'], 'Exited': command['exited'],
                'ExitCodeAvailable': command['exit_code_available'], 'PipesDrained': command['pipes_drained'], 'TimedOut': command['timeout'],
                'ChildStillRunning': command['child_still_running'], 'Failure': command['capture_failure'], 'ExitCode': command['exit'],
                'StdoutBytes': command['stdout_bytes'], 'StderrBytes': command['stderr_bytes'], 'Seconds': command['seconds'], 'StartUtc': command['start_utc']}
            expected_exit = 64 if name.startswith('invalid-cli-') else 0
            require(command['expected'] == expected_exit, 'actual child exit contract differs')
            deadline = 180 if name.endswith('-compile') else 120 if name.endswith('-link') else 15 if name.startswith('invalid-cli-') else 30
            require(command['deadline_seconds'] == deadline, 'actual child deadline differs')
            closed(raw, stdout, stderr, deadline * 1000, expected_exit)
            if name.endswith('-compile'):
                require(command['first_party_strict'] and not command['compiler_warnings'] and not re.search(rb'\bwarning [CD]\d+', stdout + stderr, re.I), 'actual strict compilation warning')
            if name.startswith('invalid-cli-'):
                require(command['arguments'] == cli[int(name.rsplit('-', 1)[1]) - 1], 'actual malformed CLI changed')
    require(re.search(rb'^D3D8 runtime selector policy PASS checks=503;', completion_members['policy-positive.stdout.txt'], re.M), 'actual503 CPU policy missing')
    callback = completion_members['callbacks-positive.stdout.txt'].decode('utf-8').replace('\r\n', '\n')
    match = re.search(r'^D3D8 runtime callback policy PASS checks=(\d+) forwarded=11 immutable_table=1 mapped_submit=1 failed_release_retained=1 stale_owner_rejected=1; controlled CPU only$', callback, re.M)
    require(match and c['callback_checks'] == int(match[1]), 'actual CPU callback ownership missing')
    require('D3D8 enumeration table boundary PASS denied=6 forwarded=0 prefix=99 tail_unchanged=1 optional_null=1 teardown_allowed=1; controlled CPU only' in callback.splitlines(), 'actual six typed enumeration denial fixture missing')
    require(re.search(rb'^D3D8_FRONT_GUARD PASS .*invalid_interfaces=6 non_system_caller=1 no_core_open=1 system_runtime_calls=0 device_permission_null=80070057 enumeration_permission_null=80070057\r?$', completion_members['frontend-null-invalid-guard.stdout.txt'], re.M), 'actual frontend guard completion missing')
    require(completion_members['original-process-api-diagnostics.stdout.raw'] == members['process-api-diagnostics.stdout.txt'], 'readonly observation was not retained exactly')
    identity = verify_module_identities(lines(members['process-api-diagnostics.stdout.txt'].decode('utf-8')))
    require([x['name'] for x in identity['modules']] == ['kernel32.dll', 'kernelbase.dll', 'gdi32.dll'], 'actual physical three-module identity differs')
    descriptor = read_json(completion_members['completion-manifest-10.json'])
    require(descriptor['source_commit'] == PROBE_SOURCE and descriptor['old_result_sha256'] == digest(members['result.json'])
        and descriptor['expected_remaining_children'] == 35, 'actual completion scope differs')
    for member, name in [('executed-completion-helper.ps1', 'complete-native-d3d8-CPU09.ps1'),
            ('invoke-native-d3d8-completion.ps1', 'invoke-native-d3d8-completion.ps1')]:
        rows = [x for x in descriptor['inputs'] if x['name'] == name]
        require(len(rows) == 1 and digest(completion_members[member]) == rows[0]['sha256'], 'actual completion helper differs')
    parent = read_json(completion_members['completion-parent.process.json'])
    raw = {'Pid': parent['pid'], 'ProcessHandle': parent['retained_process_handle'], 'Exited': parent['exited'],
        'ExitCodeAvailable': parent['exit_code_available'], 'PipesDrained': parent['pipes_drained'], 'TimedOut': parent['timeout'],
        'ChildStillRunning': parent['child_still_running'], 'Failure': parent['capture_failure'], 'ExitCode': parent['exit'],
        'StdoutBytes': parent['stdout_bytes'], 'StderrBytes': parent['stderr_bytes'], 'Seconds': parent['seconds'], 'StartUtc': parent['start_utc']}
    require(parent['runner_sha256'] == RAW and parent['deadline_ms'] == 1100000, 'actual native completion parent differs')
    closed(raw, completion_members['completion-parent.stdout.raw'], completion_members['completion-parent.stderr.raw'], 1100000)
    return {'objects': 5, 'PEs': 4, 'policy_checks': 503, 'callback_checks': c['callback_checks'], 'typed_denials': 6,
            'malformed_CLI': 32, 'owned_build_children': 16, 'owned_completion_children': 35, 'owned_completion_parent': 1,
            'original_module_identities': identity, 'readonly_diagnostic_replays': 0, 'compiler_replays': 0,
            'runtime_factories': 0, 'KMT_calls': 0, 'core_loads': 0, 'hardware_admission': False}


def read_native_archive(path):
    members = {}
    with tarfile.open(path, 'r:gz') as original:
        for member in original:
            if member.isdir():
                continue
            name = PurePosixPath(member.name.removeprefix('./'))
            require(member.isfile() and not name.is_absolute() and '..' not in name.parts and str(name) not in members,
                    'unsafe or duplicate native original member')
            members[str(name)] = original.extractfile(member).read()
    return members


def verify_owned_icd_native_setup(setup):
    """Reopen the admitted changed build, preserving its overall observer failure."""
    require(setup['accepted'] and setup['source'] == SETUP_SOURCE, 'changed native setup is not admitted')
    fixed = {
        'archive': (29528581, '1b6a4f3b6065de7c0a74a488719ff9948179e3edfedef156b5ba4b31a2ab523a'),
        'proof': (138185, '10c8b7d10a6182afb4e009a01902c0fb4438fa1d60c7d89fb58d1cd029eb4973'),
        'reader': (23817, 'eeac203bd113835fb03b8f178b509d74bb3416e58da904db62ee1c73b376bccd'),
        'ROOT_admission': (1971, 'fa9f189f227a375cd4a190fda4b398b1379698ad87d8243a804195755e31089c'),
    }
    for key, (size, sha) in fixed.items():
        row = setup[key]
        data = Path(row['path']).read_bytes()
        require(row['bytes'] == len(data) == size and row['sha256'] == digest(data) == sha,
                'actual changed native setup original differs: ' + key)
    root = read_json(Path(setup['ROOT_admission']['path']).read_bytes())
    require(root['root_originals_direct_review'] and root['native_build_scope_accepted']
        and root['source_commit'] == SETUP_SOURCE and root['new_guard_dependency_admitted']
        and root['overall_attempt_status'] == 'FAIL' and root['overall_attempt_exit'] == 1
        and root['original_posthash_status'] == 'FAIL' and root['actual_full_state_before_after_post_equal']
        and root['protected_static34_equal'] and not root['HAL_admitted'], 'ROOT changed-build scope mismatch')
    require(root['original_archive'] == setup['archive'] and root['original_native_scope_review'] == setup['proof']
        and root['independent_reader'] == setup['reader'], 'ROOT changed-build original joins differ')
    attempt = Path(setup['attempt_directory'])
    require(Path(setup['archive']['path']).parent == attempt and Path(setup['proof']['path']).parent == attempt,
            'changed native originals must belong to the same actual attempt')
    outer = setup['outer']
    require(digest(Path(outer['path']).read_bytes()) == outer['sha256']
        and len(Path(outer['path']).read_bytes()) == outer['bytes'], 'actual changed native outer differs')
    spec = importlib.util.spec_from_file_location('frozen_changed_native_originals', Path(setup['reader']['path']))
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    canonical = reader.review(attempt, Path(outer['path']))
    proof = read_json(Path(setup['proof']['path']).read_bytes())
    require(canonical == proof and proof['native_build_accepted'] and proof['new_guard_dependency_native_admitted']
        and proof['base_source_commit'] == PROBE_SOURCE and proof['base_35_fixture_replays'] == 0,
        'actual changed native binary/fixture originals failed canonical review')
    members = read_native_archive(Path(setup['archive']['path']))
    return proof, read_json(members['result.json']), members


def verify_frontend_lifetime_native_build(lifetime, setup):
    """Admit changed probe/frontend bytes separately from the unchanged guard base."""
    require(lifetime['accepted'] and lifetime['source'] == LIFETIME_SOURCE
        and setup['accepted'] and setup['source'] == SETUP_SOURCE, 'separate lifetime/setup admission required')
    for name in ('archive', 'proof', 'reader', 'ROOT_admission', 'outer'):
        row = lifetime[name]; data = Path(row['path']).read_bytes()
        require(len(data) == row['bytes'] and digest(data) == row['sha256'], 'lifetime original pin differs: ' + name)
    require(lifetime['reader']['bytes'] == 19299 and lifetime['reader']['sha256'] == LIFETIME_READER,
            'exact frozen lifetime original reader required')
    root = read_json(Path(lifetime['ROOT_admission']['path']).read_bytes())
    require(root['root_originals_direct_review'] and root['native_build_scope_accepted']
        and root['source_commit'] == LIFETIME_SOURCE and root['new_COFF'] == root['new_PE'] == 2
        and root['unchanged_guard_source_commit'] == SETUP_SOURCE and root['current_full_state_before_after_post_equal']
        and root['protected_static34_equal'] and not root['HAL_admitted'], 'ROOT lifetime build scope mismatch')
    for key, original in (('original_archive', 'archive'), ('original_native_scope_review', 'proof'),
                          ('independent_reader', 'reader'), ('outer', 'outer')):
        require(root[key] == lifetime[original], 'ROOT lifetime original join differs: ' + key)
    attempt = Path(lifetime['attempt_directory'])
    require(Path(lifetime['archive']['path']).parent == attempt and Path(lifetime['proof']['path']).parent == attempt,
            'lifetime originals must belong to one actual attempt')
    spec = importlib.util.spec_from_file_location('frozen_lifetime_native_originals', Path(lifetime['reader']['path']))
    reader = importlib.util.module_from_spec(spec); spec.loader.exec_module(reader)
    canonical = reader.review(attempt, Path(lifetime['outer']['path']))
    proof = read_json(Path(lifetime['proof']['path']).read_bytes())
    require(canonical == proof and proof['verified'] and proof['native_build_accepted']
        and proof['overall_attempt_status'] == 'PASS' and proof['source_commit'] == LIFETIME_SOURCE
        and proof['guard_base_source_commit'] == SETUP_SOURCE and proof['guard_original_overall_attempt_status'] == 'FAIL'
        and proof['new_COFF'] == proof['new_PE'] == 2 and proof['actual_build_children_closed'] == 14
        and proof['total_native_children_closed'] == 17 and proof['policy_fixture_replays'] == proof['base_35_fixture_replays'] == 0
        and proof['unchanged_guard_reused']['original_archive'] == setup['archive']
        and proof['unchanged_guard_reused']['ROOT_build_scope'] == setup['ROOT_admission'],
        'actual lifetime binary/fixture/source/guard canonical review failed')
    members = read_native_archive(Path(lifetime['archive']['path']))
    return proof, read_json(members['result.json']), members


def verify_runtime_stdout(text, phase, luid, source, core_identity, owned_icd_setup=False, frontend_path=None):
    """Dispatch only with the core tuple already joined by the archive reader."""
    require(isinstance(core_identity, dict) and set(core_identity) == {'source', 'run', 'sha256'},
            'explicit joined runtime core identity required')
    spec = importlib.util.spec_from_file_location('pixels', Path(__file__).with_name('verify-native-d3d8-system-device.py'))
    pixels = importlib.util.module_from_spec(spec); spec.loader.exec_module(pixels)
    if frontend_path is not None:
        require(owned_icd_setup, 'new frontend lifetime requires explicit owned ICD setup')
    if phase == 'enumerate':
        detail = verify_enumeration(text, luid, source, core_identity=core_identity,
                                   owned_icd_setup=owned_icd_setup)
        if pixels.verify_frontend_reference(lines(text), frontend_path):
            detail['frontend_owned_through_runtime_teardown'] = True
        return detail
    require(phase in ('offscreen', 'present'), 'runtime phase required')
    runtime_identity = verify_runtime_module_callers(lines(text))
    detail = pixels.verify(text, 'front-' + phase, luid, source, core_identity=core_identity,
                           owned_icd_setup=owned_icd_setup, frontend_path=frontend_path)
    detail['physical_runtime_identities'] = runtime_identity
    return detail


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
    owned_setup = 'native_setup' in manifest
    held_frontend = 'native_lifetime' in manifest
    require(not held_frontend or owned_setup, 'frontend lifetime requires original owned ICD setup')
    probe_source = LIFETIME_SOURCE if held_frontend else SETUP_SOURCE if owned_setup else PROBE_SOURCE
    require(manifest['probe_source'] == probe_source and manifest['native_cpu']['source'] == PROBE_SOURCE
        and manifest['core_source'] == CORE_SOURCE and manifest['core_ci_run'] == RUN, 'separate base/setup/core source identity mismatch')
    require(manifest['loader_source'] == '6a6878c614c8c6dbe81ee7a9f1176bdb52dc7dd7' and manifest['icd_source'] == '8443c71a5ab32b9d58b904fa51f4bf2f9089db8d', 'distinct original loader/ICD source mismatch')
    require(manifest['adapter_luid'] == 'ec6b000000000000' and manifest['source_id'] == 0, 'fresh selected adapter identity mismatch')
    require(len(manifest['helpers']) == len(HELPERS) and {row['name'] for row in manifest['helpers']} == HELPERS, 'exact frozen helper set required')
    native = manifest['native_cpu']
    native_proof_bytes = Path(native['original_proof_path']).read_bytes()
    native_archive_bytes = Path(native['original_archive_path']).read_bytes()
    require(digest(native_proof_bytes) == native['original_proof_sha256'] and digest(native_archive_bytes) == native['original_archive_sha256'], 'accepted original native CPU receipt/archive changed')
    native_proof = read_json(native_proof_bytes)
    require(native_proof['verified'] and native_proof['source_commit'] == PROBE_SOURCE and native_proof['archive_sha256'] == native['completion_archive_sha256'] and native_proof['build_archive_sha256'] == native['original_archive_sha256'], 'independent strict native CPU acceptance mismatch')
    require(digest(Path(native['completion_archive_path']).read_bytes()) == native['completion_archive_sha256']
        and digest(Path(native['posthash_original_path']).read_bytes()) == native['posthash_original_sha256'], 'accepted completion/posthash original chain changed')
    config = read_json(contents['task-config-original.json'])
    task = read_json(contents['task-result-original.json'])
    finalized = read_json(contents['task-collection-original.json'])
    result = read_json(contents['output/result-original.json'])
    require(result['probe_source'] == probe_source and result['core_source'] == CORE_SOURCE,
            'actual phase result source identities differ')
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
    require(auth['authorized'] and auth['owner'] in ('/root', '/root/verify_ewdk_build', '/root/verify_cpu09_completion') and auth['phase'] == phase and auth['manifest_sha256'] == digest(manifest_bytes) and auth['output'] == config['output'], 'authorization was for another phase/output')
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
    native_members = read_native_archive(Path(native['original_archive_path']))
    completion_members = read_native_archive(Path(native['completion_archive_path']))
    native_result = read_json(native_members['result.json'])
    native_cpu_details = verify_native_identity_cpu(native_result, native_members, completion_members,
        read_json(Path(native['posthash_original_path']).read_bytes()))
    setup_details = None
    lifetime_details = None
    output_result, output_members = native_result, native_members
    if owned_setup:
        setup_details, output_result, output_members = verify_owned_icd_native_setup(manifest['native_setup'])
    if held_frontend:
        lifetime_details, output_result, output_members = verify_frontend_lifetime_native_build(
            manifest['native_lifetime'], manifest['native_setup'])
    for role, path in (('probe', 'probe/d3d8-runtime-probe.exe'), ('frontend', 'front/viogpu-d3d8-runtime-front.dll')):
        data = output_members[path]
        pin = next(row for row in manifest['files'] if row['role'] == role)
        require(digest(data) == pin['sha256'] and len(data) == pin['bytes'], 'actual native I386 output chain mismatch')
        row = [item for item in output_result['outputs'] if item['path'] == pin['path']]
        require(len(row) == 1 and row[0]['sha256'] == pin['sha256'] and row[0]['machine'] == 0x14c, 'native PE/probe output receipt mismatch')
    derived_roles = {'owned-icd-json'} if owned_setup else set()
    require(len(manifest['files']) == 6 + len(derived_roles)
        and {row['role'] for row in manifest['files']} == {'probe', 'frontend', *PAYLOADS, *derived_roles}, 'exact phase input definitions required')
    require(files.keys() == ({'probe'} if phase == 'names' else {'probe', 'frontend', *PAYLOADS, *derived_roles}), 'unexpected selected phase input')
    for row in manifest['files']:
        if row['role'] in PAYLOADS:
            size, sha, name = PAYLOADS[row['role']]
            folder = r'C:\Users\Public\DxvkD3D8Candidate-de72dc2-37711793677' + ('-icd02' if owned_setup else '')
            require(row['bytes'] == size and row['sha256'] == sha and row['path'] == folder + chr(92) + name, 'original I386 payload/config pin mismatch')
        elif row['role'] == 'owned-icd-json':
            require(row['bytes'] == 148 and row['sha256'] == OWNED_ICD_SHA
                and row['path'] == r'C:\Users\Public\DxvkD3D8Candidate-de72dc2-37711793677-icd02\freedreno_icd_owned_x86.json'
                and row['original_ZIP_member'] is False, 'separately derived ICD input mismatch')
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
        require(prior['verified'] and prior['phase'] == PRIOR[phase] and prior['manifest_sha256'] == digest(manifest_bytes) and prior['probe_source'] == probe_source and prior['core_source'] == CORE_SOURCE and prior['luid'] == luid and prior['source'] == source and prior['sid'] == SID, 'previous original admission mismatch')
        registered = prior['registered_I386_filename']
        mode = {'enumerate': 'front-enumerate', 'offscreen': 'front-offscreen', 'present': 'front-present'}[phase]
        arguments = f'--{mode} "{files["frontend"]["path"]}" "{registered}" "{files["core"]["path"]}" {files["core"]["sha256"]} {CORE_SOURCE}'
        arguments += f' {luid} {source}'
        require(command['arguments'] == arguments, 'exact selected runtime command mismatch')
        core_identity = {'source': manifest['core_source'], 'run': manifest['core_ci_run'],
                         'sha256': files['core']['sha256']}
        detail = verify_runtime_stdout(text, phase, luid, source, core_identity, owned_setup,
                                      files['frontend']['path'] if held_frontend else None)
    return {'schema': 'system-d3d8-phase-admission-v1', 'verified': True, 'phase': phase, 'manifest_sha256': digest(manifest_bytes),
            'probe_source': probe_source, 'core_source': CORE_SOURCE, 'core_ci_run': RUN, 'luid': luid, 'source': source, 'sid': SID,
            'registered_I386_filename': registered, 'original_archive_sha256': digest(raw_archive), 'original_archive_bytes': len(raw_archive),
            'original_files': len(contents), 'stdout_sha256': digest(stdout), 'process_sha256': digest(contents['output/process-original.json']),
            'collection_sha256': digest(collection_path.read_bytes()), 'details': detail, 'registration': registration,
            'native_cpu_details': native_cpu_details, 'native_setup_details': setup_details,
            **({'native_lifetime_details': lifetime_details} if held_frontend else {}),
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
