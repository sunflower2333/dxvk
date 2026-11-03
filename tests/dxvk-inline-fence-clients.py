#!/usr/bin/env python3
"""Exercise actual CPU-fence clients without a second DDI call to retire work."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--out',type=Path,required=True)
p.add_argument('--bits',type=int,choices=[32,64],default=64)
p.add_argument('--case',action='append')
p.add_argument('--expect-stall',action='store_true')
p.add_argument('--sanitize',action='store_true')
a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=False)
inputs={}
def read(rel):
    path=a.source/rel
    raw=path.read_bytes();inputs[rel]=hashlib.sha256(raw).hexdigest()
    return raw.decode()
def function(rel,signature):
    source=read(rel);start=source.index(signature);brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
for rel,target in [('src/dxvk/dxvk_queue.h','dxvk_queue_under_test.h'),
                   ('src/dxvk/dxvk_queue.cpp','dxvk_queue_under_test.cpp'),
                   ('src/util/sync/sync_signal.h','sync_signal_under_test.h')]:
    (a.out/target).write_text(re.sub(r'^#include[^\n]*\n','',read(rel),flags=re.M))
limits=read('src/d3d11/d3d11_initializer.h')
lines=[]
for name in ['MaxCommandsPerSubmission','MaxMemoryPerSubmission','MaxMemoryInFlight']:
    lines.append(re.search(r'^.*constexpr static size_t '+name+r' = .*;$',limits,re.M).group())
(a.out/'initializer_limits_under_test.h').write_text('\n'.join(lines)+'\n')
methods=[function(rel,sig) for rel,sig in [
    ('src/dxvk/dxvk_device.cpp','void DxvkDevice::waitForFence('),
    ('src/d3d11/d3d11_initializer.cpp','void D3D11Initializer::ThrottleAllocationLocked()'),
    ('src/d3d9/d3d9_device.cpp','void D3D9DeviceEx::EnsureSamplerLimit()')]]
(a.out/'fence_client_methods_under_test.h').write_text('namespace dxvk {\n'+'\n\n'.join(methods)+'\n}\n')
fixture=Path(__file__).with_suffix('.cpp')
for path in [fixture,fixture.with_name('dxvk-inline-queue.cpp'),Path(__file__)]:
    inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
cmd=['clang++','-std=c++17','-O1','-g','-pthread','-include','utility','-DCLIENT_BITS='+str(a.bits),'-I',str(a.out),str(fixture),'-o',str(a.out/'test')]
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
with (a.out/'build.log').open('w') as log:build=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
results=[]
if build.returncode==0:
    for case in a.case or ['initializer','sampler','initializer-async','sampler-async','initializer-limit']:
        with (a.out/(case+'.log')).open('w') as log:
            try:
                run=subprocess.run([str(a.out/'test'),case],stdout=log,stderr=subprocess.STDOUT,timeout=3)
                result={'case':case,'exit':run.returncode,'timeout':False}
            except subprocess.TimeoutExpired:
                result={'case':case,'exit':None,'timeout':True}
        results.append(result)
passed=build.returncode==0 and bool(results) and all(r['timeout'] if a.expect_stall else r['exit']==0 and not r['timeout'] for r in results)
r={'passed':passed,'expected_stall':a.expect_stall,'bits':a.bits,'inputs':inputs,'command':cmd,'build_exit':build.returncode,'cases':results,'boundary':'Actual queue, CPU fence, waitForFence, initializer throttle and sampler-limit bodies; controlled GPU completion and resource peers. No hardware proof.'}
(a.out/'result.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({k:v for k,v in r.items() if k not in ['inputs','command']}))
raise SystemExit(0 if passed else 1)
