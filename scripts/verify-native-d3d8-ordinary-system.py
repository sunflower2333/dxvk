#!/usr/bin/env python3
"""Review literal ordinary DX8 records; target execution/recovery remain external."""
import argparse
import importlib.util
import json
import re
from pathlib import Path

COLORS = (0xff123456, 0xff739a4c, 0xffc0568e, 0xff288cb0,
          0xff623a81, 0xff91b742, 0xffa362d1)
PAYLOADS = {
    'viogpu_gl_loader_x86.dll': 'd459f2d09080865cc3d591b498c02d38305a26963b401152f8230dc60c5ad7e7',
    'viogpu_gl_vk_x86.dll': '2b549889816163433faabe6f2c1d2a61d6c106078d08e30031b74c0a66cd7f5c',
    'freedreno_icd.json': '74d7d5d6ae9432cde2d802507ed59bbe4c2f2b95d01e7ac3c2b56e9691932c80',
}
SID = 'S-1-5-21-362894365-441372107-2852668596-1000'


def require(value, message):
    if not value:
        raise ValueError(message)


def verify(text, held, phase, luid, source, core, core_sha, commit, event, output):
    require(phase in ('offscreen', 'present'), 'exact ordinary phase required')
    require(re.fullmatch(r'[0-9a-f]{16}', luid) and int(luid, 16), 'actual little-endian LUID required')
    require(type(source) is int and 0 <= source <= 0xffffffff, 'actual source ID required')
    require(re.fullmatch(r'[0-9a-f]{64}', core_sha) and re.fullmatch(r'[0-9a-f]{40}', commit), 'actual core source/hash required')
    candidate = re.fullmatch(r'C:\\Users\\Public\\DxvkD3D8Candidate-([0-9a-f]{7})-([1-9][0-9]*)(-icd02)?\\viogpudxvk\.dll', core)
    require(candidate and candidate[1] == commit[:7], 'source/run-bound owned I386 core path required')
    text = text.replace('\r\n', '\n')
    rows = text.splitlines()
    require(not any(row.startswith(('D3D8_ERROR ', 'D3D8_FAILED ', 'D3D8_UNAVAILABLE ',
        'D3D8_SELECTOR', 'D3D8_FRONTEND_REFERENCE', 'SYSTEM_D3D8_')) for row in rows), 'failed or diagnostic-selection records cannot admit ordinary rendering')
    routing = 'D3D8_SYSTEM_ROUTING selector_installed=0 diagnostic_permission=0 factory_module=SYSTEM installed_slot=WoW0'
    require(rows.count(routing) == 1, 'ordinary no-selector route absent')
    require(rows.count(f'D3D8_USER_GATE session=1 elevation=0 elevation_type=3 integrity_rid=8192 sid={SID}') == 1, 'original limited USER required')

    # Use the existing independent raw mapped/file identity parser, not the
    # probe's file_identity=1 boolean or a logical System32 name alone.
    path = Path(__file__).with_name('verify-native-d3d8-system-phase.py')
    spec = importlib.util.spec_from_file_location('d8_original_physical_identity', path)
    identity_reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(identity_reader)
    physical = identity_reader.verify_module_identities(rows)
    runtime = [row for row in physical['modules'] if row['name'] == 'd3d8.dll']
    require(len(runtime) == 3 and all(row['sha256'] == '65d8980c469e45d862c68ad046731fd85c19401d3436fd46c65c19db1182dad8'
        and row['file_key'] == runtime[0]['file_key'] and row['selected_PE'] == runtime[0]['selected_PE']
        for row in runtime), 'three consistent original Microsoft D3D8 mapped/file joins required')
    runtime_rows = re.findall(r'^D3D8_RUNTIME path=(.+) machine=014c pointer_bytes=4 sdk_version=220 caps_bytes=212$', text, re.M)
    require(len(runtime_rows) == 1 and runtime_rows[0].casefold() == r'C:\Windows\SysWOW64\d3d8.dll'.casefold(), 'genuine x86 SYSTEM runtime record required')
    require(rows.count('D3D8_API operation=Direct3DCreate8 object=1') == 1, 'genuine public D3D8 factory absent')
    matches = re.findall(r'^D3D8_KMT_MATCH adapter=([1-9]\d*) source=(\d+) luid=([0-9a-f]{16}) software=0 render=1 no_device=1$', text, re.M)
    require(len(matches) == 3 and all(row[1:] == (str(source), luid) for row in matches), 'pre-factory/public/pre-hold KMT LUID/source required')
    require(rows.count('D3D8_KMT_CLOSED status=00000000') == 3, 'three actual KMT closes required')
    names = re.findall(r'^D3D8_KMT_NAME version=0 status=00000000 terminated=1 name=(.+) pointer_bytes=4 raw_bytes=524$', text, re.M)
    require(names == [core] * 3, 'actual unmodified WoW legacy core name required')
    name_words, words = [], []
    for row in rows:
        if not row.startswith('D3D8_KMT_NAME_WORD '):
            continue
        match = re.fullmatch(r'D3D8_KMT_NAME_WORD index=(\d+) value=([0-9a-f]{4})', row)
        require(match and int(match[1]) == len(words), 'original WoW name word ordering required')
        words.append(int(match[2], 16))
        if len(words) == 260:
            require(0 in words, 'bounded original WoW name required')
            value = bytes(byte for word in words[:words.index(0)] for byte in word.to_bytes(2, 'little')).decode('utf-16-le')
            require(value == core, 'original 260-word WoW name differs from the core')
            name_words.append(words)
            words = []
    require(not words and len(name_words) == 3, 'all three original 260-word WoW names required')
    identities = []
    current = []
    for row in rows:
        if not row.startswith('D3D8_SYSTEM_IDENTITY_BYTE '):
            continue
        match = re.fullmatch(r'D3D8_SYSTEM_IDENTITY_BYTE index=(\d+) value=([0-9a-f]{2})', row)
        require(match and int(match[1]) == len(current), 'paired160 byte ordering required')
        current.append(int(match[2], 16))
        if len(current) == 160:
            identities.append(bytes(current))
            current = []
    require(not current and len(identities) == 3 and len(set(identities)) == 1, 'stable actual paired160 bytes required')
    raw = identities[0]
    u32 = lambda offset: int.from_bytes(raw[offset:offset+4], 'little')
    require([u32(n) for n in (0, 4, 8, 12, 128, 132, 136, 140, 152, 156)] ==
        [0x504d5644, 0, 128, 0, 0x44494c56, 1, 32, 1, 1, 0]
        and raw[144:152].hex() == luid and int.from_bytes(raw[24:32], 'little') > 0
        and raw[112:128] == bytes(16), 'paired160 original KMD contract required')

    folder = core.rsplit('\\', 1)[0] + '\\'
    pins = re.findall(r'^D3D8_PAYLOAD_PIN path=(.+) sha256=([0-9a-f]{64}) machine=(014c|json) locked=1 original_bytes=1$', text, re.M)
    expected = {(folder + name, digest, 'json' if name.endswith('.json') else '014c') for name, digest in {**PAYLOADS, 'viogpudxvk.dll': core_sha}.items()}
    require(len(pins) == 4 and set(pins) == expected, 'four original payload pins required')
    modules = re.findall(r'^D3D8_SYSTEM_MODULE role=([012]) present=1 exact=1 machine=014c path=(.+) explicit_preload=0$', text, re.M)
    for role, name in enumerate(('viogpudxvk.dll', 'viogpu_gl_loader_x86.dll', 'viogpu_gl_vk_x86.dll')):
        require(modules.count((str(role), folder + name)) == 2, 'actual factory/held module tuple differs')
    require(len(modules) == 6, 'exact two three-module censuses required')
    require(rows.count(f'D3D8_DERIVED_ICD_PIN path={folder}freedreno_icd_owned_x86.json sha256=f50169e3e0efc6dea34fe0ce109228c79ce1df817a5508fb13a759c71d780ff3 original_sha256={PAYLOADS["freedreno_icd.json"]} locked=1 original_bytes=0 library_path=.\\viogpu_gl_vk_x86.dll only_library_path_changed=1') == 1, 'exact separately derived ICD pin required')
    adapters = re.findall(r'^D3D8_ADAPTER index=\d+ identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=([0-9a-f]+) vs=fffe0101 ps=ffff0104 constants=96$', text, re.M)
    require(len(adapters) == 1 and int(adapters[0], 16) & 0x90000 == 0x90000, 'actual bounded VirtIO HAL caps required')
    pixels = {}
    screen = {}
    for row in rows:
        if row.startswith('D3D8_PIXEL '):
            match = re.fullmatch(r'D3D8_PIXEL stage=(\d+) x=(\d+) y=(\d+) value=([0-9a-f]{8})', row)
            require(match, 'malformed literal pixel')
            stage, x, y = map(int, match.group(1, 2, 3))
            key = stage, x, y
            require(key not in pixels and 1 <= stage <= 7 and 0 <= x < 8 and 0 <= y < 8, 'duplicate/out of bounds pixel')
            require(int(match[4], 16) == COLORS[stage-1], 'literal pixel mismatch')
            pixels[key] = int(match[4], 16)
        elif row.startswith('D3D8_SCREEN_PIXEL '):
            match = re.fullmatch(r'D3D8_SCREEN_PIXEL x=(\d+) y=(\d+) rgb=([0-9a-f]{6})', row)
            require(match, 'malformed screen pixel')
            key = int(match[1]), int(match[2])
            require(key not in screen and all(0 <= n < 8 for n in key) and match[3] == '193e72', 'literal screen mismatch')
            screen[key] = int(match[3], 16)
    present = phase == 'present'
    require(len(pixels) == 448 and len(screen) == (64 if present else 0), 'all original448/optional64 pixels required')
    calls = re.findall(r'^D3D8_API operation=(\S+) hr=([0-9a-f]{8})$', text, re.M)
    require(calls and all(hr == '00000000' for _, hr in calls), 'failed HRESULT cannot admit rendering')
    for operation, count in {'CreateDevice-HAL-hardwareVP': 1, 'ordinary-public-created-device-identity': 1,
        'CreateVertexShader-1.1': 1, 'CreatePixelShader-1.1': 1, 'CreatePixelShader-1.4': 1,
        'CreateTexture-dynamic': 1, 'CopyRects-RT-to-systemmem': 7, 'Reset': 1, 'Present-owned-window': int(present)}.items():
        require(sum(name == operation for name, _ in calls) == count, 'actual API count: ' + operation)
    render = f'D3D8_SYSTEM_RENDER PASS stages=7 pixels=448 presents={int(present)} screen_pixels={64 if present else 0} selector_installed=0'
    require(rows.count(render) == 1, 'ordinary render completion absent')
    require(len(re.findall(rf'^D3D8_COMPLETE mode=system-{phase} adapters=([1-9]\d*) create_device=1 presents={int(present)} registry_writes=0$', text, re.M)) == 1, 'ordinary final cleanup completion absent')
    keys = {'schema', 'pid', 'timeout_ms', 'pending_exit', 'hold_event', 'output', 'stage', 'pixels_passed', 'device_alive', 'modules_exact', 'selector_installed', 'restoration_proved_by_event'}
    require(set(held) == keys and held['schema'] == 'ordinary-system-d3d8-held-v1'
        and type(held['pid']) is int and held['pid'] > 0 and type(held['timeout_ms']) is int and held['timeout_ms'] == 60000
        and type(held['pending_exit']) is int and held['pending_exit'] == 0 and held['hold_event'] == event
        and held['output'] == output and held['stage'] == '' and all(held[n] is True for n in ('pixels_passed', 'device_alive', 'modules_exact'))
        and held['selector_installed'] is False and held['restoration_proved_by_event'] is False, 'exact original held success checkpoint required')
    require(rows.count('D3D8_SYSTEM_HELD_END wait=0 pending_exit=0 restoration_proved_by_event=0') == 1, 'bounded original hold release absent')
    for role in range(3):
        require(rows.count(f'D3D8_SYSTEM_MODULE_RELEASE role={role} released=1') == 1, 'actual owned module release absent')
    require(rows.count('D3D8_SYSTEM_EVENT_RELEASE released=1') == 1, 'actual owned event release absent')
    return {'scope': 'ordinary-DX8-literal-and-runtime-records', 'passed': True, 'pixels': 448,
        'screen_pixels': len(screen), 'presents': int(present), 'luid16': luid,
        'hardware_admission': False, 'registry_restoration_proved': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--held', type=Path, required=True)
    parser.add_argument('--phase', choices=('offscreen', 'present'), required=True)
    parser.add_argument('--luid16', required=True)
    parser.add_argument('--source-id', type=int, required=True)
    parser.add_argument('--core', required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--hold-event', required=True)
    parser.add_argument('--output-prefix', required=True)
    args = parser.parse_args()
    result = verify(args.stdout.read_text(encoding='utf-8-sig'), json.loads(args.held.read_text(encoding='utf-8-sig')),
        args.phase, args.luid16, args.source_id, args.core, args.core_sha256, args.source_commit, args.hold_event, args.output_prefix)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
