#!/usr/bin/env python3
"""Independent ordinary swapchain readback/Present reader; no driver admission."""
import argparse
import hashlib
import json
import ntpath
from pathlib import Path
import re
import struct

DDI11 = 0x000b000a
SUPPORTED11 = (DDI11 << 32) | (2 << 16)
RUNNER = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
MARKER = 'SYSTEM_D3D11_PRESENT_VALIDATION_PASS feature_level=10_0 typed_ddi=11 pixels=512 presents=2 software_fallback=0 production_admission=0 registry_changes=0'


def windows_path(value):
    assert type(value) is str and re.fullmatch(r'[A-Za-z]:\\[^\r\n]+', value)
    return ntpath.normcase(ntpath.normpath(value))


def integer(value, minimum=0, maximum=(1 << 64) - 1):
    assert type(value) is int and minimum <= value <= maximum
    return value


def shader_container(raw, stage):
    assert len(raw) >= 32 and raw[:4] == b'DXBC' and struct.unpack_from('<I', raw, 24)[0] == len(raw)
    count = struct.unpack_from('<I', raw, 28)[0]
    assert 3 <= count <= 32 and 32 + 4*count <= len(raw)
    chunks, spans = {}, []
    for index in range(count):
        start = struct.unpack_from('<I', raw, 32 + 4*index)[0]
        assert start >= 32 + 4*count and start + 8 <= len(raw)
        size = struct.unpack_from('<I', raw, start + 4)[0]
        assert start + 8 + size <= len(raw)
        assert all(start + 8 + size <= old_start or start >= old_end for old_start, old_end in spans)
        spans.append((start, start + 8 + size))
        tag = raw[start:start+4]
        assert tag not in chunks
        chunks[tag] = raw[start+8:start+8+size]
    assert b'ISGN' in chunks and b'OSGN' in chunks
    code = [chunks[tag] for tag in (b'SHDR', b'SHEX') if tag in chunks]
    assert len(code) == 1 and len(code[0]) >= 8 and len(code[0]) % 4 == 0
    version, words = struct.unpack_from('<II', code[0])
    assert version == (stage << 16) | 0x40 and words * 4 == len(code[0])


def negotiation(data, core, closed):
    assert set(data) == {'schema', 'core', 'liveAdapters', 'events'}
    assert integer(data['schema']) == 1 and windows_path(data['core']) == windows_path(core)
    integer(data['liveAdapters'], 0, 32)
    if closed:
        assert data['liveAdapters'] == 0
    assert type(data['events']) is list and 1 <= len(data['events']) <= 128
    event_keys = {'sequence', 'call', 'result', 'interface', 'version', 'flags', 'type', 'dataSize', 'capacity', 'count',
                  'caps', 'argument', 'adapter', 'runtimeAdapter', 'kernelCallbacks', 'coreCallbacks', 'returnSize', 'versions'}
    handles, retired, successful_creates = set(), set(), []
    pipeline, version_list = False, False
    for index, event in enumerate(data['events'], 1):
        assert set(event) == event_keys and integer(event['sequence']) == index
        for key in event_keys - {'result', 'versions'}:
            integer(event[key])
        integer(event['result'], -(1 << 31), (1 << 31)-1)
        assert type(event['versions']) is list and len(event['versions']) <= 8
        kind = integer(event['call'], 0, 5)
        if kind == 0 and event['result'] == 0:
            assert event['argument'] and event['runtimeAdapter'] and event['adapter']
            assert event['adapter'] not in handles
            handles.add(event['adapter'])
        elif kind != 0:
            assert event['adapter'] in handles
        if kind == 1 and event['result'] == 0:
            assert event['count'] == 1
            if event['versions']:
                assert event['capacity'] >= 1 and event['versions'] == [SUPPORTED11]
                version_list = True
        if kind == 2 and event['result'] == 0:
            assert event['dataSize'] == 4
            if event['type'] == 130:
                assert event['caps'] == 1
                pipeline = True
            elif event['type'] in (128, 129):
                assert event['caps'] == 0
            else:
                raise AssertionError('unknown successful caps response')
        if kind in (3, 4):
            assert event['interface'] == DDI11 and 2 <= event['version'] >> 16 <= 0xffff
            # Exact existing typed11 flags: level10_0 is zero, SINGLETHREADED
            # may be present. No other feature level/threading flags admitted.
            assert not event['flags'] & ~0x10
        if kind == 4 and event['result'] == 0:
            assert event['argument'] and event['kernelCallbacks'] and event['coreCallbacks']
            successful_creates.append(event)
        if kind == 5 and event['result'] == 0:
            assert event['adapter'] not in retired
            retired.add(event['adapter'])
    assert pipeline and version_list and successful_creates
    if closed:
        assert handles == retired
    return successful_creates


