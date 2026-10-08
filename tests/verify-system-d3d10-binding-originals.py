#!/usr/bin/env python3
"""Literal D10 binding progress-runner readback reader; no hardware admission."""
import argparse
import hashlib
import json
import ntpath
from pathlib import Path
import re


def windows_path(value):
    assert type(value) is str and re.match(r'^[A-Za-z]:\\', value)
    return ntpath.normcase(ntpath.normpath(value))


def verify(directory, stdout_path, process_path, api, luid_high, luid_low, frontend, core, system_directory):
    directory = Path(directory)
    wanted = {'clear.raw', 'draw.raw', 'manifest.json'}
    assert {path.name for path in directory.iterdir()} == wanted
    assert all(path.is_file() and not path.is_symlink() for path in directory.iterdir())
    stdout_path, process_path = Path(stdout_path), Path(process_path)
    assert not stdout_path.resolve().is_relative_to(directory.resolve())
    assert not process_path.resolve().is_relative_to(directory.resolve())
    process = json.loads(process_path.read_text(encoding='utf-8-sig'))
    assert type(process['pid']) is int and process['pid'] > 0
    assert type(process['retained_process_handle']) is int and process['retained_process_handle'] > 0
    assert process['exited'] is True and process['exit_code_available'] is True
    assert type(process['exit_code']) is int and process['exit_code'] == 0
    assert process['timed_out'] is False and process['child_still_running'] is False and process['pipes_drained'] is True
    assert process['capture_failure'] is None and type(process['stdout_bytes']) is int and process['stdout_bytes'] == stdout_path.stat().st_size
    assert type(process['stderr_bytes']) is int and process['stderr_bytes'] == 0
    assert process['expected_exit'] == 0 and process['start_utc']
    assert process['runner_sha256'] == '7def540f912623e6e4a4925bf3e747e0cf617327367d659bc2cd8e41a9c69513'
    manifest = json.loads((directory / 'manifest.json').read_text())
    fixed = dict(schema=1, api=api, width=16, height=16, frames=2, pixels=512, presents=2,
                 vendor=0x1af4, device=0x1050, luidHigh=luid_high, luidLow=luid_low, entryInterface=0x000a0002 if api == 101 else 0x000a0001, entryResult=0)
    extra = {'generation', 'capabilities', 'frontend', 'core', 'entryCalls', 'successfulEntryCalls', 'entryVersion',
             'softwareFallback', 'unregisteredValidationCandidate', 'productionAdmission', 'registrationChangedByProbe', 'frameResults', 'systemModules'}
    if api == 101:
        extra |= {'profile', 'factory', 'featureLevel'}
    assert set(manifest) == set(fixed) | extra
    assert api in (10, 101) and (luid_high or luid_low)
    if api == 101:
        assert manifest['profile'] == '10_1' and manifest['factory'] == 'D3D10CreateDevice1'
        assert type(manifest['featureLevel']) is int and manifest['featureLevel'] == 0xa100
    assert 0 <= luid_high <= (1 << 32)-1 and 0 <= luid_low <= (1 << 32)-1
    assert all(type(manifest[key]) is int and manifest[key] == value for key, value in fixed.items())
    for key in ('generation', 'entryCalls', 'successfulEntryCalls'):
        assert type(manifest[key]) is int and 0 < manifest[key] <= (1 << 64) - 1
    assert manifest['successfulEntryCalls'] <= manifest['entryCalls'] <= (1 << 31)-1
    assert type(manifest['capabilities']) is int and 0 <= manifest['capabilities'] <= (1 << 64)-1
    assert type(manifest['entryVersion']) is int and (1 if api == 101 else 4) <= manifest['entryVersion'] >> 16 <= 0xffff
    assert manifest['softwareFallback'] is False and manifest['productionAdmission'] is False
    assert manifest['registrationChangedByProbe'] is False and manifest['unregisteredValidationCandidate'] is True
    assert type(manifest['frameResults']) is list and len(manifest['frameResults']) == 2
    for frame in manifest['frameResults']:
        assert set(frame) == {'map', 'readbackRemoved', 'present', 'presentRemoved', 'rowPitch'}
        assert all(type(frame[key]) is int and frame[key] == 0 for key in ('map', 'readbackRemoved', 'present', 'presentRemoved'))
        assert type(frame['rowPitch']) is int and 64 <= frame['rowPitch'] <= (1 << 32)-1
    assert windows_path(manifest['frontend']) == windows_path(frontend)
    assert windows_path(manifest['core']) == windows_path(core)
    modules = manifest['systemModules']
    assert set(modules) == {'dxgi.dll', 'gdi32.dll', 'd3dcompiler_47.dll', 'd3d10.dll' if api == 10 else 'd3d10_1.dll'}
    for name, path in modules.items():
        assert windows_path(path) == windows_path(ntpath.join(system_directory, name))
    originals = []
    for name, literal in (('clear.raw', bytes((0, 0, 0, 255))), ('draw.raw', bytes((255, 0, 0, 255)))):
        path = directory / name; raw = path.read_bytes()
        assert raw == literal * 256, name
        originals.append(dict(name=name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
    raw_manifest = (directory / 'manifest.json').read_bytes()
    originals.append(dict(name='manifest.json', bytes=len(raw_manifest), sha256=hashlib.sha256(raw_manifest).hexdigest()))
    stdout = stdout_path.read_text(encoding='utf-8-sig')
    assert f'SYSTEM_RUNTIME_VALIDATION_DRAW_READBACK_PRESENT api={api} pixels=512 presents=2 software_fallback=0' in stdout.splitlines()
    assert f'SYSTEM_RUNTIME_VALIDATION_PASS api={api} pixels=512 presents=2 production_admission=0 registry_changes=0' in stdout.splitlines()
    held = re.findall(r'^SYSTEM_RUNTIME_VALIDATION_HELD pid=(\d+) timeout_ms=(\d+) pending_exit=(\d+) registry_restoration_not_proved_by_event=1$', stdout, re.M)
    assert len(held) == 1 and int(held[0][0]) == process['pid'] and 0 < int(held[0][1]) <= 60000 and int(held[0][2]) == 0
    assert f'SYSTEM_VALIDATION_SELECTED api={api} luid={luid_high:08x}:{luid_low:08x}' in stdout
    assert 'SYSTEM_VALIDATION_FAIL' not in stdout
    return dict(verified=True, api=api, frames=2, pixels=512, presents=2, raw_files=3, literal_bytes=2048,
                originals=originals, actual_process_id=process['pid'],
                scope='Unregistered candidate ordinary SYSTEM factory/readback/Present slice; source/module/registry restoration and KMD GPU originals require ROOT joins.',
                registry_restoration_proved=False, production_admission=False, hardware_admission=False, registration=False)


def main():
    parser = argparse.ArgumentParser()
    for name in ('directory', 'stdout', 'process', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--api', type=int, choices=(10, 101), required=True)
    for name in ('luid-high', 'luid-low'):
        parser.add_argument('--' + name, type=lambda value: int(value, 16), required=True)
    for name in ('frontend', 'core', 'system-directory'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    assert not args.output.resolve().is_relative_to(args.directory.resolve())
    result = verify(args.directory, args.stdout, args.process, args.api, args.luid_high, args.luid_low, args.frontend, args.core, args.system_directory)
    with args.output.open('x') as file:
        json.dump(result, file, indent=2); file.write('\n')
    print(f'SYSTEM runtime validation originals PASS api={args.api} frames=2 pixels=512 presents=2 files=3 bytes=2048 hardware_admission=0')


if __name__ == '__main__':
    main()
