#!/usr/bin/env python3
"""Exercise production recording failures, status propagation, and CS retirement."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=False)
inputs = {}


def source(name):
    path = a.source / name
    raw = path.read_bytes()
    inputs[str(path)] = hashlib.sha256(raw).hexdigest()
    return raw.decode()


def method(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


for ext in ['h', 'cpp']:
    text = source(f'src/dxvk/dxvk_cs.{ext}')
    (a.out / f'cs.{ext}').write_text(re.sub(r'^#include[^\n]*\n', '', text, flags=re.M))

text = source('src/dxvk/dxvk_device.h')
methods = [method(text, signature) for signature in
           ['VkResult getDeviceStatus() const', 'VkResult synchronizeSubmission()']]
if 'void recordCommandError(' in text:
    methods.append(method(text, 'void recordCommandError('))
(a.out / 'device.h').write_text('\n'.join(methods))
text = source('src/dxvk/dxvk_queue.cpp')
queue_error = ''
if 'void DxvkSubmissionQueue::recordCommandError(' in text:
    queue_error = method(text, 'void DxvkSubmissionQueue::recordCommandError(')
    queue_error = queue_error.replace('DxvkSubmissionQueue::', '')
(a.out / 'queue_error.h').write_text(queue_error)

text = source('src/dxvk/dxvk_cmdlist.h')
(a.out / 'pool.h').write_text(method(text, 'class DxvkCommandPool :') + ';\n')
(a.out / 'barrier.h').write_text(method(text, 'void cmdPipelineBarrier(') + '\n' +
    method(text, 'force_inline VkCommandBuffer getCmdBuffer(DxvkCmdBuffer'))
text = source('src/dxvk/dxvk_cmdlist.cpp')
start = text.index('DxvkCommandPool::DxvkCommandPool(')
end = text.index('DxvkCommandList::DxvkCommandList(', start)
methods = [text[start:end]]
for signature in ['void DxvkCommandList::init()',
                  'DxvkCommandList::~DxvkCommandList()',
                  'void DxvkCommandList::reset()',
                  'void DxvkCommandList::endCommandBuffer(',
                  'VkCommandBuffer DxvkCommandList::allocateCommandBuffer(']:
    methods.append(method(text, signature))
(a.out / 'command.cpp').write_text('namespace dxvk {\n' + '\n'.join(methods) + '\n}\n')
text = source('src/dxvk/dxvk_context.cpp')
(a.out / 'context.cpp').write_text('namespace dxvk {\n' +
    method(text, 'void DxvkContext::beginRecording(') + '\n}\n')

fixture = Path(__file__).with_suffix('.cpp')
for path in [fixture, Path(__file__), a.source / 'src/util/rc/util_rc_ptr.h',
             a.source / 'src/util/util_flags.h', a.source / 'src/util/util_bit.h',
             a.source / 'src/util/util_likely.h',
             a.source / 'include/vulkan/include/vulkan/vulkan_core.h']:
    inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
command = ['clang++', '-std=c++20', '-O1', '-g', '-pthread', '-I', str(a.out),
           '-I', str(a.source / 'include/vulkan/include'), str(fixture),
           '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
           '-o', str(a.out / 'test')]
with (a.out / 'build.log').open('w') as log:
    built = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
cases = []
if built.returncode == 0:
    for name in ['healthy', 'create', 'allocate', 'begin', 'secondary-allocate',
                 'secondary-begin', 'reset', 'end', 'publish', 'retire']:
        with (a.out / (name + '.log')).open('w') as log:
            try:
                run = subprocess.run([str(a.out / 'test'), name], stdout=log,
                                     stderr=subprocess.STDOUT, timeout=10)
                cases.append(dict(case=name, exit=run.returncode))
            except subprocess.TimeoutExpired:
                cases.append(dict(case=name, timeout=True))
receipt = dict(inputs=inputs, command=command, build_exit=built.returncode,
               cases=cases, boundary='Production command pool, command-list init/allocate/end/barrier, context beginRecording, device status and complete CS code; controlled Vulkan and submission peers. CPU contracts, not hardware crash causality.')
receipt['passed'] = built.returncode == 0 and bool(cases) and all(c.get('exit') == 0 for c in cases)
(a.out / 'result.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps({k: v for k, v in receipt.items() if k not in ['inputs', 'command']}))
raise SystemExit(0 if receipt['passed'] else 1)
