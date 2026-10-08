#!/usr/bin/env python3
"""Read actual Microsoft AR/COFF provider bytes; never execute a target or tool."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib


def require(value, message):
    if not value:
        raise ValueError(message)


def pin(path):
    raw = Path(path).read_bytes()
    return dict(path=str(Path(path).resolve()), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def archive_members(raw):
    require(raw[:8] == b'!<arch>\n', 'actual MS COFF archive signature')
    pos, long_names, result = 8, b'', []
    while pos < len(raw):
        require(pos + 60 <= len(raw), 'bounded archive header')
        header = raw[pos:pos+60]
        require(header[58:60] == b'`\n', 'actual archive header terminator')
        count = int(header[48:58].decode('ascii').strip())
        require(count >= 0 and count <= len(raw)-pos-60, 'bounded archive member')
        name = header[:16].decode('ascii').strip()
        payload = raw[pos+60:pos+60+count]
        if name == '//':
            long_names = payload
        elif name.startswith('/') and name[1:].isdigit():
            index = int(name[1:])
            require(index < len(long_names), 'bounded archive long name')
            ends = [end for end in (long_names.find(b'\0', index), long_names.find(b'/\n', index)) if end >= index]
            require(bool(ends), 'terminated original archive long name')
            result.append((long_names[index:min(ends)].decode('ascii'), payload))
        elif name != '/':
            result.append((name.rstrip('/'), payload))
        pos += 60 + count + (count & 1)
    require(pos == len(raw), 'complete archive extent')
    return result


def coff_symbols(raw):
    require(len(raw) >= 20, 'bounded original COFF header')
    machine, sections, _, offset, count, optional, _ = struct.unpack_from('<HHIIIHH', raw)
    require(machine in (0xaa64, 0xa641, 0x8664) and optional == 0 and
            offset >= 20+sections*40 and count <= (len(raw)-offset)//18,
            'ordinary actual COFF machine/section/symbol extent')
    strings = offset + count*18
    require(strings+4 <= len(raw), 'actual COFF string table header')
    string_bytes = struct.unpack_from('<I', raw, strings)[0]
    require(string_bytes >= 4 and string_bytes <= len(raw)-strings, 'bounded actual COFF string table')
    symbols, index = {}, 0
    while index < count:
        name, value, section, kind, storage, auxiliary = struct.unpack_from('<8sIhHBB', raw, offset+index*18)
        if name[:4] == b'\0'*4:
            cursor = struct.unpack_from('<I', name, 4)[0]
            require(4 <= cursor < string_bytes, 'bounded original symbol long name')
            end = raw.find(b'\0', strings+cursor, strings+string_bytes)
            require(end >= 0, 'terminated original symbol name')
            name = raw[strings+cursor:end]
        else:
            name = name.rstrip(b'\0')
        require(index+auxiliary < count, 'bounded actual auxiliary symbols')
        row = dict(name=name.decode('ascii'), index=index, value=value, section=section, kind=kind, storage=storage)
        if storage == 0x69:
            require(auxiliary == 1, 'one original weak external auxiliary')
            row['weak_target'], row['weak_search'] = struct.unpack_from('<II', raw, offset+(index+1)*18)
        symbols[index] = row
        index += auxiliary+1
    return machine, symbols


def provider(raw):
    members = archive_members(raw)
    candidates = []
    for name, data in members:
        if name.lower().endswith('widemath.obj'):
            machine, symbols = coff_symbols(data)
            if machine == 0xa641:
                candidates.append((name, data, symbols))
    require(len(candidates) == 1, 'one genuine ARM64EC widemath member')
    name, data, symbols = candidates[0]
    joined = []
    for alias in ('_mm_getcsr', '_mm_setcsr'):
        definition = [row for row in symbols.values() if row['name'] == '#'+alias]
        weak = [row for row in symbols.values() if row['name'] == alias]
        require(len(definition) == len(weak) == 1, 'unique actual CSR definition and alias')
        definition, weak = definition[0], weak[0]
        require(definition['section'] > 0 and definition['storage'] == 2 and definition['kind'] == 0x20,
                'actual external CSR function definition')
        require(weak['storage'] == 0x69 and weak['section'] == 0 and weak['weak_search'] == 4 and
                weak['weak_target'] == definition['index'], 'actual CSR AntiDependency alias targets definition')
        joined.append(dict(definition=definition, alias=weak))
    return dict(member=name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), machine=0xa641, csr_symbols=joined)


def zip_tail(raw, headers):
    ranges = re.findall(r'(?im)^Content-Range: bytes (\d+)-(\d+)/(\d+)\s*$', headers)
    require(len(ranges) == 1 and '206 Partial Content' in headers, 'one actual HTTP206 byte range')
    start, end, total = map(int, ranges[0])
    require(end-start+1 == len(raw) and end == total-1, 'exact original final ZIP byte range')
    pos = raw.rfind(b'PK\x05\x06')
    require(pos >= 0 and pos+22 == len(raw), 'original bounded comment-free ZIP EOCD')
    _, disk, central_disk, disk_count, count, size, offset, comment = struct.unpack_from('<4s4H2IH', raw, pos)
    require(disk == central_disk == comment == 0 and count == disk_count and offset >= start and
            offset+size == start+pos, 'one contained complete original ZIP central directory')
    cursor, rows = offset-start, []
    while cursor < pos:
        values = struct.unpack_from('<4s6H3I5H2I', raw, cursor)
        require(values[0] == b'PK\x01\x02', 'original central member signature')
        name_len, extra_len, comment_len = values[10:13]
        require(cursor+46+name_len+extra_len+comment_len <= pos, 'bounded original central member')
        name = raw[cursor+46:cursor+46+name_len].decode('utf-8')
        rows.append(dict(name=name, method=values[4], crc32=values[7], compressed=values[8], bytes=values[9], local_offset=values[16]))
        cursor += 46+name_len+extra_len+comment_len
    require(cursor == pos and len(rows) == count, 'all actual ZIP central entries')
    return rows


def range_member(raw, headers, entry, tail_headers):
    ranges = re.findall(r'(?im)^Content-Range: bytes (\d+)-(\d+)/(\d+)\s*$', headers)
    require(len(ranges) == 1 and '206 Partial Content' in headers, 'one actual member HTTP206 byte range')
    start, end, _ = map(int, ranges[0])
    etag = re.findall(r'(?im)^ETag:\s*(\S+)\s*$', headers)
    require(len(etag) == 1 and etag == re.findall(r'(?im)^ETag:\s*(\S+)\s*$', tail_headers), 'same original archive ETag')
    require(start == entry['local_offset'] and end-start+1 == len(raw), 'same indexed actual member range')
    values = struct.unpack_from('<4s5H3I2H', raw)
    name_len, extra_len = values[9:11]
    require(values[0] == b'PK\x03\x04' and values[3] == entry['method'] == 8 and
            raw[30:30+name_len].decode() == entry['name'], 'original local member header/name/method')
    require(values[6:9] == (entry['crc32'], entry['compressed'], entry['bytes']), 'local and central member CRC/size')
    offset = 30+name_len+extra_len
    require(offset+entry['compressed'] == len(raw), 'complete exact original compressed member extent')
    decoded = zlib.decompress(raw[offset:], -15)
    require(len(decoded) == entry['bytes'] and zlib.crc32(decoded) == entry['crc32'], 'actual uncompressed member size and CRC')
    return decoded


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--comparison-root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    require(not a.output.exists(), 'fresh comparison proof')
    results, originals, http_children = {}, [], []
    for arch in ('arm64', 'x64'):
        tail = a.comparison_root/(arch+'.zip-tail.raw')
        tail_headers = a.comparison_root/(arch+'.zip-tail.headers.raw')
        member = a.comparison_root/(arch+'.softintrin-member-complete.raw')
        member_headers = a.comparison_root/(arch+'.softintrin-member-complete.headers.raw')
        rows = zip_tail(tail.read_bytes(), tail_headers.read_text())
        entry = [row for row in rows if row['name'] == 'c/um/'+arch+'/softintrin.lib']
        require(len(entry) == 1, 'one original SDK soft-intrinsic entry')
        decoded = range_member(member.read_bytes(), member_headers.read_text(), entry[0], tail_headers.read_text())
        library = a.comparison_root/('sdk28000-'+arch+'-softintrin.lib')
        require(decoded == library.read_bytes(), 'genuine library derives from original ZIP range')
        results[arch] = dict(entry=entry[0], library=pin(library))
        originals += [pin(path) for path in (tail, tail_headers, member, member_headers, library)]
        package = 'microsoft.windows.sdk.cpp.'+arch
        url = 'https://api.nuget.org/v3-flatcontainer/'+package+'/10.0.28000.2526/'+package+'.10.0.28000.2526.nupkg'
        for name in (('zip-tail' if arch == 'arm64' else 'x64.zip-tail'), arch+'.softintrin-member-complete'):
            process = a.comparison_root/(name+'.process-original.json')
            row = json.loads(process.read_text())
            require(row['exit_code'] == 0 and row['reaped'] and row['argv'][-1] == url and
                    row['argv'][:3] == ['curl','--fail','--location'], 'actual closed official SDK range child/URL')
            if name == 'zip-tail':
                require(row['pid'] > 0 and row['stdout_file_closed'] and row['stderr_file_closed'], 'actual first range drained')
                outputs = [row[key] for key in ('stdout','stderr','headers','zip_tail')]
            else:
                require(row['child_pid'] > 0 and row['handles_closed'], 'actual subsequent range drained')
                outputs = [dict(value, path=str((a.comparison_root/value['path']).resolve())) for value in row['files']]
            for output in outputs:
                require(pin(output['path']) == output, 'same actual range child output')
            originals += outputs + [pin(process)]
            http_children.append(row)
    results['arm64']['provider'] = provider(Path(results['arm64']['library']['path']).read_bytes())
    x64_members = archive_members(Path(results['x64']['library']['path']).read_bytes())
    x64_widemath = [(name, coff_symbols(data)) for name, data in x64_members if name.lower().endswith('widemath.obj')]
    require(len(x64_widemath) == 1 and x64_widemath[0][1][0] == 0x8664 and
            not any(row['name'] in ('_mm_getcsr','#_mm_getcsr','_mm_setcsr','#_mm_setcsr')
                    for row in x64_widemath[0][1][1].values()), 'actual x64 widemath has no CSR provider')
    rejected = []
    for name, raw in [('x64-archive', Path(results['x64']['library']['path']).read_bytes()),
                      ('truncated-arm64', Path(results['arm64']['library']['path']).read_bytes()[:-1])]:
        try:
            provider(raw)
        except (ValueError, struct.error, UnicodeError):
            rejected.append(name)
        else:
            raise ValueError('altered provider accepted: '+name)
    processes = json.loads((a.comparison_root/'symbol-process-originals.json').read_text())
    for row in processes['processes']:
        require(row['exit_code'] == 0 and row['reaped'] and row['handles_closed'] and row['child_pid'] > 0,
                'genuine successful closed original symbol child')
        for output in row['outputs']:
            require(pin(output['path']) == output, 'same actual symbol child output')
        originals += row['outputs']
    a.output.write_text(json.dumps(dict(schema='legacy-arm64ec-intrinsic-provider-originals-v1', passed=True,
        target_library_attested=False, native_build_passed=False, whole_nupkg_attested=False,
        package_version='10.0.28000.2526', packages=['Microsoft.Windows.SDK.CPP.arm64','Microsoft.Windows.SDK.CPP.x64'],
        results=results, originals=originals, closed_symbol_children=len(processes['processes']), closed_HTTP_children=len(http_children),
        rejected_controls=rejected, limit='Comparison ZIP ranges/COFF originals prove this provider only. CPU07 must capture mounted library originals.'),indent=2)+'\n')
    print(json.dumps(dict(passed=True, output=pin(a.output), closed_symbol_children=len(processes['processes']), rejected_controls=rejected)))


if __name__ == '__main__':
    main()
