#!/usr/bin/env python3
"""Freeze exact Git diagnostic sources and rejoin retained original I386 inputs.

This is local preparation only. It neither downloads nor stages payloads and
never executes a Windows program, registry operation or graphics factory.
"""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess
import tarfile
import zipfile

CORE = 'd7e5c7d46b8ce889e993bfab66a3b78b076c49d1'
RUN = 37648387721
PRIMARY = [
    'tests/umd-d3d8-api.h', 'tests/umd-d3d8-runtime-front.cpp', 'tests/umd-d3d8-runtime-front.def',
    'tests/umd-d3d8-runtime-guard.cpp', 'tests/umd-d3d8-runtime-guard.h',
    'tests/umd-d3d8-runtime-policy.cpp', 'tests/umd-d3d8-runtime-policy.h',
    'tests/umd-d3d8-runtime-probe.cpp', 'tests/umd-d3d8-runtime-hardware.h',
    'tests/umd-d3d8-system-identity.h',
    'tests/umd-d3d8-runtime-callbacks.cpp', 'tests/umd-d3d8-runtime-callbacks.h',
    'src/umd/umd_runtime_imports.h', 'scripts/build-native-d3d8-runtime-device.ps1',
    'scripts/collect-native-d3d8-runtime-device.ps1', 'scripts/prepare-native-d3d8-runtime-device.py',
    'scripts/parse-native-d3d8-runtime-device.ps1', 'scripts/verify-native-d3d8-system-device.py',
    'scripts/owned-raw-process-f4bf37f-02.cs',
]
REFERENCES = [
    'src/umd/umd_d3d9_adapter.cpp', 'src/umd/umd_d3d9_adapter.h',
    'src/umd/umd_d3d9_device.cpp', 'src/umd/umd_d3d9_backend.cpp', 'src/umd/umd_d3d9_backend.h',
    'src/umd/umd_d3d8_compat.cpp', 'src/umd/umd_d3d8_compat.h', 'src/umd/umd_legacy_api.h',
    'src/umd/mesa_wddm_runtime.h', 'src/umd/viogpudxvk.def', 'src/umd/meson.build', 'meson_options.txt',
]
HASHES = {'viogpudxvk.dll': '7be8cbb9850407ccc304528911a6fbd71b01971fbeb4a86550cc8dcc2f346a3f',
          'viogpu_gl_loader_x86.dll': 'd459f2d09080865cc3d591b498c02d38305a26963b401152f8230dc60c5ad7e7',
          'viogpu_gl_vk_x86.dll': '2b549889816163433faabe6f2c1d2a61d6c106078d08e30031b74c0a66cd7f5c',
          'freedreno_icd.json': '74d7d5d6ae9432cde2d802507ed59bbe4c2f2b95d01e7ac3c2b56e9691932c80'}


def sha(data): return hashlib.sha256(data).hexdigest()
def row(path):
    data = path.read_bytes()
    return {'path': str(path.resolve()), 'bytes': len(data), 'sha256': sha(data)}
