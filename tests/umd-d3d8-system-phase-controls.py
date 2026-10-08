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
caps[0], caps[7], caps[8], caps[49], caps[50], caps[51] = 1, 0x90000, 0x2000, 0xfffe0101, 96, 0xffff0104
folder = rf'C:\Users\Public\DxvkD3D8Candidate-{phase.CORE_SOURCE[:7]}-{phase.RUN}' + chr(92)
enumeration = '\n'.join([
    prefix[0], *provider_records, *module_records('d3d8.dll'),
    r'SYSTEM_D3D8_CALLER_PATH actual=C:\Windows\System32\d3d8.dll expected=C:\Windows\SysWOW64\d3d8.dll machine=014c pointer_bytes=4 directory_api=GetSystemWow64Directory2W file_identity=1',
    r'D3D8_RUNTIME path=C:\Windows\SysWOW64\d3d8.dll machine=014c pointer_bytes=4 sdk_version=220 caps_bytes=212',
    'D3D8_ENUMERATION_CONSTRUCTION allowed=1 public_create_device=0 draw=0 presents=0 core_entry=OpenAdapter',
    *[f'D3D8_PAYLOAD_PIN path={folder + name} sha256={digest} machine={"json" if name.endswith(".json") else "014c"} locked=1 original_bytes=1' for _, digest, name in phase.PAYLOADS.values()],
    f'D3D8_HARDWARE_SOURCE core_commit={phase.CORE_SOURCE} ci_run={phase.RUN} loader_source=6a6878c614c8c6dbe81ee7a9f1176bdb52dc7dd7 icd_source=8443c71a5ab32b9d58b904fa51f4bf2f9089db8d icd_ci_run=37453381660 driver_selection=owned-json raw_architecture=014c',
    r'SYSTEM_D3D8_OPEN_BEGIN interface=8 version=69632 runtime=00000100 caller=C:\Windows\System32\d3d8.dll pointer_bytes=4 readonly=0',
    f'SYSTEM_D3D8_OPEN_END hr=00000000 interface=8 driver_version=12 adapter=00000200 core={folder}viogpudxvk.dll expected_ci_source_commit={phase.CORE_SOURCE} machine=014c core_create_calls=0',
    'SYSTEM_D3D8_ENUMERATION_MODE adapter=00000200 version=69632 captured_mode=3 core_entry=OpenAdapter render_permission=0',
    f'SYSTEM_D3D8_CORE_PIN path={folder}viogpudxvk.dll sha256={phase.PAYLOADS["core"][1]} expected_ci_source_commit={phase.CORE_SOURCE} machine=014c file_locked=1 core_unchanged=1',
    'SYSTEM_D3D8_CREATE_CONTRACT adapter=00000200 runtime=00000300 interface=8 version=69632 flags=00000000 callbacks=1 functions=1 command=0 allocation_list=0 patch_list=0 captured_mode=3',
    'SYSTEM_D3D8_CALLBACK_TABLE runtime=00000300 adapter_runtime=00000100 original=00000400 wrapped=00000500 bytes=88 owned_snapshot=1 borrowed_table_reread=0',
    'SYSTEM_D3D8_CALLBACK_PRESENCE runtime=00000300 allocate=1 deallocate=1 lock=1 unlock=1 create_context=1 destroy_context=1 escape=1 render=1 present=1 residency=1',
    'SYSTEM_D3D8_CREATE_RETURN runtime=00000300 driver=00000600 hr=00000000 interface=8 core_create_calls=1',
    'SYSTEM_D3D8_ENUMERATION_BOUNDARY device=00000600 blocked_mask=00004783 bytes=396 public_create_device=0 draw_forwarding=0 present_forwarding=0',
    'SYSTEM_D3D8_DEVICE_FUNCTIONS bytes=396 interface=12 published=1',
    *[f'D3D8_PRIVATE_MODULE name={name} path={folder + name} machine=014c' for name in ('viogpudxvk.dll','viogpu_gl_loader_x86.dll','viogpu_gl_vk_x86.dll')],
    'SYSTEM_D3D8_ENUMERATION_PRIVATE_PAYLOADS device=00000600 checked_after_create=1 machine=014c forbidden_loader=0',
    'SYSTEM_D3D8_CAPS_BEGIN id=1 interface=8 type=12 bytes=212 info=0 adapter=00000200',
    'SYSTEM_D3D8_CAPS_END id=1 type=12 bytes=212 hr=00000000 caps_modified=0',
    'SYSTEM_D3D8_CAPS12 id=1 bytes=212 device_type=1 devcaps=00090000 caps2=00000000 primitive=00002000 vs=fffe0101 constants=96 ps=ffff0104',
    *[f'SYSTEM_D3D8_CAPS12_WORD id=1 index={i} value={word:08x}' for i, word in enumerate(caps)],
    'SYSTEM_D3D8_DEVICE_DESTROY_BEGIN device=00000600 callback_owner_live=1',
    'SYSTEM_D3D8_LIFETIME phase=destroyed runtime=00000300 allocate=2 deallocate=2 lock=1 unlock=1 create_context=1 destroy_context=1 render=1 present=0 residency=2 live_allocations=0 live_locks=0 live_contexts=0 tracking_errors=0 callback_failures=0',
    'SYSTEM_D3D8_DEVICE_DESTROY device=00000600 hr=00000000 remaining=0 callback_owner_released=1',
    'SYSTEM_D3D8_CLOSE adapter=00000200 runtime=00000100 hr=00000000 remaining=0 live_devices=0',
    'D3D8_API operation=Direct3DCreate8 object=1',
    'D3D8_ADAPTER index=0 identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=00090000 vs=fffe0101 ps=ffff0104 constants=96',
    *module_records('gdi32.dll'),
    'D3D8_KMT_MATCH adapter=17 source=0 luid=ec6b000000000000 software=0 render=1 no_device=1',
    'D3D8_KMT_CLOSED status=00000000',
    'D3D8_SELECTOR installed=1 machine=014c pointer_bytes=4 slot_rva=1234 registry_writes=0',
    'D3D8_SELECTOR restored=1 protection_restored=1 substitutions=1 queries=2',
    'D3D8_COMPLETE mode=front-enumerate adapters=1 create_device=0 presents=0 registry_writes=0',
    'D3D8_ENUMERATION_COMPLETE internal_driver_construction=1 public_create_device=0 draw=0 presents=0 selector_restored=1 environment_restored=1']) + '\n'
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
reject('runtime-readonly-open-disabled', phase.verify_enumeration,
       enumeration.replace('pointer_bytes=4 readonly=0', 'pointer_bytes=4 readonly=1'))
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
assert len(checks) == 73
for label, old, new in [
    ('readonly-cannot-construct-internal-device', 'readonly=0', 'readonly=1'),
    ('wrong-captured-enumeration-mode', 'captured_mode=3', 'captured_mode=2'),
    ('internal-device-runtime-version-mismatch', 'interface=8 version=69632 flags=', 'interface=8 version=69633 flags='),
    ('internal-device-reserved-flags', 'flags=00000000 callbacks=', 'flags=00000001 callbacks='),
    ('required-runtime-callback-absent', 'destroy_context=1 escape=1', 'destroy_context=0 escape=1'),
    ('unowned-borrowed-callback-table', 'owned_snapshot=1 borrowed_table_reread=0', 'owned_snapshot=0 borrowed_table_reread=1'),
    ('missing-typed-draw-denial', 'blocked_mask=00004783', 'blocked_mask=00004782'),
    ('missing-typed-Present-denial', 'blocked_mask=00004783', 'blocked_mask=00000783'),
    ('missing-actual-private-ICD', f'D3D8_PRIVATE_MODULE name=viogpu_gl_vk_x86.dll path={folder}viogpu_gl_vk_x86.dll machine=014c\n', ''),
    ('wrong-private-loader-source', 'loader_source=6a6878c614c8c6dbe81ee7a9f1176bdb52dc7dd7', 'loader_source=8443c71a5ab32b9d58b904fa51f4bf2f9089db8d'),
    ('internal-device-allocation-leak', 'allocate=2 deallocate=2', 'allocate=2 deallocate=1'),
    ('internal-context-not-destroyed', 'create_context=1 destroy_context=1 render=1', 'create_context=1 destroy_context=0 render=1'),
    ('unreported-live-context', 'live_contexts=0 tracking_errors=0', 'live_contexts=1 tracking_errors=0'),
    ('internal-callback-failure', 'callback_failures=0', 'callback_failures=1'),
    ('internal-Present-callback', 'render=1 present=0 residency=', 'render=1 present=1 residency='),
    ('failed-HAL-caps-must-reject', 'caps_hr=00000000 vendor=', 'caps_hr=8876086a vendor='),
    ('missing-driver-only-legacy-fog', 'primitive=00002000', 'primitive=00000000'),
    ('public-API-device-claimed-during-enumeration', 'create_device=0 presents=0 registry_writes=0', 'create_device=1 presents=0 registry_writes=0'),
]:
    assert old in enumeration, label
    reject(label, phase.verify_enumeration, enumeration.replace(old,new))
