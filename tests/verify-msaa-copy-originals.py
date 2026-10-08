#!/usr/bin/env python3
"""Independent literal MSAA array-copy and no-mutation original reader."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

U32=4294967295
PIXELS={1:b'\xff\0\0\xff',2:b'\0\xff\0\xff',3:b'\xff\xff\0\xff',
        4:b'\0\0\xff\xff',5:b'\xff\0\xff\xff',6:b'\0\xff\xff\xff'}
SOURCE=(1,2,4)
DESTINATION=(3,5,6)


def recipe():
    rows=[]
    scene=copies=noops=rejections=0
    def add(profile,samples,fmt,phase,control,colors):
        rows.append(dict(id=len(rows),profile=profile,scene=scene,phase=phase,control=control,
            callbacks=rejections,copies=copies,noops=noops,rejections=rejections,
            descriptor=[8,4,1,3,fmt,samples,0,0,32,0,0],colors=colors))
    for profile in range(2):
        for samples in (2,4):
            for operation in range(2):
                for same in range(2):
                    for cast in range(2):
                        fmt=29 if cast else 28
                        add(profile,samples,fmt,0,U32,DESTINATION)
                        copies+=1
                        add(profile,samples,fmt,1,U32,(6,5,6) if same else (3,4,6))
                        add(profile,samples,28,2,U32,SOURCE)
                        scene+=1
        for samples in (2,4):
            for control in range(21):
                rejections+=1
                add(profile,samples,28,3,control,DESTINATION)
            for control in range(21,27):
                noops+=1
                add(profile,samples,28,4,control,DESTINATION)
            scene+=1
    assert len(rows)==204 and (scene,copies,noops,rejections)==(36,32,24,84)
    return rows


def literal(colors):
    return b''.join(PIXELS[value]*32 for value in colors)


def unique_object(pairs):
    result={}
    for key,value in pairs:
        if key in result:
            raise ValueError('duplicate metadata key')
        result[key]=value
    return result


def uint(value):
    return type(value) is int and 0<=value<=U32


def verify(directory):
    directory=Path(directory)
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('original directory must be a regular directory')
    names=[f'msaa-copy-{i:03}-{role}.bin' for i in range(204) for role in ('native','public')]
    names += [f'msaa-copy-{i:03}-metadata.json' for i in range(204)]
    entries=list(directory.iterdir())
    if sorted(p.name for p in entries)!=sorted(names):
        raise ValueError('missing, extra or renamed original files')
    if any(p.is_symlink() or not p.is_file() for p in entries):
        raise ValueError('all original files must be regular files')
    pins=[]
    for expected in recipe():
        index=expected['id']
        metadata_path=directory/f'msaa-copy-{index:03}-metadata.json'
        raw_metadata=metadata_path.read_bytes()
        actual=json.loads(raw_metadata,object_pairs_hook=unique_object)
        scalar_keys=('id','profile','scene','phase','control','callbacks','copies','noops','rejections')
        keys=set(scalar_keys)|{'native_descriptor','public_descriptor','native_row_pitches','public_row_pitches',
            'native_depth_pitches','public_depth_pitches','bytes_each_role','hardware_admission','registration'}
        if type(actual) is not dict or set(actual)!=keys:
            raise ValueError(f'metadata keys: {index}')
        for key in scalar_keys:
            if not uint(actual[key]) or actual[key]!=expected[key]:
                raise ValueError(f'literal control metadata {key}: {index}')
        for role in ('native','public'):
            descriptor=actual[role+'_descriptor']
            if type(descriptor) is not list or not all(uint(value) for value in descriptor) or descriptor!=expected['descriptor']:
                raise ValueError(f'actual {role} descriptor: {index}')
            for suffix in ('_row_pitches','_depth_pitches'):
                pitches=actual[role+suffix]
                if type(pitches) is not list or len(pitches)!=3 or not all(uint(value) for value in pitches):
                    raise ValueError(f'actual {role} pitches: {index}')
                if suffix=='_row_pitches' and any(value<32 for value in pitches):
                    raise ValueError(f'actual {role} row span: {index}')
            name=f'msaa-copy-{index:03}-{role}.bin'
            raw=(directory/name).read_bytes()
            if raw!=literal(expected['colors']):
                raise ValueError(f'literal full-array oracle: {name}')
            pins.append(dict(name=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
        if type(actual['bytes_each_role']) is not int or actual['bytes_each_role']!=384:
            raise ValueError(f'byte count: {index}')
        if actual['hardware_admission'] is not False or actual['registration'] is not False:
            raise ValueError(f'unsupported admission claim: {index}')
        pins.append(dict(name=metadata_path.name,bytes=len(raw_metadata),sha256=hashlib.sha256(raw_metadata).hexdigest()))
    return dict(verified=True,profiles=2,scenes=36,copies=32,snapshots=204,rejections=84,noops=24,
        raw_files=612,bytes_each_role=78336,byte_observations=156672,original_files=pins,
        hardware_admission=False,registration=False,
        public_scope='positive actual public copies/resolves; rejected/empty DDI calls use untouched independent public sentinel resources',
        scope='literal resolved bytes and actual descriptor/pitch/callback originals; producer execution requires independently joined process/source evidence')


def synthesize(directory):
    directory.mkdir()
    for row in recipe():
        data={key:value for key,value in row.items() if key not in ('descriptor','colors')}
        data.update(native_descriptor=row['descriptor'],public_descriptor=row['descriptor'],
            native_row_pitches=[64]*3,public_row_pitches=[32]*3,native_depth_pitches=[256]*3,public_depth_pitches=[128]*3,
            bytes_each_role=384,hardware_admission=False,registration=False)
        (directory/f"msaa-copy-{row['id']:03}-metadata.json").write_text(json.dumps(data)+'\n')
        for role in ('native','public'):
            (directory/f"msaa-copy-{row['id']:03}-{role}.bin").write_bytes(literal(row['colors']))


def selftest(root):
    root=Path(root)
    if not root.is_dir():
        raise ValueError('selftest root must exist')
    scratch=Path(tempfile.mkdtemp(prefix='msaa-copy-reader-',dir=root))
    baseline=scratch/'synthetic-baseline'; synthesize(baseline)
    positive=verify(baseline)
    controls=[]
    def probe(name,mutate):
        folder=scratch/name; shutil.copytree(baseline,folder)
        mutate(folder)
        try:
            verify(folder)
        except (ValueError,OSError,UnicodeError) as error:
            controls.append(dict(name=name,rejected=True,error=str(error)))
        else:
            raise AssertionError('reader accepted '+name)
    def metadata(folder,key,value,index=0):
        path=folder/f'msaa-copy-{index:03}-metadata.json'; value_object=json.loads(path.read_text())
        value_object[key]=value; path.write_text(json.dumps(value_object)+'\n')
    def corrupt(folder,role,index=0):
        path=folder/f'msaa-copy-{index:03}-{role}.bin'; data=bytearray(path.read_bytes()); data[0]^=255; path.write_bytes(data)
    probe('missing-native',lambda d:(d/'msaa-copy-000-native.bin').unlink())
    probe('missing-metadata',lambda d:(d/'msaa-copy-000-metadata.json').unlink())
    probe('extra-runner',lambda d:(d/'process.json').write_text('{}'))
    probe('renamed-original',lambda d:(d/'msaa-copy-000-public.bin').rename(d/'renamed.bin'))
    def symlink(folder):
        path=folder/'msaa-copy-000-native.bin'; path.unlink(); path.symlink_to(baseline/path.name)
    probe('symlink-original',symlink)
    probe('native-literal-corruption',lambda d:corrupt(d,'native'))
    probe('public-literal-corruption',lambda d:corrupt(d,'public'))
    probe('matching-wrong-planes',lambda d:(corrupt(d,'native',1),corrupt(d,'public',1)))
    probe('source-preservation',lambda d:corrupt(d,'native',2))
    probe('rejected-write-sentinel',lambda d:corrupt(d,'native',48))
    probe('empty-write-sentinel',lambda d:corrupt(d,'native',69))
    probe('wrong-native-sample-count',lambda d:metadata(d,'native_descriptor',[8,4,1,3,28,1,0,0,32,0,0]))
    probe('wrong-public-quality',lambda d:metadata(d,'public_descriptor',[8,4,1,3,28,2,1,0,32,0,0]))
    probe('wrong-array-count',lambda d:metadata(d,'native_descriptor',[8,4,1,2,28,2,0,0,32,0,0]))
    probe('wrong-format-cast',lambda d:metadata(d,'public_descriptor',[8,4,1,3,28,2,0,0,32,0,0],3))
    probe('wrong-width',lambda d:metadata(d,'native_descriptor',[16,4,1,3,28,2,0,0,32,0,0]))
    probe('short-native-row',lambda d:metadata(d,'native_row_pitches',[31,64,64]))
    probe('short-public-row',lambda d:metadata(d,'public_row_pitches',[32,31,32]))
    probe('boolean-pitch',lambda d:metadata(d,'native_depth_pitches',[True,256,256]))
    probe('negative-pitch',lambda d:metadata(d,'native_depth_pitches',[-1,256,256]))
    probe('missing-pitch-layer',lambda d:metadata(d,'public_row_pitches',[32,32]))
    probe('wrong-rejection-callback',lambda d:metadata(d,'callbacks',0,48))
    probe('wrong-copy-count',lambda d:metadata(d,'copies',0,1))
    probe('wrong-noop-count',lambda d:metadata(d,'noops',0,69))
    probe('wrong-control',lambda d:metadata(d,'control',1,48))
    probe('wrong-scene',lambda d:metadata(d,'scene',1))
    probe('wrong-phase',lambda d:metadata(d,'phase',1))
    probe('wrong-byte-count',lambda d:metadata(d,'bytes_each_role',383))
    probe('extra-metadata-key',lambda d:metadata(d,'unrecorded',0))
    probe('hardware-claim',lambda d:metadata(d,'hardware_admission',True))
    probe('registration-claim',lambda d:metadata(d,'registration',True))
    def duplicate(folder):
        path=folder/'msaa-copy-000-metadata.json'; text=path.read_text(); path.write_text(text.replace('{','{"id":0,',1))
    probe('duplicate-key',duplicate)
    result=dict(passed=True,synthetic_only=True,no_native_execution=True,no_compiler_execution=True,
        controls=len(controls),rejected_controls=controls,positive=positive,scratch=str(scratch))
    with (scratch/'reader-selftest-result.json').open('x') as file:
        json.dump(result,file,indent=2); file.write('\n')
    return result


def main():
    parser=argparse.ArgumentParser()
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--directory',type=Path)
    mode.add_argument('--selftest-root',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=verify(args.directory) if args.directory else selftest(args.selftest_root)
    if args.output:
        if args.directory and args.output.resolve().parent==args.directory.resolve():
            raise ValueError('reader JSON must be outside original directory')
        with args.output.open('x') as file:
            json.dump(result,file,indent=2); file.write('\n')
    if args.directory:
        print('MSAA color copy originals PASS profiles=2 scenes=36 snapshots=204 copies=32 bytes=78336 rejections=84 noops=24 raw_files=612 hardware_admission=0')
    else:
        print(f"MSAA color copy reader selftest PASS controls={result['controls']} synthetic_only=1 no_native_execution=1")


if __name__=='__main__':
    main()
