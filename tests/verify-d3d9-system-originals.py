#!/usr/bin/env python3
"""Independent literal reader for actual public SYSTEM D3D9/9Ex raw planes.

This validates saved bytes/metadata, never device execution, restoration or
hardware admission by itself. --selftest-root creates only synthetic reader input.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct

CLEAR_ROWS = (
    (0, 2, ()),
    (2, 7, ((1, 6, 0xff9a4c23),)),
    (7, 8, ()),
    (8, 13, ((10, 15, 0xff256eba),)),
    (13, 16, ()),
)
DRAW_ROWS = (
    (0, 1, ()),
    (1, 6, ((2, 9, 0xffbd672d),)),
    (6, 9, ()),
    (9, 13, ((11, 15, 0xff36a4d1),)),
    (13, 15, ((0, 3, 0xff85c239), (11, 15, 0xff36a4d1))),
    (15, 16, ((0, 3, 0xff85c239),)),
)


def require(value, message):
    if not value:
        raise ValueError(message)


def original(path):
    require(path.is_file() and not path.is_symlink(), 'regular original required: ' + str(path))
    raw = path.read_bytes()
    return raw, dict(name=path.name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def literal(frame, screen=False):
    rows = CLEAR_ROWS if frame == 'clear' else DRAW_ROWS
    background = 0xff173b61 if frame == 'clear' else 0xff421a75
    words = []
    for top, bottom, spans in rows:
        for _ in range(top, bottom):
            row = [background] * 16
            for left, right, color in spans:
                row[left:right] = [color] * (right-left)
            words.extend(row)
    require(len(words) == 256, 'fixed literal shape')
    if screen:
        words = [((word >> 16) & 255) | (word & 0xff00) | ((word & 255) << 16) for word in words]
    return struct.pack('<256I', *words)


def verify(directory, api, phase, luid, paths, stdout=None, stderr=None):
    require(api in ('9', '9ex') and phase in ('offscreen', 'present'), 'explicit API/phase required')
    require(re.fullmatch(r'[0-9a-fA-F]{8}:[0-9a-fA-F]{8}', luid) and int(luid.replace(':', ''),16), 'nonzero exact LUID')
    high, low = [int(part,16) for part in luid.split(':')]
    files = {'clear.raw','draw.raw','manifest.json'}
    if phase == 'present':
        files |= {'clear-screen.raw','draw-screen.raw'}
    require(directory.is_dir() and not directory.is_symlink(), 'regular original directory required')
    require({p.name for p in directory.iterdir()} == files, 'exact closed raw-directory members required')
    originals = {}
    for name in sorted(files):
        raw, row = original(directory / name)
        originals[name] = row
        if name.endswith('.raw'):
            require(raw == literal(name.split('-')[0].split('.')[0], '-screen' in name), 'literal byte mismatch: ' + name)
    manifest = json.loads(original(directory / 'manifest.json')[0])
    wanted = dict(schema='ordinary-system-d3d9-literal-v1',api=api,phase=phase,width=16,height=16,
        frames=2,pixels=512,presents=2 if phase == 'present' else 0,
        screenPixels=512 if phase == 'present' else 0,luidHigh=high,luidLow=low,
        vendor=6900,device=4176,format=21,hardwareVertexProcessing=True,softwareFallback=False,
        registrationChangedByProbe=False,productionAdmission=False)
    for key, value in wanted.items():
        require(manifest.get(key) == value and type(manifest.get(key)) is type(value), 'manifest literal field differs: ' + key)
    require(set(manifest) == set(wanted) | {'generation','capabilities','ordinal','session','frontend','core','loader','icd','systemRuntime','frameResults'}, 'exact manifest schema')
    for key in ('generation','capabilities','ordinal','session'):
        require(type(manifest[key]) is int, 'integer metadata: ' + key)
    require(manifest['generation'] > 0 and 0 <= manifest['capabilities'] < 2**64
        and 0 <= manifest['ordinal'] < 16 and manifest['session'] > 0, 'bounded native identity/session')
    for key in ('frontend','core','loader','icd'):
        require(re.fullmatch(r'[A-Za-z]:\\.+', paths[key]) and manifest[key].lower() == paths[key].lower(), 'exact module path: ' + key)
    runtime = manifest['systemRuntime'].lower()
    require(re.fullmatch(r'[a-z]:\\windows\\(?:system32|syswow64)\\d3d9\.dll',runtime), 'genuine SYSTEM runtime path')
    frames = manifest['frameResults']
    require(type(frames) is list and len(frames) == 2, 'two exact frame results')
    for frame in frames:
        require(set(frame) == {'rowPitch','cooperative','present','screenAttempts'} and all(type(v) is int for v in frame.values()), 'typed frame result')
        require(64 <= frame['rowPitch'] <= 2**31-1 and frame['cooperative'] == 0, 'successful bounded readback')
        require(frame['present'] == (0 if phase == 'present' else -2147467259), 'exact present status')
        require((1 <= frame['screenAttempts'] <= 81) if phase == 'present' else frame['screenAttempts'] == 0, 'bounded screen observation')
    held_path = directory.with_name(directory.name + '.held.json')
    held_raw, held_pin = original(held_path)
    held = json.loads(held_raw)
    require(set(held) == {'schema','pid','timeout_ms','pending_exit','hold_event','restoration_proved_by_event'}
        and held['schema'] == 'ordinary-system-d3d9-held-v1' and type(held['pid']) is int and held['pid'] > 0
        and type(held['timeout_ms']) is int and 1 <= held['timeout_ms'] <= 60000
        and type(held['pending_exit']) is int and held['pending_exit'] == 0
        and type(held['hold_event']) is str and held['hold_event'].startswith('Local\\') and len(held['hold_event']) > 6
        and held['restoration_proved_by_event'] is False, 'exact closed hold checkpoint')
    trace_verified = False
    if stdout is not None:
        require(stderr is not None and original(stderr)[0] == b'', 'actual successful stderr required')
        trace = original(stdout)[0].decode('ascii').splitlines()
        require(trace.count(f'D9_SYSTEM_RESULTS_READY api={api} phase={phase} pixels=512 presents={wanted["presents"]} screen_pixels={wanted["screenPixels"]} production_admission=0') == 1, 'actual results marker')
        require(trace.count(f'D9_SYSTEM_HELD pid={held["pid"]} timeout_ms={held["timeout_ms"]} pending_exit=0 restoration_not_proved_by_event=1') == 1
            and trace.count('D9_SYSTEM_DEVICE_RELEASE remaining=0') == 1
            and trace[-1] == 'D9_SYSTEM_DONE exit=0 production_admission=0 registry_changes=0', 'actual hold/teardown markers')
        trace_verified = True
    return dict(schema='ordinary-system-d3d9-literal-readback-v1',verified=True,api=api,phase=phase,
        original_files=list(originals.values()),held_checkpoint=held_pin,pixels=512,screen_pixels=wanted['screenPixels'],presents=wanted['presents'],
        observations=512+wanted['screenPixels'],stdout_verified=trace_verified,
        process_closure_verified=False,registry_restoration_verified=False,hardware_admission=False,
        production_admission=False,synthetic_reader_input=False)


def selftest(root):
    require(root.is_absolute() and not root.exists(), 'fresh selftest root')
    root.mkdir(parents=True)
    paths = dict(frontend=r'C:\Users\Public\Candidate\viogpudxvk9x.dll',core=r'C:\Users\Public\Candidate\arm64\viogpudxvk.dll',loader=r'C:\Users\Public\Candidate\arm64\viogpu_gl_loader.dll',icd=r'C:\Users\Public\Candidate\arm64\vulkan_adreno.dll')
    accepted = 0
    for api in ('9','9ex'):
        for phase in ('offscreen','present'):
            d=root/(api+'-'+phase);d.mkdir()
            for frame in ('clear','draw'):
                (d/(frame+'.raw')).write_bytes(literal(frame))
                if phase == 'present': (d/(frame+'-screen.raw')).write_bytes(literal(frame,True))
            manifest=dict(schema='ordinary-system-d3d9-literal-v1',api=api,phase=phase,width=16,height=16,frames=2,pixels=512,
                presents=2 if phase=='present' else 0,screenPixels=512 if phase=='present' else 0,luidHigh=0,luidLow=1,
                generation=1,capabilities=0,ordinal=0,vendor=6900,device=4176,format=21,session=1,**paths,
                systemRuntime=r'C:\Windows\System32\d3d9.dll',hardwareVertexProcessing=True,softwareFallback=False,
                registrationChangedByProbe=False,productionAdmission=False,
                frameResults=[dict(rowPitch=256,cooperative=0,present=0 if phase=='present' else -2147467259,screenAttempts=1 if phase=='present' else 0)]*2)
            (d/'manifest.json').write_text(json.dumps(manifest))
            d.with_name(d.name+'.held.json').write_text(json.dumps(dict(schema='ordinary-system-d3d9-held-v1',pid=1,timeout_ms=60000,pending_exit=0,hold_event=r'Local\D9ReaderOnly',restoration_proved_by_event=False)))
            stdout=root/(api+'-'+phase+'.stdout.raw');stderr=root/(api+'-'+phase+'.stderr.raw')
            stdout.write_text(f'D9_SYSTEM_RESULTS_READY api={api} phase={phase} pixels=512 presents={manifest["presents"]} screen_pixels={manifest["screenPixels"]} production_admission=0\n'
                'D9_SYSTEM_HELD pid=1 timeout_ms=60000 pending_exit=0 restoration_not_proved_by_event=1\n'
                'D9_SYSTEM_DEVICE_RELEASE remaining=0\nD9_SYSTEM_DONE exit=0 production_admission=0 registry_changes=0\n')
            stderr.write_bytes(b'')
            verify(d,api,phase,'00000000:00000001',paths,stdout,stderr);accepted+=1
    base=root/'9-present'
    mutations=[('clear-byte',lambda d:(d/'clear.raw').write_bytes(b'\0'+(d/'clear.raw').read_bytes()[1:])),
        ('draw-byte',lambda d:(d/'draw.raw').write_bytes((d/'draw.raw').read_bytes()[:-1]+b'\0')),
        ('screen-byte',lambda d:(d/'draw-screen.raw').write_bytes(b'\0'+(d/'draw-screen.raw').read_bytes()[1:])),
        ('missing',lambda d:(d/'draw.raw').unlink()),('extra',lambda d:(d/'extra.raw').write_bytes(b'x')),
        ('truncated',lambda d:(d/'clear.raw').write_bytes(b'x')),('symlink',lambda d:((d/'draw.raw').unlink(),(d/'draw.raw').symlink_to(base/'draw.raw')))]
    for field,value in [('vendor',1),('luidLow',2),('softwareFallback',True),('productionAdmission',True),('frontend',paths['core']),('presents',1),('format',22)]:
        def mutate(d,field=field,value=value):
            p=d/'manifest.json';m=json.loads(p.read_text());m[field]=value;p.write_text(json.dumps(m))
        mutations.append((field,mutate))
    def wronghold(d):
        p=d.with_name(d.name+'.held.json');m=json.loads(p.read_text());m['pending_exit']=1;p.write_text(json.dumps(m))
    mutations.append(('failed-hold',wronghold))
    rejected=0
    for name,mutate in mutations:
        d=root/('reject-'+name);shutil.copytree(base,d);shutil.copyfile(base.with_name(base.name+'.held.json'),d.with_name(d.name+'.held.json'));mutate(d)
        try: verify(d,'9','present','00000000:00000001',paths)
        except (ValueError,KeyError,TypeError): rejected+=1
        else: raise AssertionError('mutation accepted: '+name)
    return dict(verified=True,accepted_synthetic=accepted,rejected_controls=rejected,native_execution=False,hardware_admission=False)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--directory',type=Path);p.add_argument('--api',choices=('9','9ex'));p.add_argument('--phase',choices=('offscreen','present'))
    p.add_argument('--luid')
    for key in ('frontend','core','loader','icd'):p.add_argument('--'+key)
    p.add_argument('--stdout',type=Path);p.add_argument('--stderr',type=Path)
    p.add_argument('--selftest-root',type=Path);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();require(not args.output.exists(),'fresh reader output')
    if args.selftest_root: result=selftest(args.selftest_root)
    else:
        require(all((args.directory,args.api,args.phase,args.luid,args.frontend,args.core,args.loader,args.icd)),'all actual identity/phase arguments required')
        require((args.stdout is None)==(args.stderr is None),'stdout/stderr pair required')
        require(args.directory.resolve() not in (args.output.resolve(), *args.output.resolve().parents), 'reader output must remain outside raw directory')
        result=verify(args.directory,args.api,args.phase,args.luid,{k:getattr(args,k) for k in ('frontend','core','loader','icd')},args.stdout,args.stderr)
    args.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result if args.selftest_root else {k:result[k] for k in ('verified','api','phase','observations','hardware_admission')}))


if __name__=='__main__':main()
