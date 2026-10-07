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


name = r'C:\Windows\System32\synthetic_legacy.dll'
encoded = name.encode('utf-16le')
words = [int.from_bytes(encoded[i:i+2], 'little') for i in range(0, len(encoded), 2)]
words += [0] * (260 - len(words))
prefix = [f'D3D8_USER_GATE session=1 elevation=0 elevation_type=3 integrity_rid=8192 sid={phase.SID}',
          'D3D8_PROCESS_MACHINE process=014c native=aa64 effective=014c pointer_bytes=4 legacy_status=1 legacy_wow=0',
          r'D3D8_SYSTEM_DIRECTORY machine=014c api=GetSystemWow64Directory2W path=C:\Windows\SysWOW64',
          r'D3D8_SYSTEM_GDI32 actual=C:\Windows\SysWOW64\gdi32.dll expected=C:\Windows\SysWOW64\gdi32.dll machine=014c pointer_bytes=4',
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
reject('wrong-loaded-GDI32-path', phase.verify_names, text.replace('actual=C:\\Windows\\SysWOW64\\gdi32.dll', 'actual=C:\\Windows\\System32\\gdi32.dll'), 'ec6b000000000000', 0)
reject('missing-machine-evidence', phase.verify_names, text.replace(prefix[1] + '\n', ''), 'ec6b000000000000', 0)
reject('machine-evidence-after-KMT', phase.verify_names, text.replace(prefix[1] + '\n', '') + prefix[1] + '\n', 'ec6b000000000000', 0)
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
    r'D3D8_RUNTIME path=C:\Windows\SysWOW64\d3d8.dll machine=014c pointer_bytes=4 sdk_version=220 caps_bytes=212',
    'D3D8_ADAPTER index=0 identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=00090000 vs=fffe0101 ps=ffff0104 constants=96',
    r'SYSTEM_D3D8_OPEN_BEGIN interface=8 version=12 runtime=00000100 caller=C:\Windows\SysWOW64\d3d8.dll pointer_bytes=4 readonly=1',
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
print(json.dumps({'status': 'PASS', 'scope': 'synthetic names/process protocol controls only', 'checks': checks,
                  'actual_runtime_calls': 0, 'actual_native_processes': 0, 'GPU_runs': 0, 'target_calls': 0}, indent=2))
