#!/usr/bin/env python3
"""Read retained stdout only. Synthetic controls never claim runtime acceptance."""
import argparse
import json
import re
from pathlib import Path

CORE_COMMIT = 'de72dc2e97bd8e4ea70c5bf89c26918d06065723'
CORE_HASH = 'ba60b53fe43c8901da3e37d8407958e8e83c05d973e48c94240a3d048bc09cba'
CORE_PATH = r'C:\Users\Public\DxvkD3D8Candidate-de72dc2-37711793677\viogpudxvk.dll'
SID = 'S-1-5-21-362894365-441372107-2852668596-1000'
COLORS = (0xff123456, 0xff739a4c, 0xffc0568e, 0xff288cb0, 0xff623a81, 0xff91b742, 0xffa362d1)
PAYLOADS = {'viogpudxvk.dll': CORE_HASH,
            'viogpu_gl_loader_x86.dll': 'd459f2d09080865cc3d591b498c02d38305a26963b401152f8230dc60c5ad7e7',
            'viogpu_gl_vk_x86.dll': '2b549889816163433faabe6f2c1d2a61d6c106078d08e30031b74c0a66cd7f5c',
            'freedreno_icd.json': '74d7d5d6ae9432cde2d802507ed59bbe4c2f2b95d01e7ac3c2b56e9691932c80'}


def verify_frontend_reference(rows, frontend_path=None):
    """Join the selected frontend owner to actual construction and teardown."""
    acquired = [row for row in rows if row.startswith('D3D8_FRONTEND_REFERENCE ')]
    released = [row for row in rows if row.startswith('D3D8_FRONTEND_REFERENCE_RELEASE ')]
    if frontend_path is None:
        assert not acquired and not released, 'frontend owner requires explicit joined frontend identity'
        return False
    assert isinstance(frontend_path, str) and re.fullmatch(
        r'C:\\Users\\Public\\DxvkD3D8Runtime-[A-Za-z0-9-]+\\front\\viogpu-d3d8-runtime-front\.dll', frontend_path)
    acquire = (f'D3D8_FRONTEND_REFERENCE path={frontend_path} machine=014c owned=1 '
        'preloaded_adoption=0 hold_through_runtime_teardown=1')
    release = f'D3D8_FRONTEND_REFERENCE_RELEASE path={frontend_path} released=1'
    assert acquired == [acquire] and released == [release], 'exact single owned frontend acquisition/release required'
    begin, end = rows.index(acquire), rows.index(release)
    opens = [i for i, row in enumerate(rows) if row.startswith('SYSTEM_D3D8_OPEN_BEGIN ')]
    pins = [i for i, row in enumerate(rows) if row.startswith(('D3D8_PAYLOAD_PIN ', 'D3D8_DERIVED_ICD_PIN '))]
    teardown = [i for i, row in enumerate(rows) if row.startswith(
        ('SYSTEM_D3D8_LIFETIME ', 'SYSTEM_D3D8_DEVICE_DESTROY ', 'SYSTEM_D3D8_CLOSE '))]
    restored = [i for i, row in enumerate(rows) if row.startswith('D3D8_SELECTOR restored=1 protection_restored=1 ')]
    complete = [i for i, row in enumerate(rows) if row.startswith('D3D8_COMPLETE ')]
    assert opens and pins and teardown and len(restored) == len(complete) == 1
    assert max(pins) < begin < min(opens), 'frontend owner must follow payload pins and precede construction'
    assert max(opens + teardown + restored) < end < complete[0], 'frontend owner released before final runtime teardown'
    assert complete[0] == end + 1, 'frontend owner release must immediately precede successful completion'
    return True


