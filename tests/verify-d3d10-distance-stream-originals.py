#!/usr/bin/env python3
"""Verify original clip/cull SO programs, captured words and query counters."""
import argparse, hashlib, json, re, struct, tempfile
from pathlib import Path
SOURCE_SHA256="99f1c6067fd4aa7213b4cddfc39b8878409b6c3413f390fe2c82b2a1e13b2028"
def shader_cases():
    index=0
    for model in (40,41):
        for entry,stage in (("vs",1),("vs_alternate",1),("gs",2)):
            index+=1
            profile=f"{'gs' if stage==2 else 'vs'}_4_{model%10}"
            yield f"distance-fxc-{index:02d}-{entry}-{profile}",stage,model
def shader_chunk(binary, stage, model):
    """Independently bound every DXBC chunk before extracting original SM4 code."""
    if len(binary) < 36 or binary[:4] != b"DXBC":
        raise AssertionError("Original DXBC header")
    one, extent, count = struct.unpack_from("<3I", binary, 20)
    if one != 1 or extent != len(binary) or not 1 <= count <= 32 or 32 + 4 * count > len(binary):
        raise AssertionError("Original DXBC extent or chunk table")
    ranges, tags, found = [], set(), []
    for offset in struct.unpack_from("<" + "I" * count, binary, 32):
        if offset % 4 or offset < 32 + 4 * count or offset + 8 > len(binary):
            raise AssertionError("Original DXBC chunk offset")
        tag = binary[offset:offset + 4]
        length = struct.unpack_from("<I", binary, offset + 4)[0]
        end = offset + 8 + length
        if tag in tags or end > len(binary) or any(offset < right and left < end for left, right in ranges):
            raise AssertionError("Original DXBC chunk duplicate, extent or overlap")
        ranges.append((offset, end))
        tags.add(tag)
        if tag in (b"SHDR", b"SHEX"):
            code = binary[offset + 8:end]
            if length < 12 or length % 4:
                raise AssertionError("Original token extent")
            version, words = struct.unpack_from("<2I", code)
            expected = (stage << 16) | (0x40 if model == 40 else 0x41)
            if version != expected or 4 * words != len(code):
                raise AssertionError("Original SM4 stage/version/word count")
            found.append(code)
    if len(found) != 1:
        raise AssertionError("Exactly one original code chunk is required")
    return found[0]

def expected(geometry,sparse,alternate):
    words=[0xcccccccc]*64
    bits=lambda value:struct.unpack('<I',struct.pack('<f',value))[0]
    for vertex in range(3):
        values=[0x80000000,bits(vertex+0.5+8*alternate+(16 if geometry else 0)),
                bits(-0.25-vertex-8*alternate-(16 if geometry else 0)),
                bits(2+vertex+8*alternate+(32 if geometry else 0)),
                (0x7fc01234+8*alternate+vertex)^(0x01000000 if geometry else 0)]
        selected=[values[0],values[2],values[4]] if sparse else values
        words[vertex*8+1:vertex*8+1+len(selected)]=selected
    return words

def cases():
    for model in (40,41):
        for geometry in (0,1):
            for sparse in (0,1):
                for alternate in (0,1):
                    for role in ('native','public'):
                        yield model,geometry,sparse,alternate,role

