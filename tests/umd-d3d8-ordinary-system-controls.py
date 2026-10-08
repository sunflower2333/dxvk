#!/usr/bin/env python3
"""Synthetic corruption checks for the new reader, never GPU acceptance."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--physical-stdout', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    reader = module(root / 'scripts/verify-native-d3d8-ordinary-system.py', 'new_ordinary_d8_reader')
    physical_reader = module(root / 'scripts/verify-native-d3d8-system-phase.py', 'raw_physical_records_only')
    original = args.physical_stdout.read_bytes()
    source_rows = original.decode('utf-8-sig').splitlines()
    physical_rows = [row for row in source_rows if row.startswith(physical_reader.IDENTITY_PREFIXES)]
    joined = physical_reader.verify_module_identities(physical_rows)
    runtime = [row for row in joined['modules'] if row['name'] == 'd3d8.dll']
    if not runtime:
        raise ValueError('Retained original physical identity rows required, no pixel workload replay')
    # Reuse only the complete physical file record shapes. Everything about
    # the new factory/pixels/hold is explicitly synthetic, not an actual run.
    first = runtime[0]
    block = physical_rows[first['start_row']:first['end_row'] + 1]
    base = physical_rows[:first['start_row']]
    physical = base + block * 3
    core = r'C:\Users\Public\DxvkD3D8Candidate-1111111-12345-icd02\viogpudxvk.dll'
    commit, digest = '1' * 40, '2' * 64
    folder = core.rsplit('\\', 1)[0] + '\\'
    luid, event, output = '0300000000000000', r'Local\VioGpuD8Validation-' + '3' * 32, r'C:\Users\Public\DxvkD8Lifecycle-reader-control\originals'
    raw = bytearray(160)
    for offset, value in ((0, 0x504d5644), (8, 128), (128, 0x44494c56), (132, 1), (136, 32), (140, 1), (152, 1)):
        raw[offset:offset+4] = value.to_bytes(4, 'little')
    raw[24:32] = (2).to_bytes(8, 'little'); raw[144:152] = bytes.fromhex(luid)
    held = {'schema': 'ordinary-system-d3d8-held-v1', 'pid': 1234, 'timeout_ms': 60000, 'pending_exit': 0,
        'hold_event': event, 'output': output, 'stage': '', 'pixels_passed': True, 'device_alive': True,
        'modules_exact': True, 'selector_installed': False, 'restoration_proved_by_event': False}

    def fixture(present):
        rows = physical + ['D3D8_SYSTEM_ROUTING selector_installed=0 diagnostic_permission=0 factory_module=SYSTEM installed_slot=WoW0',
            f'D3D8_USER_GATE session=1 elevation=0 elevation_type=3 integrity_rid=8192 sid={reader.SID}',
            r'D3D8_RUNTIME path=C:\Windows\SysWOW64\d3d8.dll machine=014c pointer_bytes=4 sdk_version=220 caps_bytes=212',
            'D3D8_API operation=Direct3DCreate8 object=1',
            'D3D8_ADAPTER index=0 identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=00090000 vs=fffe0101 ps=ffff0104 constants=96']
        words = [ord(c) for c in core] + [0] * (260 - len(core))
        for _ in range(3):
            rows += [f'D3D8_KMT_MATCH adapter=1 source=0 luid={luid} software=0 render=1 no_device=1',
                'D3D8_KMT_CLOSED status=00000000', f'D3D8_KMT_NAME version=0 status=00000000 terminated=1 name={core} pointer_bytes=4 raw_bytes=524']
            rows += [f'D3D8_KMT_NAME_WORD index={i} value={word:04x}' for i, word in enumerate(words)]
            rows += [f'D3D8_SYSTEM_IDENTITY_BYTE index={i} value={value:02x}' for i, value in enumerate(raw)]
        for name, value in {**reader.PAYLOADS, 'viogpudxvk.dll': digest}.items():
            kind = 'json' if name.endswith('.json') else '014c'
            rows.append(f'D3D8_PAYLOAD_PIN path={folder}{name} sha256={value} machine={kind} locked=1 original_bytes=1')
        rows.append(f'D3D8_DERIVED_ICD_PIN path={folder}freedreno_icd_owned_x86.json sha256=f50169e3e0efc6dea34fe0ce109228c79ce1df817a5508fb13a759c71d780ff3 original_sha256={reader.PAYLOADS["freedreno_icd.json"]} locked=1 original_bytes=0 library_path=.\\viogpu_gl_vk_x86.dll only_library_path_changed=1')
        for _ in range(2):
            rows += [f'D3D8_SYSTEM_MODULE role={role} present=1 exact=1 machine=014c path={folder}{name} explicit_preload=0'
                for role, name in enumerate(('viogpudxvk.dll', 'viogpu_gl_loader_x86.dll', 'viogpu_gl_vk_x86.dll'))]
        rows += [f'D3D8_PIXEL stage={stage} x={x} y={y} value={color:08x}' for stage, color in enumerate(reader.COLORS, 1) for y in range(8) for x in range(8)]
        if present:
            rows += [f'D3D8_SCREEN_PIXEL x={x} y={y} rgb=193e72' for y in range(8) for x in range(8)]
        calls = {'CreateDevice-HAL-hardwareVP': 1, 'ordinary-public-created-device-identity': 1,
            'CreateVertexShader-1.1': 1, 'CreatePixelShader-1.1': 1, 'CreatePixelShader-1.4': 1,
            'CreateTexture-dynamic': 1, 'CopyRects-RT-to-systemmem': 7, 'Reset': 1, 'Present-owned-window': int(present)}
        rows += [f'D3D8_API operation={name} hr=00000000' for name, count in calls.items() for _ in range(count)]
        phase = 'present' if present else 'offscreen'
        rows += [f'D3D8_SYSTEM_RENDER PASS stages=7 pixels=448 presents={int(present)} screen_pixels={64 if present else 0} selector_installed=0',
            f'D3D8_COMPLETE mode=system-{phase} adapters=1 create_device=1 presents={int(present)} registry_writes=0',
            'D3D8_SYSTEM_HELD_END wait=0 pending_exit=0 restoration_proved_by_event=0']
        rows += [f'D3D8_SYSTEM_MODULE_RELEASE role={role} released=1' for role in range(3)]
        rows += ['D3D8_SYSTEM_EVENT_RELEASE released=1']
        return '\n'.join(rows)

    checks = []
    def verify(text, state, phase='offscreen'):
        return reader.verify(text, state, phase, luid, 0, core, digest, commit, event, output)
    for phase in ('offscreen', 'present'):
        verify(fixture(phase == 'present'), held, phase); checks.append('positive-' + phase)
    baseline = fixture(False)
    verify(baseline.replace(r'path=C:\Windows\SysWOW64\d3d8.dll machine=', r'path=c:\WINDOWS\syswow64\D3D8.DLL machine='), held)
    checks.append('positive-runtime-path-case')
    controls = {
        'wrong-runtime-path': baseline.replace(r'path=C:\Windows\SysWOW64\d3d8.dll machine=', r'path=C:\Users\Public\d3d8.dll machine='),
        'pixel-color': baseline.replace('value=ff123456', 'value=ff123457', 1),
        'pixel-duplicate': baseline + '\nD3D8_PIXEL stage=1 x=0 y=0 value=ff123456',
        'query-selector': baseline + '\nD3D8_SELECTOR installed=1',
        'paired-LUID': baseline.replace('D3D8_SYSTEM_IDENTITY_BYTE index=144 value=03', 'D3D8_SYSTEM_IDENTITY_BYTE index=144 value=04'),
        'filename-word': baseline.replace('D3D8_KMT_NAME_WORD index=0 value=0043', 'D3D8_KMT_NAME_WORD index=0 value=0044', 1),
        'missing-name-word': baseline.replace('D3D8_KMT_NAME_WORD index=259 value=0000\n', '', 1),
        'wrong-module-ABI': baseline.replace('exact=1 machine=014c', 'exact=1 machine=aa64', 1),
        'failed-create': baseline.replace('operation=CreateDevice-HAL-hardwareVP hr=00000000', 'operation=CreateDevice-HAL-hardwareVP hr=80004005'),
        'missing-release': baseline.replace('D3D8_SYSTEM_MODULE_RELEASE role=2 released=1\n', ''),
        'hold-timeout': baseline.replace('HELD_END wait=0', 'HELD_END wait=258'),
    }
    for name, text in controls.items():
        try: verify(text, held)
        except (ValueError, UnicodeError): checks.append('rejected-' + name)
        else: raise ValueError('Corruption accepted: ' + name)
    for key, value in {'pending_exit': 1, 'device_alive': False, 'modules_exact': False,
        'selector_installed': True, 'restoration_proved_by_event': True}.items():
        state = copy.deepcopy(held); state[key] = value
        try: verify(baseline, state)
        except ValueError: checks.append('rejected-held-' + key)
        else: raise ValueError('Held corruption accepted: ' + key)
    args.output.write_text(json.dumps({'passed': True, 'synthetic_only': True, 'hardware_admission': False,
        'physical_record_input': {'path': str(args.physical_stdout.resolve()), 'bytes': len(original), 'sha256': hashlib.sha256(original).hexdigest()},
        'checks': checks, 'check_count': len(checks)}, indent=2) + '\n')
    print(json.dumps({'synthetic_only': True, 'checks': len(checks), 'passed': True}))


if __name__ == '__main__':
    main()