reject('denied-runtime-workload-was-attempted', phase.verify_enumeration,
       enumeration + 'SYSTEM_D3D8_ENUMERATION_FORBIDDEN device=00000600 forwarded=0 hr=8876086a\n')
reject('public-API-clear-during-enumeration', phase.verify_enumeration,
       enumeration + 'D3D8_API operation=Clear hr=00000000\n')
current_native = {'schema': 'native-system-d3d8-device-x86-build-v1', 'status': 'FAIL',
                  'source_commit': phase.PROBE_SOURCE, 'core_reference_commit': phase.CORE_SOURCE}
reject('CPU09-build-alone-is-not-complete', phase.verify_native_identity_cpu, current_native, {})
reject('CPU09-completion-without-original-posthash', phase.verify_native_identity_cpu, current_native, {}, {})
for label, key, value in [('CPU08-source-does-not-admit-current-binaries', 'source_commit', '66bfbdf73d32d7213af439a69cb569730b018f55'),
                         ('old-d7-core-does-not-admit-de72', 'core_reference_commit', 'd7e5c7d46b8ce889e993bfab66a3b78b076c49d1'),
                         ('failed-marker-build-must-remain-FAIL', 'status', 'PASS')]:
    control = copy.deepcopy(current_native); control[key] = value
    reject(label, phase.verify_native_identity_cpu, control, {}, {}, {})
