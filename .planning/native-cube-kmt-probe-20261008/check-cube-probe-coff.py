#!/usr/bin/env python3
"""Compile local cube probe against unchanged official SDK/WDK and test helpers."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import time

ROOT = Path('/home/sunf/droidvm-repos')
REPO = ROOT / 'reference/codes/dxvk-umd-cube-kmt-probe-20261008'
SHARED = ROOT / 'reference/codes/dxvk-umd-dx11-so-volume-probe-20261008'
PRIOR = SHARED / 'artifacts/dx11-so-volume-probes-01/local-strict-06'
DEST = ROOT / 'artifacts/native-cube-kmt-probe-20261008' / sys.argv[1]
DEST.mkdir(parents=True, exist_ok=False)


def evidence(path):
    raw = path.read_bytes()
    return dict(path=str(path), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


inputs = [REPO / 'tests/umd-d3d11-cube-probe.cpp', REPO / 'tests/umd-cube-probe-oracle.h',
          SHARED / 'tests/umd-kmt-graphics-probe.h', SHARED / 'tests/umd-kmt-compute-transport.h',
          SHARED / 'tests/umd-kmt-callback-policy.h']
before = [evidence(path) for path in inputs]


def compile_one(arch):
    old = json.loads((PRIOR / ('volume-coff-' + arch + '.actual-process.json')).read_text())
    argv = old['argv'][:old['argv'].index('tests/umd-d3d11-volume-probe.cpp')]
    argv += ['-I', str(SHARED / 'tests'), str(REPO / 'tests/umd-d3d11-cube-probe.cpp'),
             '-O1', '-MD', '-MF', str(DEST / (arch + '.d')), '-o', str(DEST / (arch + '.obj'))]
    start = time.monotonic()
    stdout, stderr = DEST / (arch + '.stdout.raw'), DEST / (arch + '.stderr.raw')
    with stdout.open('xb') as out, stderr.open('xb') as err:
        child = subprocess.Popen(argv, cwd=REPO, stdin=subprocess.DEVNULL, stdout=out, stderr=err)
        try:
            code = child.wait(timeout=60)
            timed_out = False
        except subprocess.TimeoutExpired:
            child.kill()
            code = child.wait(timeout=5)
            timed_out = True
    result = dict(architecture=arch, argv=argv, pid=child.pid, exit=code, timeout=timed_out,
                  seconds=time.monotonic() - start, stdout=evidence(stdout), stderr=evidence(stderr),
                  native_execution=False, hardware_acceptance=False)
    if code == 0:
        obj = DEST / (arch + '.obj')
        raw = obj.read_bytes()
        machine = struct.unpack_from('<H', raw, 6 if raw[:4] == b'\0\0\xff\xff' else 0)[0]
        assert machine == (0x8664 if arch == 'x86_64' else 0x14c)
        result['COFF'] = dict(**evidence(obj), machine=machine)
    (DEST / (arch + '.process.json')).write_text(json.dumps(result, indent=2) + '\n')
    return result


with ThreadPoolExecutor(max_workers=2) as pool:
    results = list(pool.map(compile_one, ['x86_64', 'i686']))
after = [evidence(path) for path in inputs]
assert before == after
summary = dict(passed=all(r['exit'] == 0 and not r['timeout'] and r['stdout']['bytes'] == r['stderr']['bytes'] == 0
                         for r in results), inputs_before=before, inputs_after=after, commands=results,
               shared_helper_Git_freeze_pending=True, native_execution=False, hardware_acceptance=False)
(DEST / 'verified.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(dict(passed=summary['passed'], processes=[dict(architecture=r['architecture'], pid=r['pid'],
      exit=r['exit'], stderr_bytes=r['stderr']['bytes']) for r in results], output=str(DEST))))
if not summary['passed']:
    raise SystemExit(1)
