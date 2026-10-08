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

PROBE_SOURCE = '66bfbdf73d32d7213af439a69cb569730b018f55'
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


def verify_enumeration(text, luid='ec6b000000000000', source=0, core_identity=None):
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
    expected_core = rf'C:\Users\Public\DxvkD3D8Candidate-{core_source[:7]}-{core_run}\viogpudxvk.dll'
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


def verify_native_identity_cpu(result, members):
    """Join the focused CPU08 scope without relabeling the old full CPU suite."""
    require(result['schema'] == 'native-system-d3d8-device-x86-build-v1' and result['status'] == 'PASS'
        and result['source_commit'] == PROBE_SOURCE and result['core_reference_commit'] == CORE_SOURCE,
        'exact accepted shared-identity native CPU source required')
    require(result['object_count'] == 4 and result['pe_count'] == 3 and result['malformed_cli_guards'] == 4,
            'focused native CPU output/CLI scope mismatch')
    require(result['shared_identity_fixture'] == {'executed': True, 'checks': 91, 'runtime_factories': 0,
        'KMT_calls': 0, 'core_loads': 0}, 'actual shared predicate execution scope mismatch')
    require(result['process_api_diagnostics'] == {'executed': True, 'verified_modules': 3, 'readonly_file_pairs': 3,
        'runtime_factories': 0, 'KMT_calls': 0, 'core_loads': 0, 'admission': False},
        'actual read-only process/module observation scope mismatch')
    require(result['source_before'] == result['source_after'] and len(result['source_before']) == 21
        and result['before'] == result['after'], 'native original source/state changed')
    require(result['compiler_full_before'] == result['compiler_full_after']
        and len(result['compiler_full_before']) == 575 and result['sdk'] == result['sdk_after']
        and len(result['sdk']) == 13 and result['libraries'] == result['libraries_after']
        and len(result['libraries']) == 9, 'native official compiler/selected SDK/library originals changed')
    require(result['compiler_provenance']['sha256'] == 'c6333c67b4f3725f513e82c488b844054859b28456bccb0131eb59b805a5db48'
        and digest(members['original-compiler-provenance.json']) == result['compiler_provenance']['sha256'],
        'unchanged original compiler-ready manifest required')
    ready = read_json(members['original-compiler-provenance.json'])
    tools = {row['path']: row for row in ready['files']}
    observed_tools = {row['path']: row for row in result['compiler_full_before']}
    require(ready['ready'] and ready['file_count'] == len(tools) == len(ready['files']) == 575
        and len(observed_tools) == 575 and observed_tools.keys() == tools.keys(), 'complete original compiler file set required')
    for path, row in observed_tools.items():
        require(all(row[key] == tools[path][key] for key in ('bytes', 'sha256')), 'native compiler file differs from unchanged ready pin')
    source_rows = {row['input_path']: row for row in result['source_before']}
    require(len(source_rows) == 21, 'duplicate native source input')
    for path, row in source_rows.items():
        member = PurePosixPath(path)
        require(not member.is_absolute() and '..' not in member.parts, 'unsafe native source path')
        data = members['source/' + str(member)]
        require(len(data) == row['bytes'] and digest(data) == row['sha256'], 'retained native source bytes differ from before/after rows')
    for key, prefix in [('sdk', 'original-sdk-headers/'), ('libraries', 'original-link-libraries/')]:
        names = set()
        for row in result[key]:
            name = row['path'].rsplit(chr(92), 1)[-1]
            require(name not in names, 'duplicate original selected native dependency')
            names.add(name)
            data = members[prefix + name]
            require(len(data) == row['bytes'] and digest(data) == row['sha256'], 'retained native selected dependency changed')
    require(not result['installation'] and not result['production_core_built']
        and result['gpu_runs'] == result['system_runtime_calls'] == result['selector_calls'] == 0,
        'focused native CPU build cannot grant runtime/hardware admission')
    require(result['raw_process_source']['sha256'] == result['raw_process_source_after']['sha256'] == RAW
        and digest(members['executed-raw-process-source.cs']) == RAW, 'actual native process runner changed')
    expected = ['archive-list', 'extract']
    for group, units in [('front', ['tests_umd-d3d8-runtime-front.cpp']),
                         ('probe', ['tests_umd-d3d8-runtime-probe.cpp', 'tests_umd-d3d8-runtime-guard.cpp']),
                         ('identity', ['tests_umd-d3d8-system-identity.cpp'])]:
        expected += [group + '-' + unit + '-compile' for unit in units] + [group + '-link', group + '-headers']
    expected += ['identity-fixture', 'process-api-diagnostics'] + ['invalid-cli-' + str(i) for i in range(1, 5)]
    require([command['name'] for command in result['commands']] == expected,
            'exact eighteen original native build children required')
    for command in result['commands']:
        name = command['name']
        stdout, stderr = members[name + '.stdout.txt'], members[name + '.stderr.txt']
        require(command['stdout'].rsplit(chr(92), 1)[-1] == name + '.stdout.txt'
            and command['stderr'].rsplit(chr(92), 1)[-1] == name + '.stderr.txt'
            and command['raw_pipe_bytes'], 'actual native raw member paths mismatch')
        raw = {'Pid': command['pid'], 'ProcessHandle': command['retained_process_handle'], 'Exited': command['exited'],
               'ExitCodeAvailable': command['exit_code_available'], 'PipesDrained': command['pipes_drained'],
               'TimedOut': command['timeout'], 'ChildStillRunning': command['child_still_running'],
               'Failure': command['capture_failure'], 'ExitCode': command['exit'], 'StdoutBytes': command['stdout_bytes'],
               'StderrBytes': command['stderr_bytes'], 'Seconds': command['seconds'], 'StartUtc': command['start_utc']}
        expected_exit = 64 if name.startswith('invalid-cli-') else 0
        require(command['expected'] == expected_exit, 'actual native child exit contract mismatch')
        expected_deadline = 180 if name.endswith('-compile') else 120 if name.endswith('-link') else 30
        if name == 'identity-fixture' or name.startswith('invalid-cli-'):
            expected_deadline = 15
        require(command['deadline_seconds'] == expected_deadline, 'frozen native child deadline changed')
        closed(raw, stdout, stderr, command['deadline_seconds'] * 1000, expected_exit)
        if name.endswith('-compile'):
            require(command['first_party_strict'] and not command['compiler_warnings']
                and not re.search(rb'\bwarning [CD]\d+', stdout + stderr, re.I), 'strict native source compiler diagnostic retained')
        if name in ('identity-fixture', 'process-api-diagnostics'):
            require(command['arguments'] == ('' if name == 'identity-fixture' else '--process-api-diagnostics')
                and command['deadline_seconds'] == (15 if name == 'identity-fixture' else 30),
                'focused native execution command/deadline mismatch')
        if name.startswith('invalid-cli-'):
            arguments = ['', '--unknown', '--process-api-diagnostics extra', '--process-api-diagnostics one two']
            require(command['arguments'] == arguments[int(name.rsplit('-', 1)[1]) - 1], 'frozen malformed native CLI changed')
    require(members['identity-fixture.stdout.txt'].decode('utf-8').strip()
        == 'D3D8 system image identity fixture PASS checks=91 runtime_calls=0 KMT_calls=0 core_loads=0',
        'original shared predicate stdout missing')
    rows = lines(members['process-api-diagnostics.stdout.txt'].decode('utf-8'))
    identities = verify_module_identities(rows)
    require([entry['name'] for entry in identities['modules']] == ['kernel32.dll', 'kernelbase.dll', 'gdi32.dll']
        and len(identities['process_rows']) == len(identities['directory_rows']) == 1,
        'actual read-only three-module native observation incomplete')
    require(single(rows, 'D3D8_PROCESS_API_DIAGNOSTICS_COMPLETE ')
        == 'D3D8_PROCESS_API_DIAGNOSTICS_COMPLETE observed_providers=5 observed_lookup_rows=10 verified_modules=3 readonly_file_pairs=3 runtime_calls=0 KMT_calls=0 core_loads=0 admission=0',
        'actual read-only native completion/scope missing')
    return {'objects': 4, 'PEs': 3, 'shared_predicate_checks': 91, 'malformed_CLI': 4,
            'owned_build_children': 18, 'original_module_identities': identities,
            'runtime_factories': 0, 'KMT_calls': 0, 'core_loads': 0, 'hardware_admission': False}


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
        native_members = {}
        for member in original:
            if member.isdir():
                continue
            name = PurePosixPath(member.name.removeprefix('./'))
            require(member.isfile() and not name.is_absolute() and '..' not in name.parts and str(name) not in native_members,
                    'unsafe or duplicate native CPU original member')
            native_members[str(name)] = original.extractfile(member).read()
        native_result = read_json(native_members['result.json'])
        native_cpu_details = verify_native_identity_cpu(native_result, native_members)
        for role, path in (('probe', 'probe/d3d8-runtime-probe.exe'), ('frontend', 'front/viogpu-d3d8-runtime-front.dll')):
            data = native_members[path]
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
        arguments += f' {luid} {source}'
        require(command['arguments'] == arguments, 'exact selected runtime command mismatch')
        if phase == 'enumerate':
            detail = verify_enumeration(text, luid, source)
        else:
            runtime_identity = verify_runtime_module_callers(lines(text))
            spec = importlib.util.spec_from_file_location('pixels', Path(__file__).with_name('verify-native-d3d8-system-device.py'))
            pixels = importlib.util.module_from_spec(spec); spec.loader.exec_module(pixels)
            detail = pixels.verify(text, mode, luid, source)
            detail['physical_runtime_identities'] = runtime_identity
    return {'schema': 'system-d3d8-phase-admission-v1', 'verified': True, 'phase': phase, 'manifest_sha256': digest(manifest_bytes),
            'probe_source': PROBE_SOURCE, 'core_source': CORE_SOURCE, 'core_ci_run': RUN, 'luid': luid, 'source': source, 'sid': SID,
            'registered_I386_filename': registered, 'original_archive_sha256': digest(raw_archive), 'original_archive_bytes': len(raw_archive),
            'original_files': len(contents), 'stdout_sha256': digest(stdout), 'process_sha256': digest(contents['output/process-original.json']),
            'collection_sha256': digest(collection_path.read_bytes()), 'details': detail, 'registration': registration,
            'native_cpu_details': native_cpu_details,
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