assert len(checks) == 98
owned_folder = folder.rstrip(chr(92)) + '-icd02' + chr(92)
derived_pin = (f'D3D8_DERIVED_ICD_PIN path={owned_folder}freedreno_icd_owned_x86.json '
    f'sha256={phase.OWNED_ICD_SHA} original_sha256={phase.PAYLOADS["icd-json"][1]} locked=1 original_bytes=0 '
    'library_path=.' + chr(92) + 'viogpu_gl_vk_x86.dll only_library_path_changed=1')
owned_enumeration = enumeration.replace(folder, owned_folder)
owned_enumeration = owned_enumeration.replace('D3D8_HARDWARE_SOURCE ', derived_pin + '\nD3D8_HARDWARE_SOURCE ', 1)
def verify_owned(text):
    return phase.verify_enumeration(text, owned_icd_setup=True)
assert verify_owned(owned_enumeration)['HAL_caps']
checks.append({'label': 'explicit-owned-ICD-synthetic-enumeration', 'accepted_synthetic': True})
reject('new-ICD-root-with-old-setup', phase.verify_enumeration, owned_enumeration)
reject('old-ICD-root-with-new-setup', verify_owned, enumeration)
for label, old, new in [
    ('derived-ICD-marker-missing', derived_pin + '\n', ''),
    ('derived-ICD-marker-duplicated', derived_pin, derived_pin + '\n' + derived_pin),
    ('derived-ICD-hash-changed', 'sha256=' + phase.OWNED_ICD_SHA, 'sha256=' + '12' * 32),
    ('derived-ICD-original-hash-changed', 'original_sha256=' + phase.PAYLOADS['icd-json'][1], 'original_sha256=' + '34' * 32),
    ('derived-ICD-falsely-claims-original', 'locked=1 original_bytes=0', 'locked=1 original_bytes=1'),
    ('derived-ICD-relative-module-changed', 'library_path=.' + chr(92) + 'viogpu_gl_vk_x86.dll', 'library_path=viogpu_gl_vk_x86.dll'),
    ('derived-ICD-extra-field-change', 'only_library_path_changed=1', 'only_library_path_changed=0'),
]:
    reject(label, verify_owned, owned_enumeration.replace(old, new))
