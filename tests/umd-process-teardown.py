#!/usr/bin/env python3
"""Compile actual Device::close, child retire functions, global declarations and header."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import os
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',default='clang++');p.add_argument('--wine',type=Path);a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=False)
source=a.source/'src/umd/umd_ddi.cpp';raw=source.read_text()
def block(signature):
 start=raw.index(signature);end=raw.index('{',start)+1;depth=1
 while depth:depth+=(raw[end]=='{')-(raw[end]=='}');end+=1
 return raw[start:end]
(a.out/'device_close.inc').write_text(block('HRESULT close() noexcept')+'\n'+block('~Device()')+'\n')
(a.out/'child_types.inc').write_text(block('struct ComRetirement final')+';\n'+block('struct Child {')+';\n')
(a.out/'device_record.inc').write_text(re.search(r'enum class DevicePhase[^;]+;',raw).group(0)+'\n'+block('struct DeviceRecord {')+';\n')
found=[]
for name in ('deviceStorageMutex','deviceStorage','queryStorage','layoutStorage'):
 holder=re.search(r'^dxvk::umd::ProcessStorage<[^\n]+> '+name+r'Lifetime;\nauto& '+name+r' = '+name+r'Lifetime.get\(\);',raw,re.M)
 plain=re.search(r'^(?:std::|dxvk::umd::)[^\n]+ '+name+r';',raw,re.M)
 found.append(holder or plain)
assert all(found)
closing=re.search(r'bool deviceStorageClosing = false;',raw)
if closing:found.append(closing)
shutdown=re.search(r'struct DeviceStorageShutdown \{',raw)
if shutdown:
 guard=block('struct DeviceStorageShutdown {')+' deviceStorageShutdown;'
 # The source location, not a test-selected cleanup order, controls lifetime.
 found.append(re.search(re.escape(guard),raw))
 assert found[-1]
(a.out/'registry_globals.inc').write_text('\n'.join(x.group(0) for x in sorted(found,key=lambda x:x.start()))+'\n')
(a.out/'child_retirement.inc').write_text('template<typename Object, typename... Interfaces>\n'+block('void retireRegisteredChild(')+'\n'+block('void retireRegisteredQuery(')+'\n'+block('void clearRegisteredChildren(Device* device) noexcept')+'\n')
(a.out/'device_release.inc').write_text(block('void releaseDeviceStorage(')+'\n')
(a.out/'device_operation.inc').write_text(block('struct DeviceOperation {')+';\nthread_local DeviceOperation* DeviceOperation::current = nullptr;\n')
creation=raw[raw.index('HRESULT createDdiDevice('):]
registration=creation[creation.index('  try {'):creation.index('  struct Creation {')]
assert 'deviceStorage.emplace(h.pDrvPrivate, DeviceRecord{DevicePhase::Creating, owner})' in registration
(a.out/'device_registration.inc').write_text(registration)
fixture=Path(__file__).with_suffix('.cpp');(a.out/'fixture.cpp').write_bytes(fixture.read_bytes())
header=a.source/'src/umd/umd_private_children.h';(a.out/header.name).write_bytes(header.read_bytes())
process_header=a.source/'src/umd/umd_process_storage.h';(a.out/process_header.name).write_bytes(process_header.read_bytes())
exe=a.out/('test.exe' if a.wine else 'test')
command=[a.compiler,'-std=c++17','-O1','-g','-pthread','-I',str(a.out),str(a.out/'fixture.cpp'),'-o',str(exe)]
if a.wine:command+=['-static']
else:command+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
with (a.out/'build.log').open('w')as log:built=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
cases=[];env=dict(os.environ)
if a.wine:
 root=Path('/home/sunf/droidvm-repos/artifacts/vkd3d-umd-port-20261018')
 env.update(WINEDEBUG='-all',WINEPREFIX=str(root/'wine-prefix'),WINEDLLPATH=str(root/'wine-root/usr/lib/x86_64-linux-gnu/wine'),WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d')
else:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
if built.returncode==0:
 for name in ('empty','query','layout','both','mixed','explicit','repeated','reentrant','retained-reentrant','late-owner'):
  command_run=([str(a.wine)]if a.wine else[])+[str(exe.resolve()),name]
  with (a.out/(name+'.log')).open('w')as log:
   try:r=subprocess.run(command_run,stdout=log,stderr=subprocess.STDOUT,timeout=15,env=env);row=dict(case=name,exit=r.returncode)
   except subprocess.TimeoutExpired:row=dict(case=name,timeout=True)
  row['passed']=row.get('exit')==0 and 'PASS process-teardown case='+name in (a.out/(name+'.log')).read_text();cases.append(row)
receipt=dict(build_exit=built.returncode,command=command,cases=cases,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),header_sha256=hashlib.sha256(header.read_bytes()).hexdigest(),fixture_sha256=hashlib.sha256(fixture.read_bytes()).hexdigest(),globals=[dict(line=raw[:x.start()].count('\n')+1,declaration=x.group(0))for x in sorted(found,key=lambda x:x.start())],passed=built.returncode==0 and all(x['passed']for x in cases),boundary='Actual production static declarations/order, Device::close/destructor, child clear/retire and PrivateChildren. Controlled COM/runtime/GPU peers; real process CRT termination, no GPU/Windows-driver acceptance.')
(a.out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt));raise SystemExit(0 if receipt['passed']else 1)
