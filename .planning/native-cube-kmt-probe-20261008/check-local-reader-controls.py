#!/usr/bin/env python3
"""Test only synthetic C++ arithmetic exports; no shader/runtime acceptance."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

WS = Path('/home/sunf/droidvm-repos')
REPO = WS / 'reference/codes/dxvk-umd-cube-kmt-probe-20261008'
BASE = WS / 'artifacts/native-cube-kmt-probe-20261008/local-reader-controls-01'
ROOT = BASE / 'positive'
path = REPO / 'tests/verify-d3d11-kmt-cube-originals.py'
spec = importlib.util.spec_from_file_location('cube_originals', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def verify():
    originals = module.common.Originals(ROOT)
    result = module.cube_words(originals)
    originals.complete()
    assert len(originals.seen) == 753
    return result


positive = verify()
assert positive['whole_resource_words'] == 4830 and positive['sample_buffer_words'] == 1088
cases = [
    ('actual-last-face-word', ['cube-readback-0-face-11-mip-0.actual.u32'], -4),
    ('saved-expected-word', ['cube-readback-0-face-0-mip-0.expected.u32'], 0),
    ('untouched-cube-contamination', ['cube-readback-2-face-0-mip-3.actual.u32', 'cube-readback-2-face-0-mip-3.expected.u32'], 0),
    ('sample-output-tail', ['cube-sampled-0.actual.u32'], 63 * 4),
    ('relative-mip-metadata', ['cube-sampled-0.shape-relative-mip.u32'], 7 * 4),
    ('mapped-row-too-short', ['cube-readback-0-face-0-mip-0.shape-pitches.u32'], 7 * 4),
]
records = []
for name, members, offset in cases:
    originals = {member: (ROOT / member).read_bytes() for member in members}
    mutations = {}
    try:
        for member, original in originals.items():
            raw = bytearray(original)
            position = len(raw) + offset if offset < 0 else offset
            value = struct.unpack_from('<I', raw, position)[0]
            struct.pack_into('<I', raw, position, 0 if name == 'mapped-row-too-short' else value ^ 1)
            (ROOT / member).write_bytes(raw)
            mutations[member] = dict(bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())
        try:
            verify()
        except ValueError as error:
            records.append(dict(name=name, rejected=True, reason=str(error), mutations=mutations))
        else:
            raise AssertionError('independent arithmetic accepted ' + name)
    finally:
        for member, original in originals.items():
            (ROOT / member).write_bytes(original)
extra = ROOT / 'unjoined-control-file'
try:
    extra.write_bytes(b'control-only')
    try:
        verify()
    except ValueError as error:
        records.append(dict(name='extra-original', rejected=True, reason=str(error)))
    else:
        raise AssertionError('unjoined original accepted')
finally:
    extra.unlink()
assert verify() == positive
proof = dict(verified=True, evidence_kind='synthetic arithmetic reader controls only',
             shader_verification_executed=False, positive_control=positive, negative_controls=records,
             reader_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
             native_execution=False, hardware_acceptance=False, target_calls=False)
with (BASE / 'verified.json').open('x') as output:
    json.dump(proof, output, indent=2)
    output.write('\n')
print(json.dumps(dict(verified=True, synthetic_positive=1, synthetic_negatives=len(records),
                     words=4830, sample_words=1088, native_execution=False)))
