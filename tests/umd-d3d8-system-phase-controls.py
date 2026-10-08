#!/usr/bin/env python3
"""Small synthetic protocol controls. No native/runtime/hardware acceptance."""
import copy
import importlib.util
import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True
path = Path(__file__).resolve().parents[1] / 'scripts/verify-native-d3d8-system-phase.py'
spec = importlib.util.spec_from_file_location('phase', path)
phase = importlib.util.module_from_spec(spec); spec.loader.exec_module(phase)
checks = []


def reject(label, function, *args):
    try:
        function(*args)
    except (ValueError, UnicodeError):
        checks.append({'label': label, 'rejected': True})
    else:
        raise AssertionError('control incorrectly accepted: ' + label)


def module_records(name, logical=None):
    # Synthetic records exercise the actual parser, not a runtime admission.
    logical = logical or r'C:\Windows\System32' + chr(92) + name
    explicit = r'C:\Windows\SysWOW64' + chr(92) + name
    mapped = r'\Device\HarddiskVolume3\Windows\SyChpe32' + chr(92) + name
    def pe(view):
        return (f'D3D8_SYSTEM_PE name={name} view={view} machine=014c magic=010b sections=5 '
            'pe=00000080 timestamp=12345678 image_bytes=65536 header_bytes=512 checksum=00000011 entry=00001000')
    def file(view, path):
        return (f'D3D8_SYSTEM_FILE_ID name={name} view={view} input={path} final_nt={mapped} '
            f'volume=00112233 index_high=00000002 index_low=00000003 bytes=8192 machine=014c sha256={"67" * 32} unchanged=1 locked=1')
    return [pe('loaded'), pe('logical'), file('logical', logical), pe('explicit-I386'), file('explicit-I386', explicit),
        f'D3D8_SYSTEM_FILE_CLOSE name={name} view=explicit-I386 handle=00000124 status=1',
        f'D3D8_SYSTEM_FILE_CLOSE name={name} view=logical handle=00000120 status=1',
        f'D3D8_SYSTEM_MODULE_ID name={name} logical={logical} explicit={explicit} mapped_nt={mapped} identity=1 machine=014c readonly=1']


machine_line = 'D3D8_SYSTEM_PROCESS_ID process=014c native=aa64 pointer_bytes=4 api=IsWow64Process2 identity=1'
provider_records = [
    'D3D8_SYSTEM_API_LOOKUP provider=kernel32.dll process_present=1 process_error=0 directory_present=0 directory_error=127',
    'D3D8_SYSTEM_API_LOOKUP provider=kernelbase.dll directory_present=1 directory_error=0 exact_API_only=1',
    machine_line, *module_records('kernel32.dll'), *module_records('kernelbase.dll'),
    r'D3D8_SYSTEM_DIRECTORY machine=014c api=GetSystemWow64Directory2W path=C:\Windows\SysWOW64']


name = r'C:\Windows\System32\synthetic_legacy.dll'
encoded = name.encode('utf-16le')
words = [int.from_bytes(encoded[i:i+2], 'little') for i in range(0, len(encoded), 2)]
words += [0] * (260 - len(words))
prefix = [f'D3D8_USER_GATE session=1 elevation=0 elevation_type=3 integrity_rid=8192 sid={phase.SID}',
          *provider_records, *module_records('gdi32.dll'),
          r'D3D8_SYSTEM_GDI32 actual=C:\Windows\System32\gdi32.dll expected=C:\Windows\SysWOW64\gdi32.dll machine=014c pointer_bytes=4 file_identity=1',
          'D3D8_KMT_MATCH adapter=17 source=0 luid=ec6b000000000000 software=0 render=1 no_device=1',
          f'D3D8_KMT_NAME version=0 status=00000000 terminated=1 name={name} pointer_bytes=4 raw_bytes=524']
text = '\n'.join(prefix + [f'D3D8_KMT_NAME_WORD index={i} value={v:04x}' for i, v in enumerate(words)] +
                 ['D3D8_KMT_CLOSED status=00000000', 'D3D8_KMT_NAMES_COMPLETE system_runtime_calls=0 create_device=0 core_loads=0 registry_writes=0']) + '\n'
