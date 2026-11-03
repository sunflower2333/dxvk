#!/usr/bin/env python3
"""Compile complete production CS bodies; inject loss and partial execution."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--out',type=Path,required=True)
p.add_argument('--case',action='append')
a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=False)
inputs={}
for ext in ['h','cpp']:
    path=a.source/f'src/dxvk/dxvk_cs.{ext}'
    raw=path.read_bytes();inputs[str(path)]=hashlib.sha256(raw).hexdigest()
    (a.out/f'dxvk_cs_under_test.{ext}').write_text(re.sub(r'^#include[^\n]*\n','',raw.decode(),flags=re.M))
path=a.source/'src/d3d11/d3d11_context.h'
raw=path.read_bytes();inputs[str(path)]=hashlib.sha256(raw).hexdigest()
source=raw.decode();methods=[]
for signature in ['template<bool AllowFlush = true, typename Cmd>','template<typename M, bool AllowFlush = true, typename Cmd>','void FlushCsChunk()']:
    start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    methods.append(source[start:end])
(a.out/'frontend_under_test.h').write_text('\n\n'.join(methods)+'\n')
path=a.source/'src/d3d9/d3d9_device.h'
raw=path.read_bytes();inputs[str(path)]=hashlib.sha256(raw).hexdigest()
source=raw.decode();methods=[]
for signature in ['template<bool AllowFlush = true, typename Cmd>','template<typename M, bool AllowFlush = true, typename Cmd>','void FlushCsChunk()']:
    start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    methods.append(source[start:end])
(a.out/'frontend9_under_test.h').write_text('\n\n'.join(methods)+'\n')
for api in [9,11]:
    path=a.source/f'src/d3d{api}/d3d{api}_initializer.cpp'
    raw=path.read_bytes();inputs[str(path)]=hashlib.sha256(raw).hexdigest()
    source=raw.decode();start=source.index(f'void D3D{api}Initializer::FlushCsChunkLocked()');brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    (a.out/f'initializer{api}_under_test.h').write_text(source[start:end]+'\n')
path=a.source/'src/dxvk/dxvk_context.cpp'
raw=path.read_bytes();inputs[str(path)]=hashlib.sha256(raw).hexdigest()
source=raw.decode();methods=[]
for signature in ['void DxvkContext::beginRecording(', 'Rc<DxvkCommandList> DxvkContext::endRecording(', 'void DxvkContext::flushCommandList(']:
    start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    methods.append(source[start:end])
(a.out/'context_under_test.h').write_text('namespace dxvk {\n'+'\n\n'.join(methods)+'\n}\n')
fixture=Path(__file__).with_suffix('.cpp')
for path in [fixture,Path(__file__),a.source/'src/util/rc/util_rc_ptr.h',a.source/'src/util/util_flags.h',a.source/'src/util/util_bit.h',a.source/'src/util/util_likely.h']:
    inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
cmd=['clang++','-std=c++20','-O1','-g','-pthread','-I',str(a.out),str(fixture),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-o',str(a.out/'test')]
with (a.out/'build.log').open('w') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
cases=[]
if r.returncode==0:
    for name in a.case or ['healthy','removed','throwing','ownership','frontend','frontend9','initializer','context-healthy','context-lost']:
        with (a.out/(name+'.log')).open('w') as log:
            try:
                result=subprocess.run([str(a.out/'test'),name],stdout=log,stderr=subprocess.STDOUT,timeout=10)
                cases.append({'case':name,'exit':result.returncode})
            except subprocess.TimeoutExpired:cases.append({'case':name,'timeout':True})
receipt={'inputs':inputs,'command':cmd,'build_exit':r.returncode,'cases':cases,'boundary':'Actual complete CS header/implementation, frontend recording and context begin/end/flush bodies with production Rc/flags; controlled command lists, submission, device status and Vulkan peers, no hardware evidence.'}
receipt['passed']=r.returncode==0 and bool(cases) and all(x.get('exit')==0 for x in cases)
(a.out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({k:v for k,v in receipt.items() if k not in ['inputs','command']}))
raise SystemExit(0 if receipt['passed'] else 1)
