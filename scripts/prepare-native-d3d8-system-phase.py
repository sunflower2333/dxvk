#!/usr/bin/env python3
"""Copy original I386 payloads and prepare one manifest, with native pins pending."""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import tarfile
import zipfile
from pathlib import Path

SOURCE = '5c420e4daddc39effb2c8e8a28bd07ec7407c402'
CORE = 'd7e5c7d46b8ce889e993bfab66a3b78b076c49d1'
RUN = 37648387721
FOLDER = r'C:\Users\Public\DxvkD3D8Candidate-d7e5c7d-37648387721'
NATIVE = r'C:\Users\Public\DxvkD3D8Runtime-5c420e4-05'
ZIP_PINS = {
    'core': ('artifacts/dxvk-native-dx10-dx11-20261007/root-consolidated-arm-canonical-01/successful-original-ci-01/_archives/dxvk-umd-backend-x86-' + CORE + '.zip', 27246294, '2ebb7966a16417465b838b24b346f6cd4ee068e279b5e0c7f664abf28ff22b2d'),
    'mesa': ('artifacts/dxvk-native-d3d8-port-20261007/x86-runtime-artifact-audit-01/mesa-8443c71-x86-11408567654.zip', 29588250, 'a80bb994b1c7855f21b76f87e72a86add435e19719db41bd66fc55bba55a43dc')}
PAYLOADS = (
    ('core', 'core', 'viogpudxvk.dll', 5488640, '7be8cbb9850407ccc304528911a6fbd71b01971fbeb4a86550cc8dcc2f346a3f'),
    ('loader', 'mesa', 'viogpu_gl_loader_x86.dll', 677888, 'd459f2d09080865cc3d591b498c02d38305a26963b401152f8230dc60c5ad7e7'),
    ('icd', 'mesa', 'viogpu_gl_vk_x86.dll', 14300672, '2b549889816163433faabe6f2c1d2a61d6c106078d08e30031b74c0a66cd7f5c'),
    ('icd-json', 'mesa', 'freedreno_icd.json', 145, '74d7d5d6ae9432cde2d802507ed59bbe4c2f2b95d01e7ac3c2b56e9691932c80'))


def require(value, message):
    if not value:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def record(path):
    data = path.read_bytes()
    return {'path': str(path), 'bytes': len(data), 'sha256': sha(data)}


def json_read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pe(data):
    require(data[:2] == b'MZ', 'original DOS header required')
    offset = struct.unpack_from('<I', data, 0x3c)[0]
    require(data[offset:offset+4] == b'PE\0\0' and struct.unpack_from('<H', data, offset+4)[0] == 0x14c, 'matching original I386 PE required')
    require(struct.unpack_from('<H', data, offset+24)[0] == 0x10b, 'original PE32 required')
    security = struct.unpack_from('<II', data, offset+24+96+4*8)
    require(security == (0, 0), 'retain genuinely unsigned payload identity')
    return {'machine': '014c', 'optional_header': 'PE32', 'security_directory': list(security), 'authenticode_trust_verified': False}