def load(path): return json.loads(path.read_text(encoding='utf-8-sig'))
def git(root, *args): return subprocess.check_output(['git', '-C', str(root), *args])
def pe(data):
    assert data[:2] == b'MZ'
    offset = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[offset:offset + 4] == b'PE\0\0'
    machine = struct.unpack_from('<H', data, offset + 4)[0]
    optional = offset + 24
    magic = struct.unpack_from('<H', data, optional)[0]
    assert machine == 0x14c and magic == 0x10b
    security_offset, security_size = struct.unpack_from('<II', data, optional + 96 + 8 * 4)
    assert not security_size or security_offset + security_size <= len(data)
    return {'machine': 'I386', 'optional_header': 'PE32', 'security_directory_offset': security_offset,
            'security_directory_bytes': security_size,
            'embedded_signature_present': bool(security_size),
            'authenticode_trust_verified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True); parser.add_argument('--commit', required=True)
    parser.add_argument('--legacy-packet', type=Path, required=True)
    parser.add_argument('--ci-directory', type=Path, required=True)
    parser.add_argument('--mesa-audit', type=Path, required=True)
    parser.add_argument('--user-identity', type=Path, required=True)
    parser.add_argument('--policy-checks', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    commit = git(args.source, 'rev-parse', args.commit).decode().strip(); assert len(commit) == 40
    assert args.policy_checks > 306
    assert not args.output.exists(); args.output.mkdir(parents=True)
    legacy_manifest = load(args.legacy_packet/'native-system-d3d8-readonly-x86-source-01.json')
    legacy_archive = args.legacy_packet/'native-system-d3d8-readonly-x86-source-01.tar.gz'
    assert sha(legacy_archive.read_bytes()) == legacy_manifest['archive_sha256']
    legacy_rows = {r['path']: r for r in legacy_manifest['inputs']}
    blobs, sources = {}, []
    for name in sorted(PRIMARY + REFERENCES):
        revision = commit if name in PRIMARY else CORE
        data = git(args.source, 'show', revision + ':' + name)
        if name in REFERENCES: assert data == git(args.source, 'show', commit + ':' + name)
        blobs[name] = data
        sources.append({'path': name, 'bytes': len(data), 'sha256': sha(data), 'git_commit': revision,
                        'role': 'diagnostic-build-source' if name in PRIMARY else 'uncompiled-production-reference'})
    with tarfile.open(legacy_archive, 'r:gz') as archive:
        for leaf in ['d3d8.h', 'd3d8caps.h', 'd3d8types.h']:
            name = 'dependencies/legacy-d3d8/' + leaf
            data = archive.extractfile(name).read(); original = legacy_rows[name]
            assert sha(data) == original['sha256'] and len(data) == original['bytes']
            blobs[name] = data; sources.append({'path': name, 'bytes': len(data), 'sha256': sha(data),
                'role': 'original-licensed-legacy-header', 'original_input': original})
    run = load(args.ci_directory/'ci-run.api.json'); jobs = load(args.ci_directory/'ci-jobs.api.json')
    assert run['id'] == RUN and run['head_sha'] == CORE and run['conclusion'] == 'success'
    assert len(jobs['jobs']) == 6 and all(j['conclusion'] == 'success' for j in jobs['jobs'])
    accepted_path = args.ci_directory/'root-current-native-ci-verified-01.json'
    accepted = load(accepted_path)
    assert accepted['verified'] and accepted['source_commit'] == CORE and accepted['run'] == RUN
    assert accepted['successful_jobs'] == 6
    api = load(args.ci_directory/'ci-artifacts.json')
    artifacts = [r for r in api['artifacts'] if r['name'] == 'dxvk-umd-backend-x86-' + CORE]; assert len(artifacts) == 1
    artifact = artifacts[0]; assert artifact['workflow_run']['id'] == RUN and artifact['workflow_run']['head_sha'] == CORE
    core_zip = args.ci_directory/'_archives'/(artifact['name']+'.zip')
    assert core_zip.stat().st_size == artifact['size_in_bytes'] and 'sha256:'+sha(core_zip.read_bytes()) == artifact['digest']
    with zipfile.ZipFile(core_zip) as z:
        assert z.testzip() is None
        names = z.namelist(); assert len(names) == len(set(names))
        configuration = json.loads(z.read('native-build-configuration.json'))
        assert configuration['source_commit'] == CORE and configuration['arch'] == 'x86'
        assert configuration['github']['run_id'] == str(RUN) and configuration['github']['sha'] == CORE
        core = z.read('viogpudxvk.dll'); assert sha(core) == HASHES['viogpudxvk.dll']
        private = z.read('vulkan_loader_config.h').decode()
        assert '#define DXVK_PRIVATE_VULKAN_LOADER "viogpu_gl_loader_x86.dll"' in private
        core_identity = {**row(core_zip), 'artifact_id': artifact['id'], 'artifact_name': artifact['name'],
            'member': 'viogpudxvk.dll', 'member_bytes': len(core), 'member_sha256': sha(core), **pe(core),
            'private_config_member': 'vulkan_loader_config.h', 'private_config_sha256': sha(z.read('vulkan_loader_config.h'))}
    audit = load(args.mesa_audit)
    assert audit['status'] == 'PASS' and audit['mesa_ci']['commit'] == '8443c71a5ab32b9d58b904fa51f4bf2f9089db8d'
    assert audit['mesa_ci']['run'] == 37453381660 and audit['mesa_artifact']['id'] == 11408567654
    mesa_zip = Path(audit['mesa_raw_zip']); assert row(mesa_zip)['bytes'] == audit['mesa_artifact']['size_in_bytes']
    assert 'sha256:'+row(mesa_zip)['sha256'] == audit['mesa_artifact']['digest']
    payloads = []
    with zipfile.ZipFile(mesa_zip) as z:
        assert z.testzip() is None and z.read('source-commit.txt').decode().strip() == audit['mesa_ci']['commit']
        assert len(z.namelist()) == len(set(z.namelist())) == 37
        for name in ['viogpu_gl_loader_x86.dll', 'viogpu_gl_vk_x86.dll', 'freedreno_icd.json']:
            data = z.read(name); assert sha(data) == HASHES[name]
            payloads.append({'member': name, 'bytes': len(data), 'sha256': sha(data),
                **(pe(data) if name.endswith('.dll') else {'json': json.loads(data)})})
        loader_source = z.read('loader-source-commit.txt').decode().strip(); assert len(loader_source) == 40
    protocol = sha(git(args.source, 'show', CORE+':src/umd/mesa_wddm_runtime.h'))
    assert protocol == audit['matched_protocol_sha256']
    user = load(args.user_identity)
    assert user['passed'] and user['source_commit'] == CORE and user['ci_run'] == RUN
    token = user['process_token']; assert token['session_id'] == 1 and not token['elevated'] and token['elevation_type'] == 3 and token['integrity_rid'] == 8192
    assert token['sid'] == 'S-1-5-21-362894365-441372107-2852668596-1000'
    archive = args.output/'native-system-d3d8-device-x86-source-01.tar.gz'
    with archive.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w') as out:
            for name,data in sorted(blobs.items()):
                info = tarfile.TarInfo(name);info.size=len(data);info.mode=0o644;info.mtime=0
                out.addfile(info,io.BytesIO(data))
    manifest = {'schema':'native-system-d3d8-device-x86-v1','source_commit':commit,'core_reference_commit':CORE,
        'target_arch':'x86','target_execution':'deferred','archive_sha256':sha(archive.read_bytes()),'archive_bytes':archive.stat().st_size,
        'inputs':sorted(sources,key=lambda r:r['path']),'harness_git_inputs':len(PRIMARY),'uncompiled_core_git_inputs':len(REFERENCES),
        'licensed_legacy_inputs':3,'expected_original_coffs':5,'expected_original_pes':4,
        'build_helper_sha256':sha(blobs['scripts/build-native-d3d8-runtime-device.ps1']),
        'collector_helper_sha256':sha(blobs['scripts/collect-native-d3d8-runtime-device.ps1']),
        'raw_process_helper_sha256':sha(blobs['scripts/owned-raw-process-f4bf37f-02.cs']),
        'groups':[{'name':'front','kind':'dll','units':['tests/umd-d3d8-runtime-front.cpp'],'def':'tests/umd-d3d8-runtime-front.def','output':'viogpu-d3d8-runtime-front.dll'},
            {'name':'probe','kind':'exe','units':['tests/umd-d3d8-runtime-probe.cpp','tests/umd-d3d8-runtime-guard.cpp'],'output':'d3d8-runtime-probe.exe'},
            {'name':'policy','kind':'exe','units':['tests/umd-d3d8-runtime-policy.cpp'],'output':'d3d8-runtime-policy.exe'},
            {'name':'callbacks','kind':'exe','units':['tests/umd-d3d8-runtime-callbacks.cpp'],'output':'d3d8-runtime-callbacks.exe'}],
        'native_policy_checks':args.policy_checks,'native_callback_checks':'derive actual stdout; mandatory forwarded=11','malformed_cli_guards':30,
        'scope':'strict native CPU policies, callback ownership and malformed/null/non-system guards only; no valid API/selector/device/render/Present execution',
        'core_required_for_cpu_build':False,'core_binary_built':False,'source_installation':False,'gpu_runs':0,'ci_dispatches':0}
    (args.output/'native-system-d3d8-device-x86-source-01.json').write_text(json.dumps(manifest,indent=2)+'\n')
    requirements = {'schema':'genuine-system-d3d8-device-preparation-v1','prepared':True,'hardware_accepted':False,'target_execution':'pending',
        'harness_commit':commit,'core_source':CORE,'core_ci_run':RUN,'core_original':core_identity,
        'mesa_original_zip':row(mesa_zip),'mesa_artifact_id':11408567654,'mesa_source':audit['mesa_ci']['commit'],
        'mesa_ci_run':37453381660,'khronos_loader_source':loader_source,'original_payloads':payloads,'private_protocol_sha256':protocol,
        'accepted_original_CI_proof':row(accepted_path),'reused_original_Mesa_audit':row(args.mesa_audit),
        'current_native_USER_receipt':row(args.user_identity),'actual_native_luid':user['adapter_luid'],'actual_native_source':user['kmt_names']['Source'],
        'actual_I386_WoW_KMT_name':None,'native_new_build_result':None,'genuine_D3D8_HAL_caps_result':None,'device_render_readback_present_result':None,
        'before_any_hardware':['native strict CPU packet passes and original PE/COFF/SDK/toolchain receipts independently joined',
            'fresh original Limited USER/session1 identity and signed SYS/service/registration/desktop before-after',
            'genuine I386 --kmt-names exact legacy filename recorded without selector or device factory',
            'owned exact copies of current CI I386 core + original loader/ICD/json; no relabeling/signature transformation',
            'fresh process under unchanged raw d8cf runner with retained handle/30s/exited/drained/raw logs',
            'front-enumerate actual HAL CAPS12 admission before separately permitted hardware invocation'],
        'candidate_path':r'C:\Users\Public\DxvkD3D8Candidate-d7e5c7d-37648387721\viogpudxvk.dll','production_OpenAdapter_export':False,
        'read_only_caps_modified':False,'WARP_allowed':False,'app_local_D3D8_allowed':False,'registry_writes':False,'payload_staged':False}
    (args.output/'matched-x86-system-device-requirements-01.json').write_text(json.dumps(requirements,indent=2)+'\n')
    for name in ['build-native-d3d8-runtime-device.ps1','collect-native-d3d8-runtime-device.ps1','parse-native-d3d8-runtime-device.ps1','owned-raw-process-f4bf37f-02.cs']:
        (args.output/name).write_bytes(blobs['scripts/'+name])
    print(json.dumps({'commit':commit,'inputs':len(sources),'archive':row(archive),'hardware_accepted':False},indent=2))

if __name__ == '__main__': main()