reject('derived-ICD-locked-after-construction', verify_owned,
       owned_enumeration.replace(derived_pin + '\n', '') + derived_pin + '\n')
reject('owned-ICD-preserves-public-workload-denial', verify_owned,
       owned_enumeration + 'D3D8_API operation=Clear hr=00000000\n')
reject('owned-ICD-preserves-failed-HAL-denial', verify_owned,
       owned_enumeration.replace('caps_hr=00000000', 'caps_hr=8876086a'))
assert len(checks) == 111
core_identity = {'source': phase.CORE_SOURCE, 'run': phase.RUN, 'sha256': phase.PAYLOADS['core'][1]}
assert phase.verify_runtime_stdout(owned_enumeration, 'enumerate', 'ec6b000000000000', 0,
                                   core_identity, True)['HAL_caps']
checks.append({'label': 'archive-production-dispatch-owned-enumeration', 'accepted_synthetic': True})
reject('archive-runtime-dispatch-requires-explicit-core', phase.verify_runtime_stdout,
       owned_enumeration, 'enumerate', 'ec6b000000000000', 0, None, True)
for key, value in [('source', '12' * 20), ('run', phase.RUN + 1), ('sha256', '12' * 32)]:
    wrong_core = dict(core_identity); wrong_core[key] = value
    reject('archive-runtime-dispatch-core-' + key + '-must-match', phase.verify_runtime_stdout,
           owned_enumeration, 'enumerate', 'ec6b000000000000', 0, wrong_core, True)

pixel_spec = importlib.util.spec_from_file_location('pixel_fixture', path.with_name('verify-native-d3d8-system-device.py'))
pixel = importlib.util.module_from_spec(pixel_spec); pixel_spec.loader.exec_module(pixel)
def pixel_fixture(present):
    # Complete synthetic draw/readback/teardown protocol; no native or GPU calls.
    markers = ('D3D8_USER_GATE ', 'D3D8_SYSTEM_', 'SYSTEM_D3D8_CALLER_PATH ',
        'SYSTEM_D3D8_OPEN_BEGIN ', 'D3D8_RUNTIME ', 'SYSTEM_D3D8_CORE_PIN ',
        'D3D8_PAYLOAD_PIN ', 'D3D8_DERIVED_ICD_PIN ', 'D3D8_HARDWARE_SOURCE ',
        'D3D8_PRIVATE_MODULE ', 'D3D8_ADAPTER ', 'D3D8_KMT_', 'D3D8_SELECTOR ',
        'SYSTEM_D3D8_CREATE_RETURN ', 'SYSTEM_D3D8_CALLBACK_TABLE ',
        'SYSTEM_D3D8_DEVICE_FUNCTIONS ', 'SYSTEM_D3D8_LIFETIME ',
        'SYSTEM_D3D8_DEVICE_DESTROY ', 'SYSTEM_D3D8_CLOSE ')
    fixture = [row for row in owned_enumeration.splitlines() if row.startswith(markers)]
    fixture += [row for row in fixture if row.startswith('D3D8_PRIVATE_MODULE ')]
    fixture += ['D3D8_SELECTOR_QUERY index=0 type=1 bytes=524 kernel_adapter=17 original_status=00000000 selected=1 substitutions=1']
    fixture += [f'D3D8_PIXEL stage={stage} x={x} y={y} value={color:08x}'
                for stage, color in enumerate(pixel.COLORS, 1) for x in range(8) for y in range(8)]
    for operation, count in {'CreateDevice-HAL-hardwareVP':1, 'CreateVertexShader-1.1':1,
            'CreatePixelShader-1.1':1, 'CreatePixelShader-1.4':1, 'CreateTexture-dynamic':1,
            'CopyRects-RT-to-systemmem':7, 'Reset':1, 'Present-owned-window':int(present)}.items():
        fixture += [f'D3D8_API operation={operation} hr=00000000'] * count
    fixture += [f'D3D8_SELECTED_OFFSCREEN PASS stages=7 pixels=448 shader=VS1.1/PS1.1+PS1.4 dynamic_texture=1 resets=1 presents={int(present)}']
    if present:
        fixture += [f'D3D8_SCREEN_PIXEL x={x} y={y} rgb=193e72' for x in range(8) for y in range(8)]
        fixture += ['D3D8_PRESENT PASS calls=1 pixels=64 rgb=193e72 polls=1 source=actual-screen']
        fixture = [row.replace('render=1 present=0 residency=', 'render=1 present=1 residency=') for row in fixture]
    fixture += [f'D3D8_COMPLETE mode=front-{"present" if present else "offscreen"} adapters=1 create_device=1 presents={int(present)} registry_writes=0']
    return '\n'.join(fixture) + '\n'

