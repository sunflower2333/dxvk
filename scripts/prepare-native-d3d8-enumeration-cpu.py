#!/usr/bin/env python3
"""Freeze a CPU-only internal-constructor diagnostic from exact Git objects.

The production reference is source only. This script neither needs a core
binary nor downloads, stages, loads, or admits one. Runtime inputs are admitted
separately by their actual original CI ZIP and full source/run/hash tuple.
"""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

CORE = 'de72dc2e97bd8e4ea70c5bf89c26918d06065723'
RAW = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
READY = 'c6333c67b4f3725f513e82c488b844054859b28456bccb0131eb59b805a5db48'
PRIMARY = [
    'tests/umd-d3d8-api.h', 'tests/umd-d3d8-runtime-front.cpp', 'tests/umd-d3d8-runtime-front.def',
    'tests/umd-d3d8-runtime-guard.cpp', 'tests/umd-d3d8-runtime-guard.h',
    'tests/umd-d3d8-runtime-policy.cpp', 'tests/umd-d3d8-runtime-policy.h',
    'tests/umd-d3d8-runtime-probe.cpp', 'tests/umd-d3d8-runtime-hardware.h',
    'tests/umd-d3d8-runtime-callbacks.cpp', 'tests/umd-d3d8-runtime-callbacks.h',
    'tests/umd-d3d8-runtime-enumeration.h', 'tests/umd-d3d8-system-identity.h',
    'src/umd/umd_runtime_imports.h', 'scripts/build-native-d3d8-runtime-device.ps1',
    'scripts/collect-native-d3d8-runtime-device.ps1', 'scripts/prepare-native-d3d8-enumeration-cpu.py',
    'scripts/parse-native-d3d8-runtime-device.ps1', 'scripts/verify-native-d3d8-system-device.py',
    'scripts/owned-raw-process-f4bf37f-02.cs',
]
REFERENCES = [
    'src/umd/umd_d3d9_adapter.cpp', 'src/umd/umd_d3d9_adapter.h',
    'src/umd/umd_d3d9_device.cpp', 'src/umd/umd_d3d9_backend.cpp', 'src/umd/umd_d3d9_backend.h',
    'src/umd/umd_d3d8_compat.cpp', 'src/umd/umd_d3d8_compat.h', 'src/umd/umd_legacy_api.h',
    'src/umd/mesa_wddm_runtime.h', 'src/umd/viogpudxvk.def', 'src/umd/meson.build', 'meson_options.txt',
]
GROUPS = [
    {'name': 'front', 'kind': 'dll', 'units': ['tests/umd-d3d8-runtime-front.cpp'],
     'def': 'tests/umd-d3d8-runtime-front.def', 'output': 'viogpu-d3d8-runtime-front.dll'},
    {'name': 'probe', 'kind': 'exe', 'units': ['tests/umd-d3d8-runtime-probe.cpp', 'tests/umd-d3d8-runtime-guard.cpp'],
     'output': 'd3d8-runtime-probe.exe'},
    {'name': 'policy', 'kind': 'exe', 'units': ['tests/umd-d3d8-runtime-policy.cpp'],
     'output': 'd3d8-runtime-policy.exe'},
    {'name': 'callbacks', 'kind': 'exe', 'units': ['tests/umd-d3d8-runtime-callbacks.cpp'],
     'output': 'd3d8-runtime-callbacks.exe'},
]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def row(path):
    data = path.read_bytes()
    return {'path': str(path.resolve()), 'bytes': len(data), 'sha256': digest(data)}


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--licensed-packet', type=Path, required=True)
    parser.add_argument('--compiler-ready-original', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    commit = git(args.source, 'rev-parse', args.commit).decode().strip()
    assert len(commit) == 40 and len(PRIMARY) == 20 and len(REFERENCES) == 12
    ready = json.loads(args.compiler_ready_original.read_text(encoding='utf-8-sig'))
    assert digest(args.compiler_ready_original.read_bytes()) == READY
    assert ready['ready'] and ready['file_count'] == len(ready['files']) == 575
    assert not args.output.exists()
    args.output.mkdir(parents=True)
    blobs, rows = {}, []
    for name in sorted(PRIMARY + REFERENCES):
        revision = commit if name in PRIMARY else CORE
        data = git(args.source, 'show', revision + ':' + name)
        blobs[name] = data
        rows.append({'path': name, 'bytes': len(data), 'sha256': digest(data), 'git_commit': revision,
                     'role': 'diagnostic-source-or-native-helper' if name in PRIMARY else 'uncompiled-production-reference'})
    original_manifest = args.licensed_packet / 'native-system-d3d8-device-x86-source-01.json'
    original_archive = args.licensed_packet / 'native-system-d3d8-device-x86-source-01.tar.gz'
    original = json.loads(original_manifest.read_text(encoding='utf-8-sig'))
    assert digest(original_archive.read_bytes()) == original['archive_sha256']
    licensed = [entry for entry in original['inputs'] if entry['path'].startswith('dependencies/')]
    assert {entry['path'] for entry in licensed} == {
        'dependencies/legacy-d3d8/d3d8.h', 'dependencies/legacy-d3d8/d3d8caps.h', 'dependencies/legacy-d3d8/d3d8types.h'}
    with tarfile.open(original_archive, 'r:gz') as archive:
        for entry in licensed:
            data = archive.extractfile(entry['path']).read()
            assert len(data) == entry['bytes'] and digest(data) == entry['sha256']
            blobs[entry['path']] = data
            rows.append({'path': entry['path'], 'bytes': len(data), 'sha256': digest(data),
                         'role': 'original-licensed-legacy-header', 'original_input': entry,
                         'original_receipt': row(original_manifest), 'original_source_archive': row(original_archive)})
    assert len(blobs) == len(rows) == 35 and digest(blobs['scripts/owned-raw-process-f4bf37f-02.cs']) == RAW
    archive_path = args.output / 'native-system-d3d8-device-x86-source-01.tar.gz'
    with archive_path.open('wb') as raw, gzip.GzipFile(filename='', fileobj=raw, mode='wb', mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w') as archive:
            for name, data in sorted(blobs.items()):
                info = tarfile.TarInfo(name)
                info.size, info.mode, info.mtime = len(data), 0o644, 0
                archive.addfile(info, io.BytesIO(data))
    manifest = {'schema': 'native-system-d3d8-device-x86-v1', 'source_commit': commit,
        'core_reference_commit': CORE, 'target_arch': 'x86', 'target_execution': 'deferred',
        'archive_sha256': digest(archive_path.read_bytes()), 'archive_bytes': archive_path.stat().st_size,
        'inputs': sorted(rows, key=lambda entry: entry['path']), 'harness_git_inputs': 20,
        'uncompiled_core_git_inputs': 12, 'licensed_legacy_inputs': 3, 'groups': GROUPS,
        'expected_original_coffs': 5, 'expected_original_pes': 4,
        'build_helper_sha256': digest(blobs['scripts/build-native-d3d8-runtime-device.ps1']),
        'collector_helper_sha256': digest(blobs['scripts/collect-native-d3d8-runtime-device.ps1']),
        'raw_process_helper_sha256': RAW, 'compiler_ready_sha256': READY, 'compiler_ready_files': 575,
        'compiler_ready_original': row(args.compiler_ready_original),
        'native_policy_checks': 503, 'native_callback_checks': 'derive actual stdout; forwarded=11',
        'native_enumeration_boundary': {'denied': 6, 'forwarded': 0, 'prefix': 99,
            'tail_unchanged': True, 'optional_null': True, 'teardown_allowed': True},
        'malformed_cli_guards': 32, 'readonly_process_api_observations': 1,
        'verified_loaded_modules': 3, 'readonly_file_pairs': 3,
        'expected_owned_build_children': 51, 'expected_owned_collector_children': 1,
        'SDK_hashed_headers': 13, 'official_link_libraries': 9, 'readonly_management_provider_pins': 3,
        'frontend_loaded_for_null_guards': True, 'production_core_required': False,
        'core_required_for_cpu_build': False, 'core_binary_built': False, 'core_loads': 0,
        'source_installation': False, 'repair_replay': False, 'runtime_factories': 0,
        'valid_KMT_queries': 0, 'gpu_runs': 0, 'ci_dispatches': 0,
        'scope': 'Strict native diagnostic compile, actual CPU policy/callback/typed-denial fixtures, '
                 '32 malformed CLI and frontend null guards, one alreadyloaded-module physical identity observation. '
                 'No genuine D3D8 factory, valid KMT, production core, runtime device/draw/Present or GPU.'}
    manifest_path = args.output / 'native-system-d3d8-device-x86-source-01.json'
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    for name in ['scripts/build-native-d3d8-runtime-device.ps1', 'scripts/collect-native-d3d8-runtime-device.ps1',
                 'scripts/parse-native-d3d8-runtime-device.ps1', 'scripts/owned-raw-process-f4bf37f-02.cs']:
        (args.output / Path(name).name).write_bytes(blobs[name])
    with tarfile.open(archive_path, 'r:gz') as archive:
        assert len(archive.getmembers()) == 35
        for entry in rows:
            data = archive.extractfile(entry['path']).read()
            assert len(data) == entry['bytes'] and digest(data) == entry['sha256']
    print(json.dumps({'archive': row(archive_path), 'manifest': row(manifest_path), 'ready': False,
                      'target_calls': 0, 'production_core_required': False}, indent=2))


if __name__ == '__main__':
    main()