def verify(directory):
    directory=Path(directory)
    if directory.is_symlink() or not directory.is_dir():raise AssertionError('Original directory')
    wanted=set();originals=[];checks=0;words=0;query_words=0
    def load(name):
        nonlocal checks
        path=directory/name
        if path.is_symlink() or not path.is_file():raise AssertionError('Missing/nonregular original: '+name)
        raw=path.read_bytes();wanted.add(name);checks+=1
        originals.append(dict(path=str(path.resolve()),bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
        return raw
    if hashlib.sha256(load('distance-original.hlsl')).hexdigest()!=SOURCE_SHA256:raise AssertionError('Original source')
    for prefix,stage,model in shader_cases():
        if shader_chunk(load(prefix+'.dxbc'),stage,model)!=load(prefix+'.tokens'):raise AssertionError('Original FXC/token join: '+prefix)
        checks+=1
    for model,geometry,sparse,alternate,role in cases():
        prefix=f'distance-model{model}-gs{geometry}-sparse{sparse}-alternate{alternate}-{role}'
        raw=load(prefix+'.bin')
        if len(raw)!=256:raise AssertionError('Original buffer extent: '+prefix)
        values=struct.unpack('<64I',raw)
        for index,(value,oracle) in enumerate(zip(values,expected(geometry,sparse,alternate))):
            words+=1;checks+=1
            if value!=oracle:raise AssertionError(f'{prefix} word{index}: {value:08x} != {oracle:08x}')
        raw=load(prefix+'-query.bin')
        if len(raw)!=48:raise AssertionError('Original query extent: '+prefix)
        counters=struct.unpack('<6Q',raw)
        # IA vertices/primitives, NULL PS, removed, written/needed primitives.
        if counters!=(3,1,0,0,1,1):raise AssertionError('Original query counters: '+prefix+': '+str(counters))
        checks+=6;query_words+=6
    if {p.name for p in directory.iterdir()}!=wanted:raise AssertionError('Exact original closure')
    if len(originals)!=77 or words!=2048 or query_words!=192:raise AssertionError('Original totals')
    return dict(passed=True,scope='original clip/cull SO register ownership, later ordinal, sparse masks, prior VS rebind and queries',
                checks=checks,scenes=16,original_buffers=32,query_frames=32,fxc_programs=6,original_file_count=77,
                observations=words,words_per_role=1024,query_words=query_words,original_files=originals,
                hardware_admission=False,registration=False,compiler_provenance_scope='caller must join actual compiler/process originals')

def selftest(root):
    controls=0
    with tempfile.TemporaryDirectory(prefix='distance-reader-',dir=root) as temporary:
        directory=Path(temporary)
        fixture=Path(__file__).with_name('umd-d3d10-distance-stream.cpp').read_text()
        hlsl=re.search(r'static constexpr char source\[\]=R"\((.*?)\)";',fixture,re.S).group(1).encode()
        (directory/'distance-original.hlsl').write_bytes(hlsl)
        for prefix,stage,model in shader_cases():
            code=struct.pack('<3I',(stage<<16)|(0x40 if model==40 else 0x41),3,62|(1<<24))
            binary=b'DXBC'+bytes(16)+struct.pack('<4I',1,44+len(code),1,36)+b'SHDR'+struct.pack('<I',len(code))+code
            (directory/(prefix+'.dxbc')).write_bytes(binary);(directory/(prefix+'.tokens')).write_bytes(code)
        for model,geometry,sparse,alternate,role in cases():
            prefix=f'distance-model{model}-gs{geometry}-sparse{sparse}-alternate{alternate}-{role}'
            (directory/(prefix+'.bin')).write_bytes(struct.pack('<64I',*expected(geometry,sparse,alternate)))
            (directory/(prefix+'-query.bin')).write_bytes(struct.pack('<6Q',3,1,0,0,1,1))
        verify(directory);controls+=1
        def reject(name,offset,value,width='I'):
            nonlocal controls
            path=directory/name;original=path.read_bytes();changed=bytearray(original);struct.pack_into('<'+width,changed,offset,value);path.write_bytes(changed)
            try:verify(directory)
            except AssertionError:controls+=1
            else:raise AssertionError('Mutation accepted: '+name+':'+str(offset))
            finally:path.write_bytes(original)
        prefix='distance-model40-gs0-sparse0-alternate0-native'
        for offset,value in ((0,0),(4,0),(8,0xbf800000),(12,0),(16,0),(20,0x7fc00000),(24,0),(7*4,0),(24*4,1)):
            reject(prefix+'.bin',offset,value)
        reject('distance-model41-gs1-sparse0-alternate1-public.bin',8,0x3f000000)
        reject('distance-model40-gs0-sparse1-alternate0-native.bin',12,0)
        for index,value in enumerate((0,2,1,0x887a0005,0,2)):reject(prefix+'-query.bin',index*8,value,'Q')
        original='distance-fxc-01-vs-vs_4_0'
        reject(original+'.tokens',8,58|(1<<24))
        for offset,value in ((20,0),(24,60),(32,35),(40,16),(44,0x10050),(44,0x40),(48,4)):
            reject(original+'.dxbc',offset,value)
        for name,action in ((prefix+'.bin','truncate'),(prefix+'-query.bin','truncate'),('foreign.bin','extra'),(prefix+'.bin','missing'),(prefix+'.bin','symlink')):
            path=directory/name;raw=path.read_bytes() if path.exists() else None
            if action=='truncate':path.write_bytes(raw[:-4])
            elif action=='extra':path.write_bytes(b'extra')
            else:
                path.unlink()
                if action=='symlink':path.symlink_to(directory/'distance-model40-gs0-sparse0-alternate0-public.bin')
            try:verify(directory)
            except AssertionError:controls+=1
            else:raise AssertionError('Closure mutation accepted: '+action)
            finally:
                if path.is_symlink() or path.exists():path.unlink()
                if raw is not None:path.write_bytes(raw)
        path=directory/'distance-original.hlsl';raw=path.read_bytes();path.write_bytes(raw+b'\n')
        try:verify(directory)
        except AssertionError:controls+=1
        else:raise AssertionError('Changed source accepted')
        finally:path.write_bytes(raw)
        verify(directory);controls+=1
    return dict(passed=True,scope='synthetic reader controls',controls=controls,original_file_count=77,observations=2048,query_words=192,
                native_execution=False,compiler_execution=False,hardware_admission=False,registration=False)

def main():
    parser=argparse.ArgumentParser(description=__doc__);mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--directory',type=Path);mode.add_argument('--selftest-root',type=Path);parser.add_argument('--output',type=Path)
    args=parser.parse_args();result=verify(args.directory) if args.directory else selftest(args.selftest_root)
    if args.output:
        with args.output.open('x') as output:json.dump(result,output,indent=2)
    print(json.dumps({k:v for k,v in result.items() if k!='original_files'}))
if __name__=='__main__':main()