def dispatch_pixels(text, mode, identity=core_identity, owned=True):
    return phase.verify_runtime_stdout(text, mode, 'ec6b000000000000', 0, identity, owned)
def reject_pixel(label, function, *args):
    try: function(*args)
    except (AssertionError, ValueError, UnicodeError): checks.append({'label': label, 'rejected': True})
    else: raise AssertionError('control incorrectly accepted: ' + label)
for mode in ('offscreen', 'present'):
    pixels = pixel_fixture(mode == 'present')
    result = dispatch_pixels(pixels, mode)
    assert result['offscreen_pixels'] == 448 and result['screen_pixels'] == (64 if mode == 'present' else 0)
    checks.append({'label': 'archive-production-dispatch-owned-' + mode, 'accepted_synthetic': True})
    reject_pixel(mode + '-missing-owned-ICD-lock', dispatch_pixels, pixels.replace(derived_pin + '\n', ''), mode)
    reject_pixel(mode + '-old-unsuffixed-directory', dispatch_pixels, pixels.replace(owned_folder, folder), mode)
    reject_pixel(mode + '-derived-falsely-original', dispatch_pixels, pixels.replace('locked=1 original_bytes=0', 'locked=1 original_bytes=1'), mode)
    reject_pixel(mode + '-derived-lock-after-construction', dispatch_pixels, pixels.replace(derived_pin + '\n', '') + derived_pin + '\n', mode)
    reject_pixel(mode + '-owned-ICD-with-old-scope', dispatch_pixels, pixels, mode, core_identity, False)
    wrong_core = dict(core_identity); wrong_core['sha256'] = '12' * 32
    reject_pixel(mode + '-joined-core-hash-mismatch', dispatch_pixels, pixels, mode, wrong_core)
    reject_pixel(mode + '-pixel-regression', dispatch_pixels, pixels.replace('value=ff123456', 'value=ff123457', 1), mode)
    reject_pixel(mode + '-allocation-leak', dispatch_pixels, pixels.replace('allocate=2 deallocate=2', 'allocate=2 deallocate=1'), mode)
    legacy_pixels = pixels.replace(derived_pin + '\n', '').replace(owned_folder, folder)
    assert dispatch_pixels(legacy_pixels, mode, core_identity, False)['offscreen_pixels'] == 448
    checks.append({'label': 'archive-production-dispatch-legacy-' + mode, 'accepted_synthetic': True})
assert len(checks) == 136
frontend_path = r'C:\Users\Public\DxvkD3D8Runtime-fbd7afd-lifetime03\front\viogpu-d3d8-runtime-front.dll'
acquire = (f'D3D8_FRONTEND_REFERENCE path={frontend_path} machine=014c owned=1 '
    'preloaded_adoption=0 hold_through_runtime_teardown=1')
release = f'D3D8_FRONTEND_REFERENCE_RELEASE path={frontend_path} released=1'
def with_frontend_owner(text):
    rows = text.splitlines()
    first_open = next(i for i, row in enumerate(rows) if row.startswith('SYSTEM_D3D8_OPEN_BEGIN '))
    rows.insert(first_open, acquire)
    complete = next(i for i, row in enumerate(rows) if row.startswith('D3D8_COMPLETE '))
    rows.insert(complete, release)
    return '\n'.join(rows) + '\n'
def dispatch_held(text, mode, path=frontend_path):
    return phase.verify_runtime_stdout(text, mode, 'ec6b000000000000', 0, core_identity, True, path)