def verify(text, mode, luid, source, core_identity=None, owned_icd_setup=False, frontend_path=None):
    assert mode in ('front-offscreen', 'front-present')
    assert type(owned_icd_setup) is bool
    if core_identity is None:
        assert not owned_icd_setup, 'owned ICD setup requires an explicit joined core tuple'
        core_identity = {'source': CORE_COMMIT, 'run': 37711793677, 'sha256': CORE_HASH}
    assert isinstance(core_identity, dict) and set(core_identity) == {'source', 'run', 'sha256'}
    assert re.fullmatch(r'[0-9a-f]{40}', core_identity['source']) and re.fullmatch(r'[0-9a-f]{64}', core_identity['sha256'])
    assert type(core_identity['run']) is int and core_identity['run'] > 0
    core_commit, core_run, core_hash = (core_identity[key] for key in ('source', 'run', 'sha256'))
    core_path = rf'C:\Users\Public\DxvkD3D8Candidate-{core_commit[:7]}-{core_run}' + ('-icd02' if owned_icd_setup else '') + r'\viogpudxvk.dll'
    text = text.replace('\r\n', '\n')
    rows = text.splitlines()
    frontend_held = verify_frontend_reference(rows, frontend_path)
    assert not re.search(r'^(?:D3D8_(?:ERROR|FAILED|UNAVAILABLE)|SYSTEM_D3D8_(?:CREATE_BLOCKED|DEVICE_DESTROY_FAILED))\b', text, re.M)
    runtime = [r for r in rows if r.startswith('D3D8_RUNTIME ')]
    assert len(runtime) == 1
    runtime_path = re.fullmatch(r'D3D8_RUNTIME path=(.+) machine=014c pointer_bytes=4 sdk_version=220 caps_bytes=212', runtime[0])
    assert runtime_path and runtime_path[1].casefold() == r'C:\Windows\SysWOW64\d3d8.dll'.casefold()
    user = [r for r in rows if r.startswith('D3D8_USER_GATE ')]
    assert user == [f'D3D8_USER_GATE session=1 elevation=0 elevation_type=3 integrity_rid=8192 sid={SID}']
    matched = re.findall(r'^D3D8_KMT_MATCH adapter=(\d+) source=(\d+) luid=([0-9a-f]{16}) software=0 render=1 no_device=1$', text, re.M)
    assert len(matched) == 1 and int(matched[0][0]) > 0 and matched[0][1:] == (str(source), luid)
    assert rows.count('D3D8_KMT_CLOSED status=00000000') == 1
    core = re.findall(r'^SYSTEM_D3D8_CORE_PIN path=(.+) sha256=([0-9a-f]{64}) expected_ci_source_commit=([0-9a-f]{40}) machine=014c file_locked=1 core_unchanged=1$', text, re.M)
    assert core == [(core_path, core_hash, core_commit)]
    pins = re.findall(r'^D3D8_PAYLOAD_PIN path=(.+) sha256=([0-9a-f]{64}) machine=(014c|json) locked=1 original_bytes=1$', text, re.M)
    folder = core_path.rsplit('\\', 1)[0] + '\\'
    expected_pins = {(folder + name, core_hash if name == 'viogpudxvk.dll' else digest, 'json' if name.endswith('.json') else '014c') for name, digest in PAYLOADS.items()}
    assert len(pins) == 4 and set(pins) == expected_pins
    derived = [row for row in rows if row.startswith('D3D8_DERIVED_ICD_PIN ')]
    if owned_icd_setup:
        expected_derived = (f'D3D8_DERIVED_ICD_PIN path={folder}freedreno_icd_owned_x86.json '
            'sha256=f50169e3e0efc6dea34fe0ce109228c79ce1df817a5508fb13a759c71d780ff3 '
            f'original_sha256={PAYLOADS["freedreno_icd.json"]} locked=1 original_bytes=0 '
            'library_path=.' + chr(92) + 'viogpu_gl_vk_x86.dll only_library_path_changed=1')
        assert derived == [expected_derived]
        opens = [i for i, row in enumerate(rows) if row.startswith('SYSTEM_D3D8_OPEN_BEGIN ')]
        assert opens and rows.index(expected_derived) < min(opens), 'derived ICD lock must precede backend construction'
    else:
        assert not derived, 'derived ICD requires explicit setup admission'
    source_rows = [r for r in rows if r.startswith('D3D8_HARDWARE_SOURCE ')]
    assert source_rows == [f'D3D8_HARDWARE_SOURCE core_commit={core_commit} ci_run={core_run} loader_source=6a6878c614c8c6dbe81ee7a9f1176bdb52dc7dd7 icd_source=8443c71a5ab32b9d58b904fa51f4bf2f9089db8d icd_ci_run=37453381660 driver_selection=owned-json raw_architecture=014c']
    modules = re.findall(r'^D3D8_PRIVATE_MODULE name=(\S+) path=(.+) machine=014c$', text, re.M)
    assert len(modules) == 6
    for name in ('viogpudxvk.dll', 'viogpu_gl_loader_x86.dll', 'viogpu_gl_vk_x86.dll'):
        assert modules.count((name, folder + name)) == 2
    assert len(re.findall(r'^D3D8_SELECTOR installed=1 machine=014c pointer_bytes=4 slot_rva=[0-9a-f]+ registry_writes=0$', text, re.M)) == 1
    restore = re.findall(r'^D3D8_SELECTOR restored=1 protection_restored=1 substitutions=(\d+) queries=(\d+)$', text, re.M)
    assert len(restore) == 1 and int(restore[0][0]) > 0 and int(restore[0][1]) >= int(restore[0][0])
    selected = re.findall(r'^D3D8_SELECTOR_QUERY index=\d+ type=1 bytes=524 kernel_adapter=[1-9]\d* original_status=00000000 selected=1 substitutions=[1-9]\d*$', text, re.M)
    assert selected, 'exact original name substitution absent'
    adapters = re.findall(r'^D3D8_ADAPTER index=\d+ identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=([0-9a-f]+) vs=([0-9a-f]+) ps=([0-9a-f]+) constants=(\d+)$', text, re.M)
    assert len(adapters) == 1 and int(adapters[0][0], 16) & 0x90000 == 0x90000
    assert adapters[0][1:] == ('fffe0101', 'ffff0104', '96'), 'bounded shader1.x profile'
    pixels = {}
    for row in rows:
        if not row.startswith('D3D8_PIXEL '): continue
        m = re.fullmatch(r'D3D8_PIXEL stage=(\d+) x=(\d+) y=(\d+) value=([0-9a-f]{8})', row); assert m
        stage,x,y = map(int, m.group(1,2,3)); key=(stage,x,y)
        assert key not in pixels and 1 <= stage <= 7 and 0 <= x < 8 and 0 <= y < 8
        pixels[key]=int(m[4],16); assert pixels[key] == COLORS[stage-1], key
    assert len(pixels) == 448
    calls = re.findall(r'^D3D8_API operation=(\S+) hr=([0-9a-f]{8})$', text, re.M)
    assert calls and all(hr == '00000000' for _,hr in calls)
    for name,count in {'CreateDevice-HAL-hardwareVP':1, 'CreateVertexShader-1.1':1,
                       'CreatePixelShader-1.1':1, 'CreatePixelShader-1.4':1,
                       'CreateTexture-dynamic':1, 'CopyRects-RT-to-systemmem':7, 'Reset':1,
                       'Present-owned-window':int(mode=='front-present')}.items():
        assert sum(n == name for n,_ in calls) == count, name
    creates = re.findall(r'^SYSTEM_D3D8_CREATE_RETURN runtime=(\S+) driver=(\S+) hr=00000000 interface=8 core_create_calls=1$', text, re.M)
    assert creates and all(runtime not in ('0','00000000','(nil)') and device not in ('0','00000000','(nil)') for runtime,device in creates)
    assert len(set(creates)) == len(creates)
    tables = re.findall(r'^SYSTEM_D3D8_CALLBACK_TABLE runtime=(\S+) adapter_runtime=(\S+) original=(\S+) wrapped=(\S+) bytes=88 owned_snapshot=1 borrowed_table_reread=0$', text, re.M)
    assert len(tables) == len(creates)
    assert len(re.findall(r'^SYSTEM_D3D8_DEVICE_FUNCTIONS bytes=396 interface=12 published=1$',text,re.M)) == len(creates)
    lifetime_pattern = (r'^SYSTEM_D3D8_LIFETIME phase=destroyed runtime=(\S+) allocate=(\d+) deallocate=(\d+) lock=(\d+) unlock=(\d+) create_context=(\d+) destroy_context=(\d+) render=(\d+) present=(\d+) residency=(\d+) live_allocations=0 live_locks=0 live_contexts=0 tracking_errors=0 callback_failures=0$')
    lifetimes = re.findall(lifetime_pattern,text,re.M); assert len(lifetimes) == len(creates)
    for fields in lifetimes:
        assert fields[0] in {x[0] for x in creates}
        a,d,l,u,c,dc,render,present,residency = map(int,fields[1:])
        assert a > 0 and a == d and l > 0 and l == u and c > 0 and c == dc and render > 0 and residency > 0
        if mode == 'front-present': assert present > 0
    destroyed = re.findall(r'^SYSTEM_D3D8_DEVICE_DESTROY device=(\S+) hr=00000000 remaining=\d+ callback_owner_released=1$',text,re.M)
    assert sorted(destroyed) == sorted(x[1] for x in creates)
    close = re.findall(r'^SYSTEM_D3D8_CLOSE adapter=\S+ runtime=\S+ hr=00000000 remaining=(\d+) live_devices=0$',text,re.M)
    assert close and close[-1]=='0'
    present = int(mode=='front-present')
    summary = f'D3D8_SELECTED_OFFSCREEN PASS stages=7 pixels=448 shader=VS1.1/PS1.1+PS1.4 dynamic_texture=1 resets=1 presents={present}'
    assert rows.count(summary)==1
    screen = {}
    for row in rows:
        if not row.startswith('D3D8_SCREEN_PIXEL '): continue
        m=re.fullmatch(r'D3D8_SCREEN_PIXEL x=(\d+) y=(\d+) rgb=([0-9a-f]{6})',row);assert m
        key=tuple(map(int,m.group(1,2))); assert key not in screen and all(0<=v<8 for v in key)
        screen[key]=int(m[3],16);assert screen[key]==0x193e72
    assert len(screen)==64*present
    assert len(re.findall(r'^D3D8_PRESENT PASS calls=1 pixels=64 rgb=193e72 polls=[1-9]\d* source=actual-screen$',text,re.M))==present
    complete=re.findall(rf'^D3D8_COMPLETE mode={mode} adapters=([1-9]\d*) create_device=1 presents={present} registry_writes=0$',text,re.M)
    assert len(complete)==1
    return {'status':'PASS','scope':'retained genuine system8 selected runtime stdout only; source/CIPE/token/SYS/dependency/readiness/process originals must be joined separately',
            'offscreen_pixels':448,'screen_pixels':64*present,'core_devices':len(creates),'mode':mode,'luid':luid,'source':source,
            **({'frontend_owned_through_runtime_teardown':True} if frontend_held else {})}


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stdout',type=Path);parser.add_argument('--mode',choices=['front-offscreen','front-present'],required=True)
    parser.add_argument('--luid',required=True);parser.add_argument('--source',type=int,required=True)
    args=parser.parse_args();print(json.dumps(verify(args.stdout.read_text(encoding='utf-8-sig'),args.mode,args.luid,args.source),indent=2))
