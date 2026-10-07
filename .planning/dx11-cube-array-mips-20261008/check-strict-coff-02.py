#!/usr/bin/env python3
"""Retain actual strict MSVC-target COFF builds of production and the native fixture."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import time

W = Path('/home/sunf/droidvm-repos')
R = W / 'reference/codes/dxvk-umd-dx11-cube-array-mips-20261008'
O = W / 'artifacts/dx11-cube-array-mips-20261008/local-strict-coff-02'
O.mkdir(parents=True, exist_ok=False)
base = json.loads((W / 'artifacts/dx11-cube-mips-20261008/local-strict-coff-02/umd_ddi-x86_64.process.json').read_text())['argv']

def pin(p):
    d=p.read_bytes();return dict(path=str(p),bytes=len(d),sha256=hashlib.sha256(d).hexdigest())

pins=[R/'src/umd/umd_ddi.cpp',R/'src/umd/umd_view.h',R/'tests/umd-cube-array-mips.cpp']
pins += [Path(base[base.index('-include')+1]), Path(base[base.index('-ivfsoverlay')+1]), Path(shutil.which(base[0]))]
pins += [Path('/home/sunf/CrWDDK/Build/serial-kit/microsoft.windows.wdk.arm64/c/Include/10.0.26100.0/um/d3d10umddi.h'),
         Path('/home/sunf/CrWDDK/Build/serial-kit/microsoft.windows.sdk.cpp/c/Include/10.0.26100.0/um/d3d11.h'),
         Path('/home/sunf/CrWDDK/Build/serial-kit/microsoft.windows.sdk.cpp/c/Include/10.0.26100.0/um/winnt.h'),
         Path('/home/sunf/CrWDDK/Build/serial-kit/microsoft.windows.sdk.cpp/c/Include/10.0.26100.0/shared/dxgitype.h')]
before=[pin(p) for p in pins]
(O/'inputs-before.json').write_text(json.dumps(before,indent=2)+'\n')

def build(arch,source):
    label=source.stem+'-'+arch
    args=base.copy()
    args[1]='--target='+{'x64':'x86_64-pc-windows-msvc','x86':'i686-pc-windows-msvc','arm64':'aarch64-pc-windows-msvc'}[arch]
    args[args.index('-MF')+1]=str(O/(label+'.d'))
    args[args.index('-o')+1]=str(O/(label+'.obj'))
    args[next(i for i,v in enumerate(args) if v.endswith('/src/umd/umd_ddi.cpp'))]=str(source)
    timeout=False;started=time.monotonic()
    with (O/(label+'.stdout.raw')).open('xb') as out,(O/(label+'.stderr.raw')).open('xb') as err:
        child=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=out,stderr=err)
        try:child.wait(timeout=90)
        except subprocess.TimeoutExpired:timeout=True;child.kill()
        code=child.wait()
    result=dict(argv=args,actual_owned_pid=child.pid,exit_code=code,timed_out=timeout,seconds=time.monotonic()-started,
                process_exited=True,raw_outputs_drained=True,source=pin(source),stdout=pin(O/(label+'.stdout.raw')),stderr=pin(O/(label+'.stderr.raw')))
    if (O/(label+'.obj')).exists():
        output=O/(label+'.obj');data=output.read_bytes()
        result.update(output=pin(output),machine=struct.unpack_from('<H',data)[0])
        result['correct_machine']=result['machine']=={'x64':0x8664,'x86':0x14c,'arm64':0xaa64}[arch]
    (O/(label+'.process-original.json')).write_text(json.dumps(result,indent=2)+'\n')
    return result

with ThreadPoolExecutor(max_workers=6) as pool:
    jobs=[pool.submit(build,a,s) for a in ('x64','x86','arm64') for s in (R/'src/umd/umd_ddi.cpp',R/'tests/umd-cube-array-mips.cpp')]
    results=[f.result() for f in jobs]
after=[pin(p) for p in pins]
(O/'inputs-after.json').write_text(json.dumps(after,indent=2)+'\n')
assert before==after
passed=all(r['exit_code']==0 and not r['timed_out'] and r.get('correct_machine') and r['stdout']['bytes']==r['stderr']['bytes']==0 for r in results)
proof=dict(verified=passed,compile_only=True,native_execution=False,reference_runtime_execution=False,target_calls=False,source_input_pins=before,results=results)
(O/'strict-coff-actual-originals-02.json').write_text(json.dumps(proof,indent=2)+'\n')
print(json.dumps(dict(verified=passed,results=[dict(pid=r['actual_owned_pid'],source=Path(r['source']['path']).name,exit=r['exit_code'],stderr_bytes=r['stderr']['bytes'],machine=r.get('machine')) for r in results])))
raise SystemExit(0 if passed else 1)
