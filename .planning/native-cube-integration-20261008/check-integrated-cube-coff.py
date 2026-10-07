#!/usr/bin/env python3
"""Compile the combined production DDI against unchanged official SDK/WDK."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import time

WORKSPACE = Path('/home/sunf/droidvm-repos')
ROOT = WORKSPACE / 'reference/codes/dxvk-umd-cube-integration-20261008'
OUT = WORKSPACE / 'artifacts/native-cube-integration-20261008/local-strict-coff-01'
OUT.mkdir(parents=True, exist_ok=False)
ORIGINAL = WORKSPACE / 'artifacts/dx11-cube-mips-20261008/local-strict-coff-02'


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def run(arch):
    original = ORIGINAL / ('umd_ddi-' + arch + '.process.json')
    old = json.loads(original.read_text())
    assert old['actual_exit'] == 0 and not old['timed_out'] and old['correct_machine']
    args = old['argv'].copy()
    source = ROOT / 'src/umd/umd_ddi.cpp'
    args[args.index(old['argv'][old['argv'].index('-O1') - 1])] = str(source)
    args[args.index('-MF') + 1] = str(OUT / (arch + '.d'))
    args[args.index('-o') + 1] = str(OUT / (arch + '.obj'))
    timed_out = False
    start = time.monotonic()
    with (OUT / (arch + '.stdout.raw')).open('xb') as stdout, (OUT / (arch + '.stderr.raw')).open('xb') as stderr:
        child = subprocess.Popen(args, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr)
        try:
            child.wait(timeout=90)
        except subprocess.TimeoutExpired:
            timed_out = True
            child.kill()
        code = child.wait()
    result = dict(argv=args, original_argv_receipt=pin(original), pid=child.pid,
                  exited=True, actual_exit=code, timed_out=timed_out,
                  raw_outputs_drained=True, seconds=time.monotonic() - start,
                  stdout=pin(OUT / (arch + '.stdout.raw')), stderr=pin(OUT / (arch + '.stderr.raw')))
    obj = OUT / (arch + '.obj')
    if obj.exists():
        result['output'] = pin(obj)
        result['machine'] = struct.unpack_from('<H', obj.read_bytes())[0]
        result['correct_machine'] = result['machine'] == {'x86_64': 0x8664, 'i686': 0x14c}[arch]
    (OUT / (arch + '.process-original.json')).write_text(json.dumps(result, indent=2) + '\n')
    return result


sources = sorted((ROOT / 'src/umd').glob('*.h')) + sorted((ROOT / 'src/umd').glob('*.inl')) + [ROOT / 'src/umd/umd_ddi.cpp']
argv = json.loads((ORIGINAL / 'umd_ddi-x86_64.process.json').read_text())['argv']
sources += [Path(shutil.which(argv[0])), Path(argv[argv.index('-include') + 1]), Path(argv[argv.index('-ivfsoverlay') + 1])]
before = [pin(path) for path in sources]
(OUT / 'inputs-before.json').write_text(json.dumps(before, indent=2) + '\n')
with ThreadPoolExecutor(max_workers=2) as pool:
    results = list(pool.map(run, ('x86_64', 'i686')))
after = [pin(path) for path in sources]
(OUT / 'inputs-after.json').write_text(json.dumps(after, indent=2) + '\n')
assert before == after
passed = all(row['actual_exit'] == 0 and not row['timed_out'] and row.get('correct_machine') and
             row['stdout']['bytes'] == row['stderr']['bytes'] == 0 for row in results)
proof = dict(schema='combined-cube-production-strict-COFF-v1', verified=passed,
             source_inputs_before_after_equal=True, sources=before, results=results,
             compile_only=True, native_execution=False, target_calls=False)
(OUT / 'verified-originals-01.json').write_text(json.dumps(proof, indent=2) + '\n')
print(json.dumps(dict(verified=passed, processes=[dict(pid=row['pid'], exit=row['actual_exit'],
                     machine=row.get('machine'), stderr_bytes=row['stderr']['bytes']) for row in results])))
raise SystemExit(0 if passed else 1)