def prepare(workspace, output, native_originals=None, native_proof=None, native_archive=None):
    require(not output.exists(), 'preserve previous preparation; use fresh output')
    output.mkdir(parents=True)
    scripts = Path(__file__).resolve().parent
    repo = scripts.parent
    runner_source = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    bundle = output / 'helpers'; bundle.mkdir()
    rows = []
    for name in ('run-native-d3d8-system-phase.ps1', 'invoke-native-d3d8-system-phase.ps1', 'collect-native-d3d8-system-phase.ps1', 'owned-raw-process-f4bf37f-02.cs'):
        data = (scripts / name).read_bytes()
        original = subprocess.check_output(['git', '-C', str(repo), 'show', runner_source + ':scripts/' + name])
        require(data == original, 'runner helper must be frozen exact Git: ' + name)
        (bundle / name).write_bytes(data)
        rows.append({'name': name, 'bytes': len(data), 'sha256': sha(data), 'source_commit': runner_source, 'source_path': 'scripts/' + name})
    reused = workspace / 'artifacts/dxvk-native-dx10-dx11-20261007/compute-user-runner-scripts-03'
    for name in ('inspect-process-token.ps1', 'inspect-viogpu-readiness-fast-02.ps1'):
        data = (reused / name).read_bytes(); (bundle / name).write_bytes(data)
        rows.append({'name': name, 'bytes': len(data), 'sha256': sha(data), 'source_path': str(reused / name), 'scope': 'unchanged retained original helper03'})
    policy = workspace / 'artifacts/dxvk-native-dx10-dx11-20261007/compute-gpu-registration-follow-on-09/verify-registration-originals-09.py'
    data = policy.read_bytes()
    require(sha(data) == '28f29b0e69e06adb78fc2d6da10d3cd3d1709cfbb7da4619c47b0f90cbbbe051', 'reviewed explicit static34 policy changed')
    (bundle / policy.name).write_bytes(data)
    rows.append({'name': policy.name, 'bytes': len(data), 'sha256': sha(data), 'source_path': str(policy), 'scope': 'unchanged reviewed static34 policy; Python CLI is not invoked on target'})
    for name in ('parse-native-d3d8-system-phase.ps1', 'verify-native-d3d8-system-phase.py', 'verify-native-d3d8-system-device.py'):
        data = subprocess.check_output(['git', '-C', str(repo), 'show', runner_source + ':scripts/' + name])
        require(data == (scripts / name).read_bytes(), 'supplemental helper differs from frozen Git')
        (bundle / name).write_bytes(data)
    payload = output / 'candidate-originals'; payload.mkdir()
    original_zips = {}
    for role, (path, size, digest) in ZIP_PINS.items():
        original = workspace / path
        row = record(original)
        require(row['bytes'] == size and row['sha256'] == digest, 'original API ZIP pin changed: ' + role)
        original_zips[role] = row
    files = []
    provenance = []
    for role, archive_role, name, size, digest in PAYLOADS:
        with zipfile.ZipFile(original_zips[archive_role]['path']) as archive:
            matches = [info for info in archive.infolist() if info.filename.rsplit('/', 1)[-1] == name]
            require(len(matches) == 1, 'one original ZIP payload member required')
            info = matches[0]; data = archive.read(info)  # Original ZIP CRC checked by ZipFile.
        require(len(data) == size and sha(data) == digest, 'original ZIP member changed: ' + name)
        identity = {'json': json.loads(data)} if role == 'icd-json' else pe(data)
        (payload / name).write_bytes(data)
        files.append({'role': role, 'path': FOLDER + '\\' + name, 'bytes': size, 'sha256': digest, 'phases': ['enumerate', 'offscreen', 'present']})
        provenance.append({'role': role, 'archive': original_zips[archive_role], 'member': info.filename, 'crc32': info.CRC, 'bytes': size, 'sha256': digest, **identity})
    with tarfile.open(output / 'candidate-original-payloads.tar.gz', 'w:gz') as archive:
        for path in sorted(payload.iterdir()):
            archive.add(path, arcname=path.name, recursive=False)
    native = {'accepted': False, 'source': SOURCE, 'original_archive_sha256': None, 'original_proof_sha256': None}
    pins = {
        'probe': {'path': NATIVE + r'\probe\d3d8-runtime-probe.exe', 'bytes': None, 'sha256': None, 'phases': ['names', 'enumerate', 'offscreen', 'present']},
        'frontend': {'path': NATIVE + r'\front\viogpu-d3d8-runtime-front.dll', 'bytes': None, 'sha256': None, 'phases': ['enumerate', 'offscreen', 'present']}}
    supplied = (native_originals, native_proof, native_archive)
    require(all(supplied) or not any(supplied), 'supply all three actual native original inputs or leave all pending')
    if all(supplied):
        result = json_read(native_originals / 'result.json'); proof = json_read(native_proof)
        archive_pin = record(native_archive)
        require(proof['verified'] and proof['source_commit'] == SOURCE and proof['archive_sha256'] == archive_pin['sha256'], 'independently accepted native original proof required')
        require(result['source_commit'] == SOURCE and result['status'] == 'PASS' and result['object_count'] == 5 and result['pe_count'] == 4 and result['malformed_cli_guards'] == 30 and result['policy_checks'] == 329 and result['callback_checks'] > 0, 'actual strict native CPU suite incomplete')
        require(result['source_before'] == result['source_after'] and result['before'] == result['after'] and result['gpu_runs'] == result['system_runtime_calls'] == result['selector_calls'] == 0, 'native original source/state/scope mismatch')
        with tarfile.open(native_archive, 'r:gz') as archive:
            native_members = {member.name.removeprefix('./'): archive.extractfile(member).read() for member in archive if member.isfile()}
        require(native_members['result.json'] == (native_originals / 'result.json').read_bytes(), 'accepted native receipt not in original archive')
        for role, pin in pins.items():
            path = ('probe/d3d8-runtime-probe.exe' if role == 'probe' else 'front/viogpu-d3d8-runtime-front.dll')
            data = (native_originals / path).read_bytes()
            require(native_members[path] == data, 'actual native output differs from original archive')
            row = [item for item in result['outputs'] if item['path'] == pin['path']]
            require(len(row) == 1 and row[0]['bytes'] == len(data) and row[0]['sha256'] == sha(data), 'actual native output pin mismatch')
            pe(data)
            pin.update(bytes=len(data), sha256=sha(data))
        native.update(accepted=True, original_archive_sha256=archive_pin['sha256'], original_proof_sha256=sha(native_proof.read_bytes()), original_proof_path=str(native_proof), original_archive_path=str(native_archive))
    files = [{'role': role, **pin} for role, pin in pins.items()] + files
    manifest = {'schema': 'system-d3d8-phase-inputs-v1', 'ready': False, 'probe_source': SOURCE, 'runner_source': runner_source,
                'core_source': CORE, 'core_ci_run': RUN, 'loader_source': '6a6878c614c8c6dbe81ee7a9f1176bdb52dc7dd7', 'icd_source': '8443c71a5ab32b9d58b904fa51f4bf2f9089db8d',
                'adapter_luid': 'ec6b000000000000', 'source_id': 0, 'user': {'account': r'DROIDVM\USER', 'sid': 'S-1-5-21-362894365-441372107-2852668596-1000'},
                'native_cpu': native, 'files': files, 'helpers': rows, 'payload_originals': provenance,
                'pending': ['native phase parser and independent root review before a separately authorized phase'] + ([] if native['accepted'] else ['native5c420e4 strict CPU success and independent original proof', 'actual native I386 probe/frontend hashes']),
                'native_phase_parse': 'pending', 'I386_KMT_names': 'pending', 'system_HAL_enumeration': 'pending', 'hardware_admission': False,
                'registry_driver_writes': False, 'installation': False, 'payload_staged': False}
    (bundle / 'phase-inputs-original.json').write_text(json.dumps(manifest, indent=2) + '\n')
    proof = {'prepared': True, 'target_actions': 0, 'core_module_loads': 0, 'GPU_runs': 0, 'manifest': record(bundle / 'phase-inputs-original.json'),
             'candidate_archive': record(output / 'candidate-original-payloads.tar.gz'), 'native_inputs_pending': not native['accepted'],
             'helpers': [record(path) for path in sorted(bundle.iterdir())], 'original_payloads': provenance}
    (output / 'prepared-phase-originals-01.json').write_text(json.dumps(proof, indent=2) + '\n')
    return proof


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, default=Path('/home/sunf/droidvm-repos'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--native-originals', type=Path); parser.add_argument('--native-proof', type=Path); parser.add_argument('--native-archive', type=Path)
    args = parser.parse_args()
    proof = prepare(args.workspace, args.output, args.native_originals, args.native_proof, args.native_archive)
    print(json.dumps({'prepared': proof['prepared'], 'native_inputs_pending': proof['native_inputs_pending'], 'manifest': proof['manifest'], 'target_actions': 0}, indent=2))