assert phase.verify_names(text.replace('\n', '\r\n'), 'ec6b000000000000', 0)['registered_I386_filename'] == name
checks.append({'label': 'original-260-word-CRLF-shape', 'accepted_synthetic': True})
reject('missing-last-original-word', phase.verify_names, text.replace('D3D8_KMT_NAME_WORD index=259 value=0000\n', ''), 'ec6b000000000000', 0)
reject('duplicate-original-word', phase.verify_names, text + 'D3D8_KMT_NAME_WORD index=259 value=0000\n', 'ec6b000000000000', 0)
reject('word-and-printed-name-disagree', phase.verify_names, text.replace('name=' + name, 'name=' + name.replace('synthetic', 'different')), 'ec6b000000000000', 0)
reject('wrong-pointer-architecture', phase.verify_names, text.replace('pointer_bytes=4', 'pointer_bytes=8'), 'ec6b000000000000', 0)
reject('wrong-process-machine', phase.verify_names, text.replace('process=014c', 'process=aa64'), 'ec6b000000000000', 0)
reject('legacy-directory-selection', phase.verify_names, text.replace('api=GetSystemWow64Directory2W', 'api=GetSystemDirectoryW'), 'ec6b000000000000', 0)
reject('wrong-system-directory', phase.verify_names, text.replace('path=C:\\Windows\\SysWOW64', 'path=C:\\Windows\\System32'), 'ec6b000000000000', 0)
reject('wrong-loaded-GDI32-path', phase.verify_names, text.replace('D3D8_SYSTEM_GDI32 actual=C:\\Windows\\System32\\gdi32.dll', 'D3D8_SYSTEM_GDI32 actual=C:\\Windows\\SysWOW64\\gdi32.dll'), 'ec6b000000000000', 0)
reject('missing-machine-evidence', phase.verify_names, text.replace(machine_line + '\n', ''), 'ec6b000000000000', 0)
reject('machine-evidence-after-KMT', phase.verify_names, text.replace(machine_line + '\n', '') + machine_line + '\n', 'ec6b000000000000', 0)
reject('wrong-adapter-source', phase.verify_names, text, 'ec6b000000000000', 1)
reject('elevated-USER', phase.verify_names, text.replace('elevation=0', 'elevation=1'), 'ec6b000000000000', 0)
reject('factory-output-in-names-phase', phase.verify_names, text + 'D3D8_API operation=Direct3DCreate8 object=1\n', 'ec6b000000000000', 0)
reject('name-query-original-failure', phase.verify_names, text.replace('status=00000000 terminated=1', 'status=c0000001 terminated=1'), 'ec6b000000000000', 0)
row = dict(Pid=19, ProcessHandle=21, Exited=True, ExitCodeAvailable=True, PipesDrained=True,
           TimedOut=False, ChildStillRunning=False, Failure=None, ExitCode=0,
           StdoutBytes=4, StderrBytes=0, Seconds=0.2, StartUtc='2026-10-08T00:00:00Z')
phase.closed(row, b'PASS', b'')
checks.append({'label': 'original-owned-exit-shape', 'accepted_synthetic': True})
for label, key, value in [('timeout-with-PASS-output', 'TimedOut', True), ('live-child-with-PASS-output', 'ChildStillRunning', True),
                          ('incomplete-raw-drain', 'PipesDrained', False), ('nonzero-exit-with-PASS-output', 'ExitCode', 7),
                          ('truncated-raw-output', 'StdoutBytes', 5), ('missing-retained-OS-handle', 'ProcessHandle', 0)]:
    control = copy.deepcopy(row); control[key] = value
    reject(label, phase.closed, control, b'PASS', b'')
