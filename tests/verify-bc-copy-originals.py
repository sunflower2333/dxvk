#!/usr/bin/env python3
"""Independent fixed encoded-block oracle for typed BC regional-copy originals."""
import argparse
import copy
import hashlib
import json
from pathlib import Path

PAIRS=((70,72,8),(74,73,16),(78,77,16),(80,81,8),(84,82,16))
FIELDS={'profile','pair','cube','phase','role','format','width','height','mips','layers',
        'block_bytes','bytes','native_pitches','public_pitches','hardware_admission','registration'}

def initial(profile,pair,cube,seed):
    width,layers=(16,6) if cube else (24,2)
    block=PAIRS[pair][2]; planes=[]; rows=[]
    for sub in range(5*layers):
        mip=sub%5; w=max(1,width>>mip); h=max(1,16>>mip)
        row=((w+3)//4)*block; height=(h+3)//4
        planes.append(bytearray((seed+profile*41+pair*17+cube*13+sub*29+y*7+x)&255
            for y in range(height) for x in range(row)))
        rows.append(row)
    return planes,rows

def expected(profile,pair,cube,phase,role):
    source,rows=initial(profile,pair,cube,0x17)
    destination,_=initial(profile,pair,cube,0xa3)
    if role=='source': return b''.join(source)
    if role=='immutable': return b''.join(destination)
    if phase<2: return b''.join(source)
    block=PAIRS[pair][2]; last=25 if cube else 5
    # Literal encoded block-row slices. No producer geometry/helper is imported.
    for y in range(2):
        destination[last][(y+1)*rows[last]+2*block:(y+1)*rows[last]+4*block]=source[0][(y+1)*rows[0]+block:(y+1)*rows[0]+3*block]
    col=0 if cube else 1
    destination[last+2][col*block:(col+1)*block]=source[2][col*block:(col+1)*block]
    destination[last+3][:block]=source[3][:block]
    destination[last+4][:block]=source[4][:block]
    destination[last][3*rows[last]+block:3*rows[last]+2*block]=source[3][:block]
    destination[last][:block]=destination[0][:block]
    return b''.join(destination)

def recipe():
    for profile in range(2):
        for pair in range(5):
            for cube in range(2):
                for phase in range(4):
                    for role in ('destination','source'): yield profile,pair,cube,phase,role
                yield profile,pair,cube,4,'immutable'

def name(row): return 'bc-copy-'+'-'.join(map(str,row))
def unique(pairs):
    result={}
    for key,value in pairs:
        if key in result: raise ValueError('duplicate JSON key')
        result[key]=value
    return result
def integer(value): return type(value) is int and 0<=value<=0xffffffff

def verify(directory):
    directory=Path(directory); rows=list(recipe())
    wanted={name(row)+suffix for row in rows for suffix in ('.native.bin','.public.bin','.layout.json')}
    actual={path.name for path in directory.iterdir()}
    if actual!=wanted: raise ValueError('missing, extra or renamed originals')
    if any(path.is_symlink() or not path.is_file() for path in directory.iterdir()): raise ValueError('originals must be regular files')
    pins=[]; byte_total=0; subresources=0
    for row in rows:
        profile,pair,cube,phase,role=row; stem=name(row)
        width,layers=(16,6) if cube else (24,2); fmt=PAIRS[pair][0 if role=='source' else 1]; block=PAIRS[pair][2]
        oracle=expected(*row); path=directory/(stem+'.layout.json')
        original=path.read_bytes(); metadata=json.loads(original,object_pairs_hook=unique)
        if set(metadata)!=FIELDS: raise ValueError('metadata fields')
        fixed=dict(profile=profile,pair=pair,cube=cube,phase=phase,role=role,format=fmt,width=width,height=16,mips=5,layers=layers,block_bytes=block,bytes=len(oracle))
        for key,value in fixed.items():
            if metadata[key]!=value or (type(value) is int and not integer(metadata[key])): raise ValueError(f'fixed metadata {stem}:{key}')
        if metadata['hardware_admission'] is not False or metadata['registration'] is not False: raise ValueError('admission modified')
        for key in ('native_pitches','public_pitches'):
            pitches=metadata[key]
            if type(pitches) is not list or len(pitches)!=5*layers: raise ValueError('mapped pitch count')
            for sub,pitch in enumerate(pitches):
                row_bytes=((max(1,width>>(sub%5))+3)//4)*block
                if not integer(pitch) or pitch<row_bytes: raise ValueError('mapped block-row pitch')
        pins.append(dict(name=path.name,bytes=len(original),sha256=hashlib.sha256(original).hexdigest()))
        for suffix in ('.native.bin','.public.bin'):
            path=directory/(stem+suffix); original=path.read_bytes()
            if original!=oracle: raise ValueError(f'encoded block oracle mismatch: {path.name}')
            pins.append(dict(name=path.name,bytes=len(original),sha256=hashlib.sha256(original).hexdigest()))
        byte_total+=len(oracle); subresources+=5*layers
    return dict(verified=True,profiles=2,scenes=20,copies=160,rejections=400,noops=120,
        snapshots=len(rows),subresources=subresources,raw_files=len(wanted),bytes_each_role=byte_total,
        byte_observations=2*byte_total,files=pins,hardware_admission=False,registration=False,
        scope='Independent literal encoded block originals; native process/build and GPU admission need separate evidence')

def selftest(root):
    root=Path(root); root.mkdir(parents=True,exist_ok=True); raw=root/'synthetic-originals'; raw.mkdir()
    for row in recipe():
        profile,pair,cube,phase,role=row; width,layers=(16,6) if cube else (24,2); block=PAIRS[pair][2]
        oracle=expected(*row); stem=name(row)
        pitches=[((max(1,width>>(sub%5))+3)//4)*block+16 for sub in range(5*layers)]
        metadata=dict(profile=profile,pair=pair,cube=cube,phase=phase,role=role,
            format=PAIRS[pair][0 if role=='source' else 1],width=width,height=16,mips=5,layers=layers,
            block_bytes=block,bytes=len(oracle),native_pitches=pitches,public_pitches=pitches,
            hardware_admission=False,registration=False)
        (raw/(stem+'.layout.json')).write_text(json.dumps(metadata))
        for suffix in ('.native.bin','.public.bin'): (raw/(stem+suffix)).write_bytes(oracle)
    golden=verify(raw); controls=1
    def rejected():
        nonlocal controls
        try: verify(raw)
        except (ValueError,OSError,json.JSONDecodeError): controls+=1; return
        raise AssertionError('malformed reader control was accepted')
    for stem in ('bc-copy-0-0-0-2-destination','bc-copy-1-2-1-3-source','bc-copy-0-3-0-4-immutable'):
        for suffix in ('.native.bin','.public.bin'):
            path=raw/(stem+suffix); before=path.read_bytes(); corrupt=bytearray(before); corrupt[-1]^=1
            path.write_bytes(corrupt); rejected(); path.write_bytes(before)
            path.write_bytes(before[:-1]); rejected(); path.write_bytes(before)
    path=raw/'bc-copy-0-0-0-2-destination.layout.json'; before=path.read_bytes(); metadata=json.loads(before)
    for key in ('profile','pair','cube','phase','format','width','height','mips','layers','block_bytes','bytes'):
        altered=copy.deepcopy(metadata); altered[key]=True; path.write_text(json.dumps(altered)); rejected(); path.write_bytes(before)
    for key in ('native_pitches','public_pitches'):
        for pitches in ([],[0]*10,[True]*10):
            altered=copy.deepcopy(metadata); altered[key]=pitches; path.write_text(json.dumps(altered)); rejected(); path.write_bytes(before)
    for key in ('hardware_admission','registration'):
        altered=copy.deepcopy(metadata); altered[key]=True; path.write_text(json.dumps(altered)); rejected(); path.write_bytes(before)
    path.write_bytes(before[:-1]+b',"profile":0}'); rejected(); path.write_bytes(before)
    extra=raw/'extra.bin'; extra.write_bytes(b'x'); rejected(); extra.unlink()
    target=raw/'bc-copy-0-0-0-0-source.native.bin'; renamed=raw/'renamed.bin'
    target.rename(renamed); rejected(); renamed.rename(target)
    before=target.read_bytes(); target.unlink(); rejected(); target.write_bytes(before)
    assert verify(raw)==golden
    return dict(verified=True,controls=controls,synthetic_only=True,native_execution=False,
        originals540=True,golden=golden,hardware_admission=False,registration=False)

def main():
    parser=argparse.ArgumentParser(); selected=parser.add_mutually_exclusive_group(required=True)
    selected.add_argument('--directory',type=Path); selected.add_argument('--selftest-root',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args(); result=selftest(args.selftest_root) if args.selftest_root else verify(args.directory)
    if args.output:
        with args.output.open('x') as file: json.dump(result,file,indent=2); file.write('\n')
    if args.selftest_root: print(f"BC copy reader controls PASS controls={result['controls']} synthetic_only=1 native_execution=0")
    else: print(f"BC copy originals PASS snapshots=180 raw_files=540 bytes={result['byte_observations']} hardware_admission=0")

if __name__=='__main__': main()
