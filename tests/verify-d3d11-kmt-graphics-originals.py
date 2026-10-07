#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Recompute raw SO/Texture3D bytes; process, token and input joins are separate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def require(value, message):
    if not value:
        raise ValueError(message)


def u32(data):
    require(len(data) % 4 == 0, 'unaligned original u32 data')
    return struct.unpack('<' + 'I' * (len(data) // 4), data)


def packed(values):
    return struct.pack('<' + 'I' * len(values), *values)


def signature(data, tag):
    require(len(data) >= 8, 'short signature header')
    count, first = struct.unpack_from('<II', data)
    stream_field = tag in (b'ISG1', b'OSG1', b'OSG5')
    precision_field = tag in (b'ISG1', b'OSG1')
    stride = 24 + 4 * stream_field + 4 * precision_field
    require(first == 8 and count <= (len(data) - 8) // stride, 'signature entry range')
    entries = []
    for index in range(count):
        offset = 8 + index * stride
        stream = struct.unpack_from('<I', data, offset)[0] if stream_field else 0
        offset += 4 * stream_field
        name, semantic, system, component, register, masks = struct.unpack_from('<6I', data, offset)
        require(8 + count * stride <= name < len(data), 'signature name range')
        end = data.find(b'\0', name)
        require(end > name and end - name < 128, 'signature semantic name')
        mask = masks & 0xff
        require(stream < 4 and register < 32 and 0 < mask < 16 and component in (1, 2, 3), 'signature fields')
        if precision_field:
            require(struct.unpack_from('<I', data, offset + 24)[0] == 0, 'unexpected minimum precision')
        entries.append((stream, register, mask, system, component, semantic, data[name:end]))
    return entries


def dxbc(data, stage):
    require(len(data) >= 32 and data[:4] == b'DXBC', 'original DXBC header')
    one, length, count = struct.unpack_from('<III', data, 20)
    require(one == 1 and length == len(data) and 0 < count <= (length - 32) // 4, 'original DXBC size')
    chunks, intervals = {}, []
    for offset in struct.unpack_from('<' + 'I' * count, data, 32):
        require(offset % 4 == 0 and 32 + count * 4 <= offset <= length - 8, 'DXBC chunk offset')
        tag = data[offset:offset + 4]
        size = struct.unpack_from('<I', data, offset + 4)[0]
        require(size <= length - offset - 8 and tag not in chunks, 'DXBC chunk range/duplicate')
        require(all(offset + 8 + size <= start or offset >= end for start, end in intervals), 'overlapping DXBC chunks')
        intervals.append((offset, offset + 8 + size))
        chunks[tag] = data[offset + 8:offset + 8 + size]
    require(b'SHEX' in chunks, 'missing original SM5 SHEX')
    tokens = u32(chunks[b'SHEX'])
    require(len(tokens) >= 2 and tokens[0] == (stage << 16) | 0x50 and tokens[1] == len(tokens), 'SHEX stage/length')
    def entries(tags):
        present = [tag for tag in tags if tag in chunks]
        require(len(present) == 1, 'signature chunk count')
        return signature(chunks[present[0]], present[0])
    return chunks[b'SHEX'], entries((b'ISGN', b'ISG1')), entries((b'OSGN', b'OSG1', b'OSG5'))


class Originals:
    def __init__(self, root):
        self.root, self.seen, self.rows = root, set(), []

    def read(self, name):
        require(name not in self.seen, 'duplicate original read: ' + name)
        path = self.root / name
        require(path.is_file() and not path.is_symlink(), 'missing original: ' + name)
        data = path.read_bytes()
        self.seen.add(name)
        self.rows.append(dict(member=name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
        return data

    def exact(self, name, expected):
        require(self.read(name) == expected, 'original byte mismatch: ' + name)

    def image(self, name, expected):
        raw = packed(expected)
        self.exact(name + '.actual.u32', raw)
        self.exact(name + '.expected.u32', raw)

    def declaration(self, name, expected):
        raw = self.read(name)
        # Original26100 ABI: three UINTs, one BYTE mask and three trailing
        # padding bytes. Retain all bytes; compare every meaningful field.
        require(len(raw) == 16 * len(expected), 'SO declaration ABI length')
        fields = [struct.unpack_from('<IIIB', raw, 16 * index) for index in range(len(expected))]
        require(fields == expected, 'SO declaration typed fields')
        self.rows[-1]['typed_fields'] = fields
        self.rows[-1]['retained_abi_padding_hex'] = [raw[16 * index + 13:16 * index + 16].hex() for index in range(len(expected))]

    def shader(self, name, source, stage):
        self.exact(name + '.hlsl', source)
        shex, inputs, outputs = dxbc(self.read(name + '.dxbc'), stage)
        self.exact(name + '.shex', shex)
        for output, entries in enumerate((inputs, outputs)):
            for index, (stream, register, mask, system, component, semantic, text) in enumerate(entries):
                stem = name + ('.output-' if output else '.input-') + str(index)
                self.exact(stem + '.signature-u32', packed((output, index, stream, register, mask, system, component, semantic)))
                self.exact(stem + '.semantic', text)
        diagnostic = self.root / (name + '.compiler.txt')
        if diagnostic.exists():
            self.exact(diagnostic.name, b'')
        return outputs

    def complete(self):
        require({p.name for p in self.root.iterdir() if p.is_file()} == self.seen
          and all(p.is_file() for p in self.root.iterdir()), 'unjoined original files')


def hlsl(source, variable):
    found = re.findall(r'constexpr char ' + re.escape(variable) + r'\[\] = R"\((.*?)\)";', source, re.S)
    require(len(found) == 1, 'original HLSL source definition')
    return found[0].encode('ascii')


def callbacks(text, profile):
    pending, accepted, stages = None, 0, []
    for line in text.splitlines():
        raw = re.fullmatch(r'DX11_CORE_ERROR hr=([0-9a-f]{8})', line)
        admitted = re.fullmatch(r'D3D11_KMT_EXPECTED_CORE_ERROR stage=(\S+) hr=([0-9a-f]{8}) total=(\d+) delta=1', line)
        if raw:
            require(pending is None, 'multiple/overwritten core callbacks')
            pending = raw[1]
        elif admitted:
            stage, result, total = admitted.groups()
            require(pending == result and int(total) == accepted + 1, 'callback exact HRESULT/count/event order')
            if stage == 'named-query-pending':
                require(profile == 'so' and result == '887a000a', 'named query callback')
            else:
                require(result == '80070057', 'named negative callback')
            stages.append(stage); accepted += 1; pending = None
    require(pending is None, 'unclassified trailing callback')
    negatives = [stage for stage in stages if stage != 'named-query-pending']
    expected = ['named-invalid-SO-declaration'] * 8 if profile == 'so' else [
      'volume-copy-destination-depth', 'volume-upload-row-too-short', 'volume-upload-slice-overlap', 'volume-dynamic-invalid-mip']
    require(negatives == expected, 'named negative callback order/count')
    lines = re.findall(r'^DX11_KMT_COUNTS queries=(\d+) contexts=(\d+)/(\d+) allocations=(\d+)/(\d+) locks=(\d+)/(\d+) renders=(\d+) escapes=(\d+) residency=(\d+)/(\d+) wrong_threads=(\d+) bad_cookies=(\d+) core_errors=(\d+) malformed_outputs=(\d+) remaining_allocations=(\d+) remaining_residents=(\d+) pending_paging=(\d+)$', text, re.M)
    require(len(lines) == 1, 'actual callback counts')
    values = list(map(int, lines[0]))
    require(values[0] > 0 and values[1:3] == [1, 1] and values[3] == values[4] > 0
      and values[5] == values[6] > 0 and values[7] > 0 and values[8] >= 2
      and values[9] == values[10] > 0 and values[11:13] == [0, 0]
      and values[13] == accepted and not any(values[14:]), 'kernel callback ownership/error balance')
    return accepted, dict(zip(('queries', 'contexts', 'context_closes', 'allocations', 'deallocations', 'locks',
      'unlocks', 'renders', 'escapes', 'residents', 'evictions', 'wrong_threads', 'bad_cookies', 'core_errors',
      'malformed_outputs', 'remaining_allocations', 'remaining_residents', 'pending_paging'), values))


def expected_so(stream, first):
    stride = (8, 4, 6, 4)[stream]
    words = []
    for index in range(64):
        record, component = divmod(index, stride)
        words.append(first + record // (stream + 1) + 100 * stream if record < 3 * (stream + 1) and component == 0
          else 900 + stream + record % (stream + 1) if record < 3 * (stream + 1) and component == 1 else 0xa5a55a5a)
    return words


def verify_so(originals, text, source):
    originals.shader('four-stream-vs', hlsl(source, 'VertexSource'), 1)
    outputs = originals.shader('four-stream-gs', hlsl(source, 'GeometrySource'), 2)
    def register(entries, semantic, stream=0):
        values = [entry[1] for entry in entries if entry[0] == stream and entry[-1] == semantic and entry[5] == 0]
        require(len(values) == 1, 'original SO semantic register')
        return values[0]
    registers = [register(outputs, b'DATA', stream) for stream in range(4)]
    declaration = [(stream, stream, registers[stream], 3) for stream in range(4)] + [(0, 0, 0xffffffff, 1)]
    originals.declaration('four-stream-DDI-declaration.raw', declaration)
    originals.exact('four-stream-strides.u32', packed((32, 16, 24, 16)))
    captures = {}
    for draw in range(2):
        for stream in range(4):
            name = f'draw-{draw}-stream-{stream}'
            captures[name] = (stream, expected_so(stream, 17 * draw))
            originals.image(name, captures[name][1])
            originals.exact(name + '.query-u64', struct.pack('<3Q', 3 * (stream + 1), 3 * (stream + 1), 0))
        originals.exact(f'draw-{draw}-aggregate-overflow.u32', packed((0,)))
    for stream in range(4):
        originals.exact(f'overflow-stream-{stream}.query-u64', struct.pack('<3Q',
          1 if stream == 2 else 3 * (stream + 1), 3 * (stream + 1), int(stream == 2)))
    originals.exact('overflow-aggregate-overflow.u32', packed((1,)))
    for test in range(8):
        fields = [test, 0, 0, registers[0], 3, 0xffffffff, 16, 0x80070057]
        slot, value = {0: (4, 0), 1: (2, 4), 2: (1, 4), 3: (3, 32), 4: (4, 8), 5: (5, 4), 6: (6, 13), 7: (6, 4)}[test]
        fields[slot] = value
        originals.exact(f'invalid-SO-{test}.fields-u32', packed(fields))
    outputs_a = originals.shader('null-GS-vs-A', hlsl(source, 'NullVertexA'), 1)
    outputs_b = originals.shader('null-GS-vs-B', hlsl(source, 'NullVertexB'), 1)
    require([(entry[:6], entry[-1]) for entry in outputs_a] == [(entry[:6], entry[-1]) for entry in outputs_b], 'NULL GS compatible signatures')
    originals.declaration('null-GS-DDI-declaration.raw', [(0, 0, register(outputs_a, b'SV_Position'), 15),
      (0, 0, register(outputs_a, b'DATA'), 15)])
    for draw in range(3):
        alternate = draw == 1
        payload = (0xffc04321, 0xffffffff, 0x11223344) if alternate else (0x7fc01234, 0x80000000, 0x87654321)
        expected = sum(([0, 0, 0, 0x3f800000, *payload, vertex + 100 * alternate] for vertex in range(3)), []) + [0xa5a55a5a] * 40
        captures[f'null-GS-draw-{draw}'] = (0, expected)
        originals.image(f'null-GS-draw-{draw}', expected)
    recorded = set()
    for name, stream, index, actual, expected in re.findall(r'^D3D11_KMT_SO_WORD case=(\S+) stream=(\d+) word=(\d+) actual=([0-9a-f]{8}) expected=([0-9a-f]{8})$', text, re.M):
        key = name, int(index)
        require(name in captures and int(stream) == captures[name][0] and 0 <= int(index) < 64
          and key not in recorded and int(actual, 16) == int(expected, 16) == captures[name][1][int(index)], 'raw SO stdout word join')
        recorded.add(key)
    require(len(recorded) == 704, 'full SO word trace count')
    return dict(words=704, draws=6, streams=4, negatives=8)


def expected_volume(index, mip, x, y, z):
    color = 0xff000000 + mip * 0x100000 + z * 0x1000 + y * 64 + x
    if index == 1 and mip == 0:
        if 5 <= x < 8 and 2 <= y < 4 and 4 <= z < 6:
            return 0xff000000 + (z - 3) * 0x1000 + (y - 1) * 64 + x - 4
        if 2 <= x < 5 and 1 <= y < 3 and 2 <= z < 5:
            return 0xff000000 + (z - 2) * 0x1000 + (y - 1) * 64 + x - 2
    if 2 <= index <= 4:
        return color ^ ((index - 2) * 0x10101)
    if index == 5 and mip == 1 and z >= 1:
        return 0xff0000ff if z == 1 else 0xff00ff00
    if index >= 6 and (mip == 1 or index == 6 and mip >= 2 or index == 7 and mip == 2):
        return 0xff00ffff
    return color


def verify_volume(originals, text, source):
    originals.shader('volume-load-cs', hlsl(source, 'SampleSource'), 5)
    shapes = [(9, 5, 7, 4), (9, 5, 7, 4), (7, 3, 5, 1), (7, 3, 5, 1), (7, 3, 5, 1),
      (8, 4, 8, 3), (8, 8, 8, 4), (8, 8, 8, 4), (8, 8, 8, 4)]
    words, images, total = {}, {}, 0
    for index, (width, height, depth, levels) in enumerate(shapes):
        for mip in range(levels):
            shape = tuple(max(1, value >> mip) for value in (width, height, depth))
            expected = [expected_volume(index, mip, x, y, z) for z in range(shape[2]) for y in range(shape[1]) for x in range(shape[0])]
            name = f'volume-readback-{index}-mip-{mip}'
            originals.image(name, expected)
            fields = u32(originals.read(name + '.dimensions-pitches.u32'))
            require(len(fields) == 7 and fields[:5] == (index, mip, *shape) and fields[5] >= shape[0] * 4
              and (shape[2] == 1 or fields[6] >= (shape[1] - 1) * fields[5] + shape[0] * 4), 'original volume dimensions/pitches')
            words[index, mip] = expected
            images[index, mip] = shape, len(expected), fields[5], fields[6]
            total += len(expected)
    sampled = 0
    sample_cases = [(0, mip, mip) for mip in range(4)] + [(index, mip, mip - 1)
      for index, count in ((6, 3), (7, 2), (8, 1)) for mip in range(1, count + 1)]
    for sample, (index, mip, relative) in enumerate(sample_cases):
        originals.image(f'volume-sampled-{sample}', words[index, mip])
        originals.exact(f'volume-sampled-{sample}.shape-relative-mip.u32', packed((*images[index, mip][0], relative)))
        sampled += len(words[index, mip])
    originals.exact('invalid-dynamic-map-output.u64', struct.pack('<4Q', 0, 0, 0, 1))
    lines = re.findall(r'^D3D11_KMT_VOLUME_READBACK index=(\d+) mip=(\d+) xyz=(\d+),(\d+),(\d+) words=(\d+) row_pitch=(\d+) depth_pitch=(\d+) mismatches=0$', text, re.M)
    require(len(lines) == 26, 'volume raw readback log count')
    observed = set()
    for line in lines:
        index, mip, x, y, z, count, row, depth = map(int, line)
        require((index, mip) not in observed and images.get((index, mip)) == ((x, y, z), count, row, depth), 'volume raw log metadata join')
        observed.add((index, mip))
    samples = re.findall(r'^D3D11_KMT_VOLUME_SAMPLED index=(\d+) words=(\d+) relative_mip=(\d+) mismatches=0$', text, re.M)
    require(len(samples) == 10 and [(int(a), int(b), int(c)) for a, b, c in samples] == [
      (sample, len(words[index, mip]), relative) for sample, (index, mip, relative) in enumerate(sample_cases)], 'volume shader sampling log join')
    require(total == 3046 and sampled == 551, 'full volume oracle counts')
    return dict(voxels=total, sampled=sampled, cases=9, readbacks=9, negatives=4)


def verify(profile, originals_root, stdout, source_root, luid):
    require(re.fullmatch('[0-9a-f]{16}', luid) and int(luid, 16), 'expected LUID bytes')
    text = stdout.decode('ascii')
    upper = profile.upper()
    require(text.count(f'D3D11_KMT_{upper}_SELECTED luid={luid}\n') == 1, 'exact selected LUID')
    require(re.findall(r'^DX11_KMT_SELECTED generation=(\d+) capabilities=(\d+) version=(\d+) software=0$', text, re.M)
      == [('2', '0', '2200')], 'actual render/nonsoftware identity')
    require(re.findall(r'^D3D11_KMT_GRAPHICS_DEVICE interface=([0-9a-f]+) version=([0-9a-f]+) flags=([0-9a-f]+)$', text, re.M)
      == [('000b000a', '00020009', '00000004')], 'exact official typed11/DXGI1.1 ABI')
    require(text.count(f'D3D11_KMT_{upper}_RAW_CLOSE hr=00000000\n') == 1 and f'D3D11_KMT_{upper}_FAIL' not in text, 'original raw close/no failure')
    accepted, counts = callbacks(text, profile)
    originals = Originals(originals_root)
    source = (source_root / 'tests' / f'umd-d3d11-{profile}-probe.cpp').read_text()
    result = verify_so(originals, text, source) if profile == 'so' else verify_volume(originals, text, source)
    final = f'D3D11_KMT_SO_PASS streams=4 draws=6 words=704 negatives=8 expected_core_errors={accepted} guards=1 balanced=1 hr=00000000 ordinary_runtime_admission=0\n' if profile == 'so' else (
      f'D3D11_KMT_VOLUME_PASS cases=9 readbacks=9 voxels=3046 sampled=551 negatives=4 expected_core_errors={accepted} balanced=1 hr=00000000 ordinary_runtime_admission=0\n')
    require(text.count(final) == 1, 'complete native marker')
    originals.complete()
    return dict(schema='typed11-kmt-graphics-raw-originals-v1', passed=True, profile=profile,
      expected_luid=luid, oracle=result, actual_callbacks=counts, expected_core_errors=accepted,
      stdout_sha256=hashlib.sha256(stdout).hexdigest(), originals=originals.rows,
      process_token_and_input_provenance_verified=False, ordinary_runtime_admission=False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('profile', choices=('so', 'volume'))
    parser.add_argument('--originals', required=True, type=Path)
    parser.add_argument('--stdout', required=True, type=Path)
    parser.add_argument('--source-root', required=True, type=Path)
    parser.add_argument('--luid', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    require(not args.output.exists(), 'fresh offline output')
    result = verify(args.profile, args.originals, args.stdout.read_bytes(), args.source_root, args.luid)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(passed=True, profile=args.profile, oracle=result['oracle'], files=len(result['originals']))))


if __name__ == '__main__':
    main()