caps = [0] * 53
caps[0], caps[7], caps[49], caps[50], caps[51] = 1, 0x90000, 0xfffe0101, 96, 0xffff0104
enumeration = '\n'.join([
    *provider_records, *module_records('d3d8.dll'),
    r'SYSTEM_D3D8_CALLER_PATH actual=C:\Windows\System32\d3d8.dll expected=C:\Windows\SysWOW64\d3d8.dll machine=014c pointer_bytes=4 directory_api=GetSystemWow64Directory2W file_identity=1',
    r'D3D8_RUNTIME path=C:\Windows\SysWOW64\d3d8.dll machine=014c pointer_bytes=4 sdk_version=220 caps_bytes=212',
    'D3D8_ADAPTER index=0 identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=00090000 vs=fffe0101 ps=ffff0104 constants=96',
    r'SYSTEM_D3D8_OPEN_BEGIN interface=8 version=12 runtime=00000100 caller=C:\Windows\System32\d3d8.dll pointer_bytes=4 readonly=1',
    f'SYSTEM_D3D8_OPEN_END hr=00000000 interface=8 driver_version=12 adapter=00000200 core=C:\\Users\\Public\\DxvkD3D8Candidate-d7e5c7d-37648387721\\viogpudxvk.dll expected_ci_source_commit={phase.CORE_SOURCE} machine=014c core_create_calls=0',
    f'SYSTEM_D3D8_CORE_PIN path=C:\\Users\\Public\\DxvkD3D8Candidate-d7e5c7d-37648387721\\viogpudxvk.dll sha256={phase.PAYLOADS["core"][1]} expected_ci_source_commit={phase.CORE_SOURCE} machine=014c file_locked=1 core_unchanged=1',
    'SYSTEM_D3D8_CAPS_BEGIN id=1 interface=8 type=12 bytes=212 info=0 adapter=00000200',
    'SYSTEM_D3D8_CAPS_END id=1 type=12 bytes=212 hr=00000000 caps_modified=0',
    'SYSTEM_D3D8_CAPS12 id=1 bytes=212 device_type=1 devcaps=00090000 caps2=00000000 primitive=00000000 vs=fffe0101 constants=96 ps=ffff0104',
    *[f'SYSTEM_D3D8_CAPS12_WORD id=1 index={i} value={word:08x}' for i, word in enumerate(caps)],
    'D3D8_SELECTOR installed=1 machine=014c pointer_bytes=4 slot_rva=1234 registry_writes=0',
    'D3D8_SELECTOR restored=1 protection_restored=1 substitutions=1 queries=2',
    'SYSTEM_D3D8_CLOSE adapter=00000200 runtime=00000100 hr=00000000 remaining=0 live_devices=0',
    'D3D8_COMPLETE mode=front-enumerate adapters=1 create_device=0 presents=0 registry_writes=0']) + '\n'
assert phase.verify_enumeration(enumeration)['HAL_caps']
checks.append({'label': 'synthetic-CAPS12-enumeration-shape', 'accepted_synthetic': True})
reject('missing-original-CAPS12-word', phase.verify_enumeration, enumeration.replace('SYSTEM_D3D8_CAPS12_WORD id=1 index=52 value=00000000\n', ''))
reject('wrong-typed-interface-publication', phase.verify_enumeration, enumeration.replace('driver_version=12', 'driver_version=13'))
reject('unsupported-shader-profile', phase.verify_enumeration, enumeration.replace('ps=ffff0104', 'ps=ffff0200'))
reject('device-callback-in-readonly-enumeration', phase.verify_enumeration, enumeration + 'SYSTEM_D3D8_CREATE_BLOCKED core_create_calls=0\n')

# Keep the exact previous 27 cases above and extend the same actual parser.
assert len(checks) == 27
physical = '\n'.join(provider_records + module_records('gdi32.dll')) + '\n'
original_identity = phase.verify_module_identities(phase.lines(physical))
assert original_identity['module_count'] == original_identity['readonly_file_pairs'] == 3
assert original_identity['closed_file_handles'] == 6
checks.append({'label': 'complete-mapped-file-identity-joins', 'accepted_synthetic': True})

direct_lookup = physical.replace(provider_records[0],
    'D3D8_SYSTEM_API_LOOKUP provider=kernel32.dll process_present=1 process_error=0 directory_present=1 directory_error=0')
