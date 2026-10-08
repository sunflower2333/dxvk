#!/usr/bin/env python3
"""Original-input and path-anchoring controls; no module or target execution."""
from pathlib import Path
import argparse
import importlib.util
import json
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--original', type=Path, required=True)
parser.add_argument('--scratch', type=Path, required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parents[1] / 'scripts/derive-native-d3d8-owned-icd.py'
spec = importlib.util.spec_from_file_location('derive_icd', source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
original = args.original.read_bytes()
derived = module.derive(original)
assert len(derived) == 148 and module.sha(derived) == module.DERIVED_SHA256
before, after = json.loads(original), json.loads(derived)
assert after['ICD']['library_path'] == '.\\viogpu_gl_vk_x86.dll'
after['ICD']['library_path'] = before['ICD']['library_path']
assert after == before
assert derived.replace(module.NEW, module.OLD, 1) == original
rejected = 0
for invalid in [original + b'\n', original[:-1], original.replace(b'1.4.354', b'1.4.353'),
                original.replace(b'viogpu_gl_vk_x86', b'viogpu_gl_vk_x64'),
                derived, original.replace(module.OLD, module.NEW + module.OLD)]:
    try:
        module.derive(invalid)
    except ValueError:
        rejected += 1
    else:
        raise AssertionError('Nonoriginal configuration admitted')
args.scratch.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='icd-controls-', dir=args.scratch) as directory:
    output = Path(directory) / 'freedreno_icd_owned_x86.json'
    proof = module.prepare(args.original, output)
    assert output.read_bytes() == derived and args.original.read_bytes() == original
    assert proof['target_actions'] == proof['module_loads'] == proof['Vulkan_instances'] == 0
    try:
        module.prepare(args.original, output)
    except ValueError:
        rejected += 1
    else:
        raise AssertionError('Existing derivative overwritten')
    assert output.read_bytes() == derived
print(json.dumps({'passed': True, 'rejections': rejected, 'original_unchanged': True,
                  'derived_bytes': len(derived), 'derived_sha256': module.sha(derived),
                  'target_actions': 0, 'module_loads': 0}, indent=2))
