#!/usr/bin/env python3
"""Freeze committed read-only x86 source; no target, CI or staging operation."""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

PRIMARY = [
    'tests/umd-d3d8-api.h',
    'tests/umd-d3d8-runtime-front.cpp',
    'tests/umd-d3d8-runtime-front.def',
    'tests/umd-d3d8-runtime-guard.cpp',
    'tests/umd-d3d8-runtime-guard.h',
    'tests/umd-d3d8-runtime-policy.cpp',
    'tests/umd-d3d8-runtime-policy.h',
    'tests/umd-d3d8-runtime-probe.cpp',
    'src/umd/umd_runtime_imports.h',
    'scripts/build-native-d3d8-runtime-readonly.ps1',
    'scripts/collect-native-d3d8-runtime-readonly.ps1',
    'scripts/prepare-native-d3d8-runtime.py',
    'scripts/test-native-d3d8-runtime-policy.py',
    'docs/native-d3d8-system-runtime-20261007.md',
]
# These files are provenance references, not additional compiled translation
# units. The native packet never builds or runs an embedded GPU core.
CORE_REFERENCES = [
    'src/umd/umd_d3d9_adapter.cpp', 'src/umd/umd_d3d9_adapter.h',
    'src/umd/umd_d3d9_backend.cpp', 'src/umd/umd_d3d9_backend.h',
    'src/umd/umd_d3d8_compat.cpp', 'src/umd/umd_d3d8_compat.h',
    'src/umd/umd_legacy_api.h', 'src/umd/mesa_wddm_runtime.h',
    'src/umd/viogpudxvk.def', 'src/umd/meson.build', 'meson_options.txt',
]
CORE_REFERENCE_COMMIT = 'b75d6d583aa587da2f78b4e7183d3da14e5b373f'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--legacy-root', type=Path, required=True)
    parser.add_argument('--legacy-manifest', type=Path, required=True)
    parser.add_argument('--payload-audit', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    commit = git(args.source, 'rev-parse', args.commit).decode().strip()
    legacy_manifest = json.loads(args.legacy_manifest.read_text(encoding='utf-8-sig'))
    legacy_rows = {row['path']: row for row in legacy_manifest['inputs']}
    audit = json.loads(args.payload_audit.read_text(encoding='utf-8-sig'))
    assert audit['status'] == 'PASS'
    assert not args.output.exists(), 'fresh packet output required'
    args.output.mkdir(parents=True)
    blobs, rows = {}, []
    for name in sorted(PRIMARY + CORE_REFERENCES):
        role = 'harness-source' if name in PRIMARY else 'uncompiled-core-reference'
        revision = commit if name in PRIMARY else CORE_REFERENCE_COMMIT
        data = git(args.source, 'show', revision + ':' + name)
        # Core references must also match the committed diagnostic baseline.
        if role == 'uncompiled-core-reference':
            assert data == git(args.source, 'show', commit + ':' + name)
        blobs[name] = data
        rows.append({'path': name, 'bytes': len(data), 'sha256': sha(data),
                     'role': role, 'git_commit': revision})
    for leaf in ['d3d8.h', 'd3d8caps.h', 'd3d8types.h']:
        name = 'dependencies/legacy-d3d8/' + leaf
        data = (args.legacy_root / leaf).read_bytes()
        parent = legacy_rows[name]
        assert sha(data) == parent['sha256'] and len(data) == parent['bytes']
        blobs[name] = data
        rows.append({'path': name, 'bytes': len(data), 'sha256': sha(data),
                     'role': 'licensed-legacy-api-header', 'original_input': parent})
    archive = args.output / 'native-system-d3d8-readonly-x86-source-01.tar.gz'
    with archive.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w') as out:
            for name, data in sorted(blobs.items()):
                info = tarfile.TarInfo(name)
                info.size = len(data)
                info.mode = 0o644
                info.mtime = 0
                out.addfile(info, io.BytesIO(data))
    config = {
        'uncompiled_core_reference_commit': CORE_REFERENCE_COMMIT,
        'required_core_commit': None,
        'required_core_ci_run': None,
        'candidate_path': None,
        'candidate_sha256': None,
        'candidate_path_template': r'C:\Users\Public\DxvkD3D8Candidate-<commit7>-<actual-CI-run>\viogpudxvk.dll',
        'candidate_source': 'unchanged original I386 production DLL from the next successful exact-source consolidated CI',
        'permission': 'read-only-d3d8-interface8-v1',
        'process_local_pins': ['VIOGPU_DXVK_D3D8_CORE_PATH', 'VIOGPU_DXVK_D3D8_CORE_SHA256', 'VIOGPU_DXVK_D3D8_CORE_COMMIT'],
        'required_core_machine': 'IMAGE_FILE_MACHINE_I386',
        'hardware_loader_gate': 'verify actual future CI Meson loader configuration and match original I386 Khronos/ICD before rendering; no rebuild or relabel in this packet',
        'read_only_caps_creates_vulkan_device': False,
        'production_OpenAdapter_export': False,
        'runtime_CreateDevice_forwarding': False,
        'core_reference_source_available': True,
        'core_binary_built': False,
        'core_binary_modified': False,
        'core_required_for_cpu_build': False,
        'matching_core_artifact_available': False,
        'older_core_audit': audit['core_ci'],
        'loader': {key: audit['loader'][key] for key in ['path', 'bytes', 'sha256', 'machine']},
        'icd': {key: audit['icd'][key] for key in ['path', 'bytes', 'sha256', 'machine']},
        'icd_json': audit['icd_json'],
        'private_runtime_protocol_sha256': audit['matched_protocol_sha256'],
        'historical_audit_remaining_gates': audit['remaining_gates'],
        'payload_audit': {'path': str(args.payload_audit.resolve()), 'sha256': sha(args.payload_audit.read_bytes())},
        'payload_staged': False,
    }
    manifest = {
        'schema': 'native-system-d3d8-readonly-x86-v1', 'source_commit': commit,
        'core_reference_commit': CORE_REFERENCE_COMMIT, 'target_arch': 'x86', 'target_execution': 'deferred',
        'scope': 'compile readonly frontend/probe; native policy, null/invalid caller guards and malformed CLI only',
        'archive_sha256': sha(archive.read_bytes()), 'archive_bytes': archive.stat().st_size,
        'inputs': sorted(rows, key=lambda row: row['path']),
        'harness_git_inputs': len(PRIMARY), 'uncompiled_core_git_inputs': len(CORE_REFERENCES),
        'licensed_legacy_inputs': 3, 'expected_original_coffs': 4, 'expected_original_pes': 3,
        'build_helper_sha256': sha(blobs['scripts/build-native-d3d8-runtime-readonly.ps1']),
        'collector_helper_sha256': sha(blobs['scripts/collect-native-d3d8-runtime-readonly.ps1']),
        'groups': [
            {'name': 'front', 'kind': 'dll', 'units': ['tests/umd-d3d8-runtime-front.cpp'],
             'def': 'tests/umd-d3d8-runtime-front.def', 'output': 'viogpu-d3d8-runtime-front.dll'},
            {'name': 'probe', 'kind': 'exe', 'units': ['tests/umd-d3d8-runtime-probe.cpp',
                 'tests/umd-d3d8-runtime-guard.cpp'], 'output': 'd3d8-runtime-probe.exe'},
            {'name': 'policy', 'kind': 'exe', 'units': ['tests/umd-d3d8-runtime-policy.cpp'],
             'output': 'd3d8-runtime-policy.exe'},
        ],
        'native_policy_checks': 306, 'malformed_cli_guards': 14,
        'compiler_host_policy': 'prefer official Hostarm64/x86; official Hostx64/x86 under ARM64 emulation is permitted and must be labeled',
        'legacy_source_manifest': {'path': str(args.legacy_manifest.resolve()), 'sha256': sha(args.legacy_manifest.read_bytes())},
        'candidate_configuration': config,
        'source_installation': False, 'gpu_runs': 0, 'selector_execution': False, 'ci_dispatches': 0,
    }
    (args.output / 'native-system-d3d8-readonly-x86-source-01.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (args.output / 'matched-x86-payload-requirements-01.json').write_text(json.dumps(config, indent=2) + '\n')
    for name in ['build-native-d3d8-runtime-readonly.ps1', 'collect-native-d3d8-runtime-readonly.ps1']:
        (args.output / name).write_bytes(blobs['scripts/' + name])
    print(json.dumps({'source_commit': commit, 'archive': str(archive.resolve()),
                      'archive_sha256': manifest['archive_sha256'], 'inputs': len(rows),
                      'harness_git_inputs': len(PRIMARY), 'uncompiled_core_git_inputs': len(CORE_REFERENCES),
                      'target_execution': 'deferred'}, indent=2))


if __name__ == '__main__':
    main()