direct_lookup = direct_lookup.replace(provider_records[1] + '\n', '')
assert phase.verify_module_identities(phase.lines(direct_lookup))['module_count'] == 3
checks.append({'label': 'original-Kernel32-Directory2-success-shape', 'accepted_synthetic': True})

def physical_reject(label, changed):
    reject(label, phase.verify_module_identities, phase.lines(changed))

gdi = module_records('gdi32.dll')
for label, old, new in [
    ('alternate-provider-without-proc-not-found', 'directory_present=0 directory_error=127', 'directory_present=0 directory_error=126'),
    ('failed-Process2-lookup', 'process_present=1 process_error=0', 'process_present=0 process_error=127'),
    ('wrong-alternate-provider', 'provider=kernelbase.dll directory_present=1', 'provider=ntdll.dll directory_present=1'),
    ('failed-alternate-Directory2', 'provider=kernelbase.dll directory_present=1 directory_error=0', 'provider=kernelbase.dll directory_present=0 directory_error=127'),
    ('missing-provider-readonly-join', '\n'.join(module_records('kernelbase.dll')) + '\n', ''),
    ('missing-provider-directory-publication', provider_records[-1] + '\n', ''),
    ('missing-logical-file-record', gdi[2] + '\n', ''),
    ('missing-explicit-file-record', gdi[4] + '\n', ''),
    ('loaded-PE-machine-mismatch', gdi[0], gdi[0].replace('machine=014c', 'machine=aa64')),
    ('explicit-selected-PE-mismatch', gdi[3], gdi[3].replace('timestamp=12345678', 'timestamp=12345679')),
    ('different-volume-identity', gdi[4], gdi[4].replace('volume=00112233', 'volume=00112234')),
    ('different-file-index-high', gdi[4], gdi[4].replace('index_high=00000002', 'index_high=00000004')),
    ('different-file-index-low', gdi[4], gdi[4].replace('index_low=00000003', 'index_low=00000004')),
    ('different-original-file-length', gdi[4], gdi[4].replace('bytes=8192', 'bytes=8193')),
    ('different-original-file-hash', gdi[4], gdi[4].replace('sha256=' + '67' * 32, 'sha256=' + '68' * 32)),
    ('changed-file-during-observation', gdi[2], gdi[2].replace('unchanged=1', 'unchanged=0')),
    ('readonly-file-not-locked', gdi[2], gdi[2].replace('locked=1', 'locked=0')),
    ('zero-original-file-index', gdi[2], gdi[2].replace('index_high=00000002 index_low=00000003', 'index_high=00000000 index_low=00000000')),
    ('missing-explicit-close', gdi[5] + '\n', ''),
    ('failed-explicit-close', gdi[5], gdi[5].replace('status=1', 'status=0')),
    ('missing-logical-close', gdi[6] + '\n', ''),
    ('failed-logical-close', gdi[6], gdi[6].replace('status=1', 'status=0')),
    ('zero-original-file-handle', gdi[5], gdi[5].replace('handle=00000124', 'handle=00000000')),
    ('duplicate-simultaneous-file-handle', gdi[5], gdi[5].replace('handle=00000124', 'handle=00000120')),
    ('reordered-close-originals', '\n'.join(gdi[5:7]), '\n'.join(reversed(gdi[5:7]))),
    ('mapped-NT-path-differs-from-held-files', gdi[7], gdi[7].replace('mapped_nt=\\Device\\HarddiskVolume3', 'mapped_nt=\\Device\\HarddiskVolume4')),
    ('logical-final-NT-differs-from-mapping', gdi[2], gdi[2].replace('final_nt=\\Device\\HarddiskVolume3', 'final_nt=\\Device\\HarddiskVolume4')),
    ('explicit-final-NT-differs-from-mapping', gdi[4], gdi[4].replace('final_nt=\\Device\\HarddiskVolume3', 'final_nt=\\Device\\HarddiskVolume4')),
    ('logical-module-name-differs-from-opened-file', gdi[7], gdi[7].replace('logical=C:\\Windows\\System32', 'logical=C:\\Windows\\SysWOW64')),
    ('explicit-directory-view-not-I386', '\n'.join(gdi), '\n'.join(gdi).replace('explicit=C:\\Windows\\SysWOW64', 'explicit=C:\\Windows\\System32').replace('input=C:\\Windows\\SysWOW64', 'input=C:\\Windows\\System32')),
    ('unpaired-identity-publication', '\n'.join(gdi), gdi[7]),
    ('unknown-named-system-module', '\n'.join(gdi), '\n'.join(gdi).replace('gdi32.dll', 'application.dll')),
    ('identity-flag-alone-cannot-admit', gdi[7], gdi[7].replace('identity=1', 'identity=0')),
]:
    assert old in physical, label
    physical_reject(label, physical.replace(old, new))

