#!/usr/bin/env python3
"""Read the frozen local originals; never execute a target operation."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path('/home/sunf/droidvm-repos/reference/codes/dxvk-umd-cube-mip-generation-20261008')
COMMIT = '6b1524780b8ca17b7d8b40f06fcdb569eb2851df'
BASE = '8528d91357255fe8f31138d5438e7313e2367fec'
LOCAL = ROOT / 'artifacts/local-cube-mip-01'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def pin(row):
    data = Path(row['path']).read_bytes()
    assert len(data) == row['bytes'] and digest(data) == row['sha256'], row['path']
    return data


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def git(commit, path):
    return subprocess.check_output(['git', '-C', str(ROOT), 'show', commit + ':' + path])


changed = subprocess.check_output([
    'git', '-C', str(ROOT), 'diff', '--name-only', BASE, COMMIT
]).decode().splitlines()
assert len(changed) == 4
for path in changed:
    assert (ROOT / path).read_bytes() == git(COMMIT, path), path

header = git(COMMIT, 'src/umd/umd_generate_mips.h')
start = header.index(b'// Normalize each selected 2D slice')
end = header.index(b'// Generate exactly the validated SRV', start)
reverse = header[:start] + header[end:]
reverse = reverse.replace(
    b'  if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)\n'
    b'    return generateTexture2DViewMips(device, context, resource.Get(), view);\n', b'')
assert reverse == git(BASE, 'src/umd/umd_generate_mips.h')
fixture = git(COMMIT, 'tests/umd-texturecube.cpp')
start = fixture.index(b'// Capture direct public-backend behavior')
end = fixture.index(b'static void testMips(Fixture& f)', start)
reverse = fixture[:start] + fixture[end:]
reverse = reverse.replace(
    b'  UINT publicCase = 0;\n'
    b'  for (UINT selected : {UINT32_MAX, 2u, 1u})\n'
    b'    observePublicMips(f, publicCase++, selected);\n', b'')
assert reverse == git(BASE, 'tests/umd-texturecube.cpp')

before = load(LOCAL / 'source-before-original.json')
assert before == load(LOCAL / 'source-after-original.json') and len(before) == 160
for row in before:
    pin(row)
compiler = load(LOCAL / 'compiler-input-original-copies.json')
assert len(compiler) == 341
for row in compiler:
    assert pin(row['original']) == pin(row['copy'])

compiles = load(LOCAL / 'strict-coff-compile-verified-01.json')
assert compiles['verified'] and compiles['diagnostics'] == 0
assert len(compiles['processes']) == 6
for row in compiles['processes']:
    assert row['exit_code'] == 0 and row['exited'] and row['pipes_drained']
    assert not row['timed_out'] and not pin(row['stdout']) and not pin(row['stderr'])
    assert '-Werror' in row['argv'] and '-O2' in row['argv']

controls = load(ROOT / 'artifacts/local-cube-public-reader-controls-01/'
                'local-coff-and-public-reader-controls-verified-01.json')
assert controls['synthetic_positive_controls'] == 2
assert controls['synthetic_negative_controls'] == 4
assert len(controls['objects']) == 6 and len(controls['processes']) == 12
assert pin(controls['reader']) == git(COMMIT, 'scripts/verify-native-cube-public-mips-originals.py')
for row in controls['objects']:
    data = pin(row)
    offset = 6 if data[:4] == b'\0\0\xff\xff' else 0
    assert struct.unpack_from('<H', data, offset)[0] == int(row['machine'], 16)
for index, row in enumerate(controls['processes']):
    expected = 0 if index < 8 else 1
    assert row['exit_code'] == expected and row['exited'] and row['pipes_drained']
    assert row['streams_closed'] and not row['timed_out']
    stdout, stderr = pin(row['stdout']), pin(row['stderr'])
    if expected == 0:
        assert not stderr
    else:
        assert stderr and not stdout

result = {
    'verified': True, 'source_commit': COMMIT,
    'changed_git_files': len(changed), 'exact_header_reverse_match': True,
    'exact_fixture_reverse_match': True, 'frozen_source_inputs': len(before),
    'original_compiler_inputs': len(compiler), 'strict_local_COFF_objects': 6,
    'strict_compile_processes': 6, 'LLVM_inspection_processes': 6,
    'actual_positive_reader_processes': 2, 'actual_negative_reader_processes': 4,
    'original_process_streams_rehashed': 36,
    'production_oracle_unchanged': True, 'canonical_raw_files': 336,
    'canonical_readbacks': 7, 'canonical_words': 10380,
    'native_reference_pending': True, 'target_calls': False,
    'hardware_acceptance': False, 'ordinary_runtime_admission': False,
}
output = LOCAL / 'root-source-local-originals-review-01.json'
assert not output.exists()
output.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({'output': str(output), 'bytes': output.stat().st_size,
                  'sha256': digest(output.read_bytes()), 'verified': True}))
