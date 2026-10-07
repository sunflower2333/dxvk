from pathlib import Path
import concurrent.futures, hashlib, json, subprocess, time

root = Path(__file__).resolve().parents[2]
base = Path('/home/sunf/droidvm-repos')
original = base / 'artifacts/dxvk-native-dx10-dx11-20261007/guest-adapter-guard-c407197-02/local-strict-coff-review/umd_adapter-x86_64.receipt.json'
flags = json.loads(original.read_text())['argv'][:-4]
out = base / 'artifacts/dxvk-so-volume-integration-20261008/local-strict-coff-01'
out.mkdir(parents=True, exist_ok=False)
sources = ['src/umd/umd_ddi.cpp', 'src/umd/umd_shader11.cpp', 'tests/umd-stream-output.cpp', 'tests/umd-d3d11-device.cpp']
inputs = sources + ['src/umd/umd_d3d11_shader.inl','src/umd/umd_shader11.h','src/umd/umd_stream_output.h','src/umd/umd_d3d11_ddi.inl','src/umd/umd_output_merger.h','src/umd/umd_output_policy.h','src/umd/umd_view.h','src/umd/umd_texture3d.h','src/umd/umd_volume_policy.h','tests/umd-texture3d.cpp','tests/umd-volume-policy.cpp']

def pin(path):
    data = path.read_bytes()
    return {'path': str(path), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}

before = [pin(root / name) for name in inputs]
(out / 'source-before.json').write_text(json.dumps(before, indent=2) + '\n')

def run(case):
    arch, source = case
    argv = flags.copy()
    argv[argv.index('--target=x86_64-pc-windows-msvc')] = '--target=' + arch + '-pc-windows-msvc'
    label = Path(source).stem + '-' + arch
    obj = out / (label + '.obj')
    argv += ['-I' + str(base / 'dxvk-umd-ci/subprojects/dxbc-spirv')]
    if source == 'tests/umd-stream-output.cpp':
        argv += ['-Wno-unused-parameter', '-DVIOGPU_STREAM_OUTPUT_WARP']
    argv += [str(root / source), '-O1', '-MMD', '-MF', str(out / (label + '.d')), '-o', str(obj)]
    started = time.monotonic()
    process = subprocess.Popen(argv, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    timeout = False
    try:
        stdout, stderr = process.communicate(timeout=120)
    except subprocess.TimeoutExpired:
        timeout = True
        process.kill()
        stdout, stderr = process.communicate(timeout=10)
    for suffix, data in [('stdout.raw', stdout), ('stderr.raw', stderr)]:
        (out / (label + '.' + suffix)).write_bytes(data)
    row = {'label': label, 'argv': argv, 'pid': process.pid, 'actual_exit': process.returncode,
           'timed_out': timeout, 'seconds': time.monotonic() - started,
           'stdout': pin(out / (label + '.stdout.raw')), 'stderr': pin(out / (label + '.stderr.raw'))}
    if obj.exists():
        data = obj.read_bytes()
        offset = 6 if data[:4] == b'\0\0\xff\xff' else 0
        machine = int.from_bytes(data[offset:offset + 2], 'little')
        row.update(output=pin(obj), machine=machine, correct_machine=machine == {'x86_64': 0x8664, 'i686': 0x14c}[arch])
    (out / (label + '.process.json')).write_text(json.dumps(row, indent=2) + '\n')
    return row

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    rows = list(pool.map(run, [(arch, source) for arch in ['x86_64', 'i686'] for source in ['src/umd/umd_ddi.cpp']]))
after = [pin(root / name) for name in inputs]
(out / 'source-after.json').write_text(json.dumps(after, indent=2) + '\n')
passed = before == after and all(row['actual_exit'] == 0 and not row['timed_out']
    and row['stderr']['bytes'] == 0 and row.get('correct_machine') for row in rows)
(out / 'verified.json').write_text(json.dumps({'passed': passed, 'source_unchanged': before == after,
    'actual_processes': rows, 'native_execution': False, 'hardware_acceptance': False}, indent=2) + '\n')
print(json.dumps({'passed': passed, 'processes': [{'label': row['label'], 'exit': row['actual_exit'],
    'stderr_bytes': row['stderr']['bytes']} for row in rows]}, indent=2))
raise SystemExit(0 if passed else 1)
