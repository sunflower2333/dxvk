#!/usr/bin/env python3
"""Owned synthetic file-format controls; no Windows/runtime/GPU evidence."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    output = Path(args.output); output.mkdir(exist_ok=False)
    reader_path = Path(__file__).with_name('verify-d3d11-system-present-originals.py')
    spec = importlib.util.spec_from_file_location('system_d11_reader', reader_path)
    reader = importlib.util.module_from_spec(spec); spec.loader.exec_module(reader)
    raw = output / 'owned-synthetic'; raw.mkdir()
    folder = r'C:\Controlled'
    front, core = folder + r'\viogpudxvk_validate11.dll', folder + r'\viogpudxvk.dll'
    loader, icd, config = folder + r'\viogpu_gl_loader_arm64.dll', folder + r'\viogpu_gl_vk_arm64.dll', folder + r'\owned-icd.json'
    system = r'C:\Windows\System32'
    manifest = dict(schema=1, api=11, passed=True, failure='', failureResult=0, factoryCalled=True, factoryResult=0, featureLevel=0xa000,
                    luidHigh=0, luidLow=0x1234, generation=2, capabilities=0, frontend=front, core=core,
                    privateLoader=loader, icd=icd, icdJson=config, effective=front, heldReleased=True, kmtClose=0,
                    pixels=512, softwareFallback=False, productionAdmission=False, registrationChangedByProbe=False, presents=2,
                    window=dict(handle=99,pid=123,thread=124,session=1,destroyed=True,classUnregistered=True,associationResult=0,associationFlags=3),
                    swapchain=dict(captured=True,width=16,height=16,format=28,sampleCount=1,sampleQuality=0,usage=32,
                                   bufferCount=1,outputWindow=99,windowed=True,effect=0,flags=0),
                    readbacks=[dict(map=0, removed=0, rowPitch=64, present=0, presentRemoved=0, syncInterval=1, presentFlags=0, windowedBefore=True, windowedAfter=True) for _ in range(2)],
                    loadedModules=dict(frontend=front,core=core,privateLoader=loader,icd=icd),
                    systemModules={name: system + '\\' + name for name in ('dxgi.dll', 'd3d11.dll', 'gdi32.dll', 'd3dcompiler_47.dll', 'user32.dll')})
    identity = bytearray(160)
    for offset, value in ((0,0x504d5644),(8,128),(24,2),(128,0x44494c56),(132,1),(136,32),(140,1),(144,0x1234),(152,1)):
        struct.pack_into('<I', identity, offset, value)
    (raw / 'identity.raw').write_bytes(identity)
    for name, pixel in (('clear.raw', bytes((0,0,0,255))), ('draw.raw', bytes((255,0,0,255)))):
        (raw / name).write_bytes(pixel * 256)
    for name, stage in (('vs.dxbc',1),('ps.dxbc',0)):
        chunks = [(b'ISGN',bytes(8)),(b'OSGN',bytes(8)),(b'SHDR',struct.pack('<II',(stage<<16)|0x40,2))]
        blob = bytearray(44)
        blob[:4] = b'DXBC'; struct.pack_into('<I',blob,28,len(chunks))
        for index, (tag, data) in enumerate(chunks):
            struct.pack_into('<I',blob,32+index*4,len(blob)); blob += tag + struct.pack('<I',len(data)) + data
        struct.pack_into('<I',blob,24,len(blob)); (raw / name).write_bytes(blob)
    def event(kind, **values):
        result = dict(sequence=0,call=kind,result=0,interface=0,version=0,flags=0,type=0,dataSize=0,capacity=0,count=0,caps=0,
                      argument=1,adapter=2,runtimeAdapter=0,kernelCallbacks=0,coreCallbacks=0,returnSize=0,versions=[])
        result.update(values); return result
    events = [event(0,runtimeAdapter=3),event(1,count=1,capacity=1,versions=[reader.SUPPORTED11]),
              event(2,type=130,dataSize=4,caps=1),event(2,type=128,dataSize=4),event(2,type=129,dataSize=4),
              event(3,interface=reader.DDI11,version=0x20009,flags=16,returnSize=256),
              event(4,interface=reader.DDI11,version=0x20009,flags=16,kernelCallbacks=4,coreCallbacks=5)]
    for index, e in enumerate(events,1): e['sequence'] = index
    negotiation = dict(schema=1,core=core,liveAdapters=1,events=events)
    after = copy.deepcopy(negotiation); after['liveAdapters']=0; after['events'].append(event(5,sequence=8))
    stdout = f'SYSTEM_D3D11_HELD pid=123 timeout_ms=60000 pixels_passed=1 stage= hr=00000000\n{reader.MARKER}\n'
    (output / 'stdout.raw').write_text(stdout)
    process = dict(pid=123,retained_process_handle=42,exited=True,exit_code_available=True,exit_code=0,timed_out=False,
                   child_still_running=False,pipes_drained=True,capture_failure=None,runner_sha256=reader.RUNNER,
                   stdout_bytes=len(stdout.encode()),stderr_bytes=0,start_utc='synthetic-not-native')
    held_path = raw.with_name(raw.name + '.held.json')
    held = dict(schema=1, api=11, pid=123, event=r'Local\SyntheticHold', timeout_ms=60000, output=folder+'\\'+raw.name,
                factoryCalled=True, factoryResult=0, pixelsPassed=True, stage='', result=0)
    def write():
        (raw / 'manifest.json').write_text(json.dumps(manifest))
        (raw / 'negotiation.json').write_text(json.dumps(negotiation))
        (raw / 'closed-negotiation.json').write_text(json.dumps(after))
        (output / 'process.json').write_text(json.dumps(process))
        held_path.write_text(json.dumps(held))
    def verify():
        return reader.verify(raw,output/'stdout.raw',output/'process.json',held_path,0,0x1234,front,core,loader,icd,config,system)
    write(); baseline = verify(); assert baseline['pixels']==512 and not baseline['hardware_admission']
    controls = []
    def reject(name, mutate, restore):
        mutate(); write()
        try:
            verify()
        except (AssertionError, KeyError, TypeError, ValueError):
            controls.append(dict(name=name,rejected=True))
        else:
            raise AssertionError(name)
        finally:
            restore(); write()
    for key, bad in [('passed',False),('factoryCalled',False),('factoryResult',-1),('featureLevel',0xb000),('heldReleased',False),
                     ('softwareFallback',True),('productionAdmission',True),('registrationChangedByProbe',True),('presents',0),
                     ('pixels',0),('kmtClose',-1),('luidLow',0xabcd),('frontend',folder+r'\other.dll'),('privateLoader',folder+r'\other.dll')]:
        old=manifest[key]
        reject('manifest-'+key,lambda k=key,b=bad:manifest.__setitem__(k,b),lambda k=key,v=old:manifest.__setitem__(k,v))
    for index in range(2):
        for key, bad in [('present',0x087a0001),('present',-2005270523),('presentRemoved',-1),('syncInterval',0),('presentFlags',1),('windowedBefore',False),('windowedAfter',False)]:
            old=manifest['readbacks'][index][key]
            reject('present-'+str(index)+'-'+key+'-'+str(bad),lambda i=index,k=key,b=bad:manifest['readbacks'][i].__setitem__(k,b),
                   lambda i=index,k=key,v=old:manifest['readbacks'][i].__setitem__(k,v))
    for key,bad in [('handle',0),('pid',124),('thread',0),('session',0),('destroyed',False),('classUnregistered',False),('associationResult',-1),('associationFlags',0)]:
        old=manifest['window'][key]
        reject('window-'+key,lambda k=key,b=bad:manifest['window'].__setitem__(k,b),lambda k=key,v=old:manifest['window'].__setitem__(k,v))
    for key,bad in [('captured',False),('width',0),('height',32),('format',87),('sampleCount',4),('sampleQuality',1),
                    ('usage',0),('bufferCount',2),('outputWindow',100),('windowed',False),('effect',3),('flags',1)]:
        old=manifest['swapchain'][key]
        reject('swapchain-'+key,lambda k=key,b=bad:manifest['swapchain'].__setitem__(k,b),lambda k=key,v=old:manifest['swapchain'].__setitem__(k,v))
    for index, key, bad in [(1,'versions',[reader.SUPPORTED11,0xa000100040000]),(2,'caps',0),(2,'caps',7),(3,'caps',1),
                            (4,'caps',2),(6,'interface',0xa0001),(6,'flags',2),(6,'flags',1),(6,'kernelCallbacks',0),(6,'coreCallbacks',0)]:
        old=negotiation['events'][index][key]
        reject('negotiation-'+str(index)+'-'+key+'-'+str(bad),lambda i=index,k=key,b=bad:negotiation['events'][i].__setitem__(k,b),
               lambda i=index,k=key,v=old:negotiation['events'][i].__setitem__(k,v))
    for key,bad in [('exit_code',1),('retained_process_handle',0),('pipes_drained',False),('child_still_running',True)]:
        old=process[key]
        reject('process-'+key,lambda k=key,b=bad:process.__setitem__(k,b),lambda k=key,v=old:process.__setitem__(k,v))
    for key in ('frontend','core','privateLoader','icd'):
        old=manifest['loadedModules'][key]
        reject('loaded-'+key,lambda k=key:manifest['loadedModules'].__setitem__(k,folder+r'\other.dll'),
               lambda k=key,v=old:manifest['loadedModules'].__setitem__(k,v))
    for key, bad in [('pid',124),('api',10),('event',r'Global\SyntheticHold'),('timeout_ms',60001),('factoryResult',-1),('pixelsPassed',False),('stage','failed')]:
        old=held[key]
        reject('held-'+key,lambda k=key,b=bad:held.__setitem__(k,b),lambda k=key,v=old:held.__setitem__(k,v))
    for name in ('clear.raw','draw.raw'):
        path=raw/name; original=path.read_bytes()
        for offset in (0,1,2,3,1020,1021,1022,1023):
            damaged=bytearray(original); damaged[offset]^=1
            reject(name+'-byte-'+str(offset),lambda p=path,d=damaged:p.write_bytes(d),lambda p=path,d=original:p.write_bytes(d))
    path=raw/'identity.raw'; original=path.read_bytes(); damaged=bytearray(original); damaged[144]^=1
    reject('private-identity-luid',lambda:path.write_bytes(damaged),lambda:path.write_bytes(original))
    for name in ('vs.dxbc','ps.dxbc'):
        path=raw/name; original=path.read_bytes(); damaged=bytearray(original); struct.pack_into('<I',damaged,24,len(damaged)+1)
        reject(name+'-container-length',lambda p=path,d=damaged:p.write_bytes(d),lambda p=path,d=original:p.write_bytes(d))
    final=verify(); assert final==baseline
    # The original Khronos loader can be staged under this private basename.
    # Its actual path must still be the exact approved operand, never foreign.
    original_loader = loader
    loader = folder + r'\winevulkan.dll'
    manifest['privateLoader'] = manifest['loadedModules']['privateLoader'] = loader
    write(); wine_tuple = verify(); assert wine_tuple['pixels'] == 512 and not wine_tuple['hardware_admission']
    reject('wine-loader-foreign-path', lambda: manifest['loadedModules'].__setitem__('privateLoader', r'C:\Foreign\winevulkan.dll'),
           lambda: manifest['loadedModules'].__setitem__('privateLoader', loader))
    loader = original_loader
    manifest['privateLoader'] = manifest['loadedModules']['privateLoader'] = loader
    write(); assert verify() == baseline
    proof=dict(scope='owned-synthetic-format-controls-only',native_execution=False,hardware_admission=False,
               controls=controls,rejections=len(controls),baseline_literal_bytes=2048,
               approved_wine_named_loader_tuple=True,
               reader_sha256=hashlib.sha256(reader_path.read_bytes()).hexdigest())
    with (output/'reader-controls-verified.json').open('x') as stream: json.dump(proof,stream,indent=2); stream.write('\n')
    print('SYSTEM_D3D11_PRESENT_READER_CONTROLS_PASS rejected='+str(len(controls))+' literal_bytes=2048 synthetic_only=1')


if __name__=='__main__': main()