# Different observed NT paths are allowed only when all physical records agree.
# The policy never treats a SyChpe32 suffix as a directory admission shortcut.
consistent_nt = physical.replace('\\Device\\HarddiskVolume3\\Windows\\SyChpe32', '\\Device\\HarddiskVolume9\\Windows\\ObservedImageDirectory')
assert phase.verify_module_identities(phase.lines(consistent_nt))['module_count'] == 3
checks.append({'label': 'complete-observed-NT-join-without-suffix-whitelist', 'accepted_synthetic': True})

reject('runtime-caller-physical-pair-absent', phase.verify_enumeration,
       enumeration.replace('\n'.join(module_records('d3d8.dll')) + '\n', ''))
reject('runtime-caller-differs-from-physical-image', phase.verify_enumeration,
       enumeration.replace('SYSTEM_D3D8_CALLER_PATH actual=C:\\Windows\\System32\\d3d8.dll',
                           'SYSTEM_D3D8_CALLER_PATH actual=C:\\Windows\\SysWOW64\\d3d8.dll'))
reject('runtime-open-differs-from-verified-caller', phase.verify_enumeration,
       enumeration.replace('caller=C:\\Windows\\System32\\d3d8.dll', 'caller=C:\\Windows\\System32\\d3d9.dll'))
reject('runtime-caller-published-after-open', phase.verify_enumeration,
       '\n'.join(row for row in phase.lines(enumeration) if not row.startswith('SYSTEM_D3D8_CALLER_PATH ')) + '\n'
       + next(row for row in phase.lines(enumeration) if row.startswith('SYSTEM_D3D8_CALLER_PATH ')) + '\n')
reject('extra-unpaired-runtime-open', phase.verify_enumeration,
       enumeration + next(row for row in phase.lines(enumeration) if row.startswith('SYSTEM_D3D8_OPEN_BEGIN ')) + '\n')
reject('runtime-readonly-open-disabled', phase.verify_enumeration, enumeration.replace('readonly=1', 'readonly=0'))
assert phase.verify_enumeration(enumeration.replace('C:\\Windows\\', 'C:\\WINDOWS\\'))['HAL_caps']
checks.append({'label': 'actual-directory-API-case-preserved', 'accepted_synthetic': True})
reject('runtime-path-differs-from-explicit-file-view', phase.verify_enumeration,
       enumeration.replace('D3D8_RUNTIME path=C:\\Windows\\SysWOW64', 'D3D8_RUNTIME path=C:\\Windows\\System32'))
old_native = {'schema': 'native-system-d3d8-device-x86-build-v1', 'status': 'PASS',
              'source_commit': '5c420e4daddc39effb2c8e8a28bd07ec7407c402', 'core_reference_commit': phase.CORE_SOURCE,
              'object_count': 5, 'pe_count': 4, 'malformed_cli_guards': 30}
reject('old-full-native-suite-is-not-CPU08-source', phase.verify_native_identity_cpu, old_native, {})
changed_source = copy.deepcopy(old_native); changed_source['source_commit'] = phase.PROBE_SOURCE
reject('old-full-native-suite-cannot-be-relabeled-CPU08', phase.verify_native_identity_cpu, changed_source, {})
print(json.dumps({'status': 'PASS', 'scope': 'synthetic names/process/mapped-file protocol controls only', 'checks': checks,
                  'actual_runtime_calls': 0, 'actual_native_processes': 0, 'GPU_runs': 0, 'target_calls': 0}, indent=2))