for mode in ('offscreen', 'present'):
    held = with_frontend_owner(pixel_fixture(mode == 'present'))
    detail = dispatch_held(held, mode)
    assert detail['frontend_owned_through_runtime_teardown'] and detail['offscreen_pixels'] == 448
    assert detail['screen_pixels'] == (64 if mode == 'present' else 0)
    checks.append({'label': 'archive-dispatch-held-frontend-' + mode, 'accepted_synthetic': True})
    for label, old, new in [
        ('missing-acquisition', acquire + '\n', ''), ('missing-release', release + '\n', ''),
        ('duplicate-acquisition', acquire, acquire + '\n' + acquire), ('duplicate-release', release, release + '\n' + release),
        ('adopts-preloaded-frontend', 'preloaded_adoption=0', 'preloaded_adoption=1'),
        ('missing-runtime-hold', 'hold_through_runtime_teardown=1', 'hold_through_runtime_teardown=0'),
        ('release-failed', 'released=1', 'released=0'),
        ('wrong-frontend-module', 'path=' + frontend_path, 'path=' + frontend_path.replace('fbd7afd', 'badbeef')),
    ]:
        reject_pixel(mode + '-frontend-' + label, dispatch_held, held.replace(old, new), mode)
    reject_pixel(mode + '-frontend-identity-not-explicit', dispatch_pixels, held, mode)
    reject_pixel(mode + '-frontend-released-before-runtime-teardown', dispatch_held,
        release + '\n' + held.replace(release + '\n', ''), mode)
    reject_pixel(mode + '-frontend-acquired-after-runtime-construction', dispatch_held,
        held.replace(acquire + '\n', '') + acquire + '\n', mode)
    reject_pixel(mode + '-frontend-release-after-completion', dispatch_held,
        held.replace(release + '\n', '') + release + '\n', mode)
    reject_pixel(mode + '-frontend-runtime-record-after-release', dispatch_held,
        held.replace(release + '\n', release + '\nSYSTEM_D3D8_CLOSE adapter=1 runtime=1 hr=00000000 remaining=0 live_devices=0\n'), mode)
assert len(checks) == 164
held_enumeration = with_frontend_owner(owned_enumeration)
assert dispatch_held(held_enumeration, 'enumerate')['frontend_owned_through_runtime_teardown']
checks.append({'label': 'archive-dispatch-held-frontend-enumeration', 'accepted_synthetic': True})
reject_pixel('enumeration-frontend-owner-missing', dispatch_held, owned_enumeration, 'enumerate')
reject_pixel('enumeration-frontend-owner-released-early', dispatch_held,
    release + '\n' + held_enumeration.replace(release + '\n', ''), 'enumerate')
reject_pixel('frontend-acquisition-before-private-pins', dispatch_held,
    acquire + '\n' + held_enumeration.replace(acquire + '\n', ''), 'enumerate')
reject_pixel('frontend-acquisition-wrong-machine', dispatch_held,
    held_enumeration.replace('machine=014c owned=1', 'machine=aa64 owned=1'), 'enumerate')
reject_pixel('frontend-owner-borrowed', dispatch_held,
    held_enumeration.replace('machine=014c owned=1', 'machine=014c owned=0'), 'enumerate')
reject_pixel('frontend-explicit-identity-differs', dispatch_held,
    held_enumeration, 'enumerate', frontend_path.replace('fbd7afd', 'badbeef'))
reject('lifetime-build-unaccepted', phase.verify_frontend_lifetime_native_build,
       {'accepted': False, 'source': phase.LIFETIME_SOURCE}, {'accepted': True, 'source': phase.SETUP_SOURCE})
reject('lifetime-build-relabels-old-source', phase.verify_frontend_lifetime_native_build,
       {'accepted': True, 'source': phase.SETUP_SOURCE}, {'accepted': True, 'source': phase.SETUP_SOURCE})
reject('lifetime-build-lacks-original-guard-admission', phase.verify_frontend_lifetime_native_build,
       {'accepted': True, 'source': phase.LIFETIME_SOURCE}, {'accepted': False, 'source': phase.SETUP_SOURCE})
assert len(checks) == 174
print(json.dumps({'status': 'PASS', 'scope': 'synthetic names/process/mapped-file/runtime-dispatch/pixel protocol controls only', 'checks': checks,
                  'actual_runtime_calls': 0, 'actual_native_processes': 0, 'GPU_runs': 0, 'target_calls': 0}, indent=2))
