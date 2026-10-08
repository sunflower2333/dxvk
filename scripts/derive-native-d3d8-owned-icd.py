#!/usr/bin/env python3
"""Derive a separately owned I386 ICD configuration from immutable ZIP bytes."""
from pathlib import Path
import argparse
import hashlib
import json

ORIGINAL_BYTES = 145
ORIGINAL_SHA256 = '74d7d5d6ae9432cde2d802507ed59bbe4c2f2b95d01e7ac3c2b56e9691932c80'
DERIVED_BYTES = 148
DERIVED_SHA256 = 'f50169e3e0efc6dea34fe0ce109228c79ce1df817a5508fb13a759c71d780ff3'
ORIGINAL_PATH = 'viogpu_gl_vk_x86.dll'
DERIVED_PATH = '.\\viogpu_gl_vk_x86.dll'
OLD = b'"library_path": "viogpu_gl_vk_x86.dll"'
NEW = b'"library_path": ".\\\\viogpu_gl_vk_x86.dll"'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def derive(original):
    if len(original) != ORIGINAL_BYTES or sha(original) != ORIGINAL_SHA256:
        raise ValueError('Exact original 145-byte ZIP JSON is required')
    before = json.loads(original)
    if before != {'ICD': {'api_version': '1.4.354', 'library_path': ORIGINAL_PATH},
                  'file_format_version': '1.0.1'} or original.count(OLD) != 1:
        raise ValueError('Original JSON fields or unique library path differ')
    derived = original.replace(OLD, NEW, 1)
    expected = json.loads(original)
    expected['ICD']['library_path'] = DERIVED_PATH
    if (len(derived) != DERIVED_BYTES or sha(derived) != DERIVED_SHA256
            or json.loads(derived) != expected or derived.replace(NEW, OLD, 1) != original):
        raise ValueError('Deterministic library-path-only derivation failed')
    return derived


def prepare(original_path, output):
    original = original_path.read_bytes()
    derived = derive(original)
    if output.exists():
        raise ValueError('Fresh derived configuration path required')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as stream:
        stream.write(derived)
    if original_path.read_bytes() != original or output.read_bytes() != derived:
        raise ValueError('Original input changed or derived output differs')
    return {'schema': 'system-d3d8-owned-ICD-derivation-v1', 'derived': True,
            'original': {'path': str(original_path), 'bytes': len(original), 'sha256': sha(original)},
            'output': {'path': str(output), 'bytes': len(derived), 'sha256': sha(derived)},
            'changed_JSON_fields': ['ICD.library_path'],
            'old_library_path': ORIGINAL_PATH, 'new_library_path': DERIVED_PATH,
            'all_other_original_bytes_preserved': True, 'target_actions': 0,
            'module_loads': 0, 'Vulkan_instances': 0, 'hardware_admission': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.original, args.output), indent=2))