def verify(directory, stdout_path, process_path, held_path, luid_high, luid_low, frontend, core, loader, icd, icd_json, system_directory):
    directory = Path(directory)
    names = {'identity.raw', 'clear.raw', 'draw.raw', 'vs.dxbc', 'ps.dxbc', 'negotiation.json', 'closed-negotiation.json', 'manifest.json'}
    assert {path.name for path in directory.iterdir()} == names
    assert all(path.is_file() and not path.is_symlink() for path in directory.iterdir())
    stdout_path, process_path = Path(stdout_path), Path(process_path)
    assert not stdout_path.resolve().is_relative_to(directory.resolve())
    assert not process_path.resolve().is_relative_to(directory.resolve())
    process = json.loads(process_path.read_text(encoding='utf-8-sig'))
    integer(process['pid'], 1); integer(process['retained_process_handle'], 1)
    assert process['exited'] is True and process['exit_code_available'] is True and integer(process['exit_code']) == 0
    assert process['timed_out'] is False and process['child_still_running'] is False and process['pipes_drained'] is True
    assert process['capture_failure'] is None and process['runner_sha256'] == RUNNER
    assert integer(process['stdout_bytes']) == stdout_path.stat().st_size
    integer(process['stderr_bytes']); assert process['start_utc']
    manifest = json.loads((directory / 'manifest.json').read_text())
    assert set(manifest) == {'schema', 'api', 'passed', 'failure', 'failureResult', 'factoryCalled', 'factoryResult', 'featureLevel',
        'luidHigh', 'luidLow', 'generation', 'capabilities', 'frontend', 'core', 'privateLoader', 'icd', 'icdJson', 'effective',
        'heldReleased', 'kmtClose', 'pixels', 'softwareFallback', 'productionAdmission', 'registrationChangedByProbe', 'presents', 'window', 'swapchain', 'readbacks', 'systemModules', 'loadedModules'}
    fixed = dict(schema=1, api=11, failureResult=0, factoryResult=0, featureLevel=0xa000,
                 luidHigh=luid_high, luidLow=luid_low, kmtClose=0, pixels=512, presents=2)
    integer(luid_high, 0, 0xffffffff); integer(luid_low, 0, 0xffffffff)
    assert luid_high or luid_low
    assert all(integer(manifest[key]) == value for key, value in fixed.items())
    assert manifest['passed'] is True and manifest['factoryCalled'] is True and manifest['heldReleased'] is True
    assert manifest['failure'] == '' and manifest['softwareFallback'] is False
    assert manifest['productionAdmission'] is False and manifest['registrationChangedByProbe'] is False
    for key, value in dict(frontend=frontend, core=core, privateLoader=loader, icd=icd, icdJson=icd_json, effective=frontend).items():
        assert windows_path(manifest[key]) == windows_path(value)
    assert set(manifest['loadedModules']) == {'frontend', 'core', 'privateLoader', 'icd'}
    for key, value in dict(frontend=frontend, core=core, privateLoader=loader, icd=icd).items():
        assert windows_path(manifest['loadedModules'][key]) == windows_path(value)
    assert set(manifest['systemModules']) == {'dxgi.dll', 'd3d11.dll', 'gdi32.dll', 'd3dcompiler_47.dll', 'user32.dll'}
    for name, value in manifest['systemModules'].items():
        assert windows_path(value) == windows_path(ntpath.join(system_directory, name))
    assert type(manifest['readbacks']) is list and len(manifest['readbacks']) == 2
    for frame in manifest['readbacks']:
        assert set(frame) == {'map', 'removed', 'rowPitch', 'present', 'presentRemoved', 'syncInterval', 'presentFlags', 'windowedBefore', 'windowedAfter'}
        assert integer(frame['map']) == 0 and integer(frame['removed']) == 0
        integer(frame['rowPitch'], 64, 0xffffffff)
        assert integer(frame['present']) == integer(frame['presentRemoved']) == integer(frame['presentFlags']) == 0
        assert integer(frame['syncInterval']) == 1
        assert frame['windowedBefore'] is True and frame['windowedAfter'] is True
    window = manifest['window']
    assert set(window) == {'handle', 'pid', 'thread', 'session', 'destroyed', 'classUnregistered', 'associationResult', 'associationFlags'}
    assert integer(window['associationResult']) == 0 and integer(window['associationFlags']) == 3
    integer(window['handle'], 1); integer(window['thread'], 1); integer(window['session'], 1, 0xffffffff-1)
    assert integer(window['pid'], 1) == process['pid'] and window['destroyed'] is True and window['classUnregistered'] is True
    swapchain = manifest['swapchain']
    assert set(swapchain) == {'captured', 'width', 'height', 'format', 'sampleCount', 'sampleQuality', 'usage',
                             'bufferCount', 'outputWindow', 'windowed', 'effect', 'flags'}
    assert swapchain['captured'] is True and swapchain['windowed'] is True
    expected = dict(width=16, height=16, format=28, sampleCount=1, sampleQuality=0, usage=32, bufferCount=1,
                    outputWindow=window['handle'], effect=0, flags=0)
    assert all(integer(swapchain[key]) == value for key, value in expected.items())
    identity = (directory / 'identity.raw').read_bytes()
    assert len(identity) == 160
    u32 = lambda offset: struct.unpack_from('<I', identity, offset)[0]
    u64 = lambda offset: struct.unpack_from('<Q', identity, offset)[0]
    assert tuple(u32(i) for i in (0,4,8,12,128,132,136,140,152,156)) == (0x504d5644,0,128,0,0x44494c56,1,32,1,1,0)
    assert u64(24) == integer(manifest['generation'], 1) and u64(16) == integer(manifest['capabilities'])
    assert u64(112) == u64(120) == 0 and u32(144) == luid_low and u32(148) == luid_high
    before = json.loads((directory / 'negotiation.json').read_text())
    after = json.loads((directory / 'closed-negotiation.json').read_text())
    creates = negotiation(before, core, False); negotiation(after, core, True)
    assert after['events'][:len(before['events'])] == before['events']
    for name, pixel in (('clear.raw', bytes((0, 0, 0, 255))), ('draw.raw', bytes((255, 0, 0, 255)))):
        assert (directory / name).read_bytes() == pixel * 256, name
    for name, stage in (('vs.dxbc', 1), ('ps.dxbc', 0)):
        shader_container((directory / name).read_bytes(), stage)
    lines = stdout_path.read_text(encoding='utf-8-sig').splitlines()
    assert MARKER in lines
    held = [re.fullmatch(r'SYSTEM_D3D11_HELD pid=(\d+) timeout_ms=(\d+) pixels_passed=1 stage= hr=00000000', line) for line in lines]
    held = [match for match in held if match]
    assert len(held) == 1 and int(held[0][1]) == process['pid'] and 0 < int(held[0][2]) <= 60000
    held_path = Path(held_path)
    assert held_path.is_file() and not held_path.is_symlink()
    assert held_path.resolve() == directory.resolve().with_name(directory.name + '.held.json')
    checkpoint = json.loads(held_path.read_text(encoding='utf-8-sig'))
    assert set(checkpoint) == {'schema', 'api', 'pid', 'event', 'timeout_ms', 'output', 'factoryCalled', 'factoryResult', 'pixelsPassed', 'stage', 'result'}
    assert integer(checkpoint['schema']) == 1 and integer(checkpoint['api']) == 11
    assert integer(checkpoint['pid']) == process['pid'] and integer(checkpoint['timeout_ms']) == int(held[0][2])
    assert type(checkpoint['event']) is str and re.fullmatch(r'Local\\[^\r\n]+', checkpoint['event'])
    assert checkpoint['factoryCalled'] is True and integer(checkpoint['factoryResult']) == 0
    assert checkpoint['pixelsPassed'] is True and checkpoint['stage'] == '' and integer(checkpoint['result']) == 0
    assert windows_path(checkpoint['output']).rsplit('\\', 1)[1] == directory.name.lower()
    originals = []
    for name in sorted(names):
        raw = (directory / name).read_bytes()
        originals.append(dict(name=name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
    return dict(verified=True, scope='ordinary-SYSTEM-D3D11-FL10_0-windowed-present-originals', pixels=512, literal_bytes=2048,
                typed_ddi=DDI11, creates=len(creates), originals=originals,
                held_original=dict(bytes=held_path.stat().st_size, sha256=hashlib.sha256(held_path.read_bytes()).hexdigest()),
                hardware_admission=False, registration_restoration=False, production_admission=False, present_proof=True, presents=2, desktop_pixels=False, dwm_hardware=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('directory', 'stdout', 'process', 'held', 'frontend', 'core', 'loader', 'icd', 'icd-json', 'system-directory', 'output'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--luid-high', required=True, type=lambda value: int(value, 0))
    parser.add_argument('--luid-low', required=True, type=lambda value: int(value, 0))
    args = parser.parse_args()
    result = verify(args.directory, args.stdout, args.process, args.held, args.luid_high, args.luid_low,
                    args.frontend, args.core, args.loader, args.icd, args.icd_json, args.system_directory)
    path = Path(args.output)
    with path.open('x') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    print('PASS actual ordinary SYSTEM D3D11 Present originals: typed11 FL10_0, 512 backbuffer pixels, two S_OK Presents; admission/restoration/DWM separate.')
