#!/usr/bin/env python3
"""Freeze two-object I386 probe-only native build; no target or CI operation."""
import argparse
import gzip
import hashlib
import io
import json
import subprocess
import tarfile
from pathlib import Path

INPUTS = [
    'tests/umd-d3d8-runtime-probe.cpp', 'tests/umd-d3d8-runtime-guard.cpp',
    'tests/umd-d3d8-runtime-guard.h', 'tests/umd-d3d8-api.h',
    'tests/umd-d3d8-runtime-policy.h', 'tests/umd-d3d8-runtime-hardware.h',
    'tests/umd-d3d8-system-identity.h', 'src/umd/umd_runtime_imports.h',
    'src/umd/umd_runtime_identity.h', 'src/umd/umd_identity.h',
    'scripts/build-native-d3d8-ordinary-probe.ps1',
    'scripts/collect-native-d3d8-ordinary-probe.ps1',
    'scripts/owned-raw-process-f4bf37f-02.cs',
    'scripts/verify-native-d3d8-ordinary-system.py',
    'scripts/verify-native-d3d8-system-phase.py',
    'docs/native-d3d8-ordinary-system-20261009.md',
]
RAW = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def pin(path):
    data = path.read_bytes()
    return {'path': str(path.resolve()), 'bytes': len(data), 'sha256': sha(data)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--legacy-archive', type=Path, required=True)
    parser.add_argument('--legacy-manifest', type=Path, required=True)
    parser.add_argument('--compiler-provenance', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    commit = subprocess.check_output(['git', '-C', str(args.source), 'rev-parse', args.commit]).decode().strip()
    sources, blobs = [], {}
    for name in INPUTS:
        data = subprocess.check_output(['git', '-C', str(args.source), 'show', commit + ':' + name])
        if (args.source / name).read_bytes() != data:
            raise ValueError('Working source differs from frozen Git source: ' + name)
        blobs[name] = data
        sources.append({'path': name, 'bytes': len(data), 'sha256': sha(data), 'git_commit': commit,
            'role': 'probe-native-build-source' if name.startswith(('tests/', 'src/')) else 'native-helper-or-unexecuted-reader-reference'})
    if sha(blobs['scripts/owned-raw-process-f4bf37f-02.cs']) != RAW:
        raise ValueError('Unchanged original bounded runner required')
    original = json.loads(args.legacy_manifest.read_text(encoding='utf-8-sig'))
    old = {row['path']: row for row in original['inputs']}
    with tarfile.open(args.legacy_archive, 'r:gz') as archive:
        for leaf in ('d3d8.h', 'd3d8caps.h', 'd3d8types.h'):
            name = 'dependencies/legacy-d3d8/' + leaf
            data = archive.extractfile(name).read()
            if old[name]['bytes'] != len(data) or old[name]['sha256'] != sha(data):
                raise ValueError('Original licensed legacy header differs')
            blobs[name] = data
            sources.append({'path': name, 'bytes': len(data), 'sha256': sha(data),
                'role': 'unchanged-licensed-legacy-header', 'original_input': old[name]})
    tools = json.loads(args.compiler_provenance.read_text(encoding='utf-8-sig'))
    if tools.get('ready') is not True or tools.get('file_count') != 575 or len(tools['files']) != 575:
        raise ValueError('Original actual 575-file official compiler provenance required')
    if args.output.exists():
        raise ValueError('Fresh packet output required')
    args.output.mkdir(parents=True)
    archive = args.output / 'native-ordinary-d3d8-probe-x86-source-01.tar.gz'
    with archive.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as packed:
        with tarfile.open(fileobj=packed, mode='w') as out:
            for name, data in sorted(blobs.items()):
                info = tarfile.TarInfo(name); info.size = len(data); info.mode = 0o644; info.mtime = 0
                out.addfile(info, io.BytesIO(data))
    compiler_pin = pin(args.compiler_provenance)
    manifest = {'schema': 'native-ordinary-d3d8-probe-x86-v1', 'source_commit': commit,
        'target_arch': 'x86', 'target_execution': 'deferred', 'scope': 'two I386 COFFs/one PE; no executable run',
        'inputs': sorted(sources, key=lambda row: row['path']), 'archive_sha256': sha(archive.read_bytes()),
        'archive_bytes': archive.stat().st_size, 'build_helper_sha256': sha(blobs['scripts/build-native-d3d8-ordinary-probe.ps1']),
        'collector_helper_sha256': sha(blobs['scripts/collect-native-d3d8-ordinary-probe.ps1']),
        'raw_process_helper_sha256': RAW, 'compiler_ready_sha256': compiler_pin['sha256'],
        'compiler_provenance': compiler_pin, 'compiler_root': tools['compiler_root'],
        'compiler_files': 575, 'compiler_current_target_revalidated': False,
        'groups': [{'name': 'probe', 'kind': 'exe', 'units': ['tests/umd-d3d8-runtime-probe.cpp',
            'tests/umd-d3d8-runtime-guard.cpp'], 'output': 'd3d8-runtime-probe.exe'}],
        'expected_original_coffs': 2, 'expected_original_pes': 1,
        'core_source_commit': None, 'core_ci_run': None, 'core_sha256': None,
        'core_binary_required_for_build': False, 'production_core_built': False,
        'system_runtime_calls': 0, 'name_queries': 0, 'gpu_calls': 0, 'registry_writes': 0,
        'executable_runs': 0, 'hardware_admission': False, 'default_replacement': False,
        'legacy_archive_original': pin(args.legacy_archive), 'legacy_manifest_original': pin(args.legacy_manifest),
        'preparer': pin(Path(__file__))}
    (args.output / 'native-ordinary-d3d8-probe-x86-source-01.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for name in ('build-native-d3d8-ordinary-probe.ps1', 'collect-native-d3d8-ordinary-probe.ps1', 'owned-raw-process-f4bf37f-02.cs'):
        (args.output / name).write_bytes(blobs['scripts/' + name])
    print(json.dumps({'source_commit': commit, 'inputs': len(sources), 'archive': pin(archive),
        'manifest': pin(args.output / 'native-ordinary-d3d8-probe-x86-source-01.json'), 'native_execution': 'pending'}, indent=2))


if __name__ == '__main__':
    main()
