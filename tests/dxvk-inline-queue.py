#!/usr/bin/env python3
"""Compile the actual queue implementation against controlled Vulkan peers."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
p.add_argument('--out', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--case',action='append')
p.add_argument('--timeout',type=float,default=30)
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=False)
inputs = {}
for ext in ['h', 'cpp']:
    source = a.source / f'src/dxvk/dxvk_queue.{ext}'
    raw = source.read_bytes()
    inputs[str(source)] = hashlib.sha256(raw).hexdigest()
    # Only replace dependencies; preserve every production method and field.
    text = re.sub(r'^#include[^\n]*\n', '', raw.decode(), flags=re.M)
    (a.out / f'dxvk_queue_under_test.{ext}').write_text(text)
fixture = Path(__file__).with_suffix('.cpp')
inputs[str(fixture)] = hashlib.sha256(fixture.read_bytes()).hexdigest()
cmd = ['clang++', '-std=c++17', '-O1', '-g', '-pthread', '-include', 'utility',
       '-I', str(a.out), str(fixture), '-o', str(a.out / 'test')]
if a.sanitize:
    cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
with (a.out / 'build.log').open('w') as log:
    build = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT)
result = {'inputs': inputs, 'command': cmd, 'build_exit': build.returncode,
          'boundary': 'Actual production queue bodies; controlled Vulkan, resource and presenter peers. No hardware proof.'}
if build.returncode == 0:
    results=[]
    for case in a.case or ['standard','lost-resource','lost-submission','recording-error','recording-oom']:
        with (a.out / (case+'.log')).open('w') as log:
            try:
                run = subprocess.run([str(a.out / 'test')]+([] if case=='standard' else [case]), stdout=log, stderr=subprocess.STDOUT, timeout=a.timeout)
                results.append({'case':case,'exit':run.returncode})
            except subprocess.TimeoutExpired:results.append({'case':case,'timeout':True})
    result['cases']=results
    result['test_exit']=0 if all(x.get('exit')==0 for x in results) else 1
(a.out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result))
raise SystemExit(result.get('test_exit', build.returncode))
