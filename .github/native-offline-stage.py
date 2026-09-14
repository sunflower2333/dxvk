#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""One-shot reviewed source transfer. Final builds compile tracked source directly."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import urllib.request

BASE = '9090cb22e6d32131f08e7779fa88b31671b3f919'
REPO = 'sunflower2333/dxvk'
BRANCH = 'refs/heads/work/native-offline-contracts-20260915'


# Reconstruct exact reviewed bytes; publish only content-addressed blobs, never refs.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    if os.environ['GITHUB_REPOSITORY'] != REPO or os.environ['GITHUB_REF'] != BRANCH:
        raise RuntimeError('Unexpected repository or staging branch')
    plan = json.loads(Path('.github/native-offline-edits.json').read_bytes())
    if plan['base'] != BASE:
        raise RuntimeError('Wrong review baseline')
    outputs = {}
    for path, item in plan['files'].items():
        if not path.startswith('src/umd/') or '..' in Path(path).parts:
            raise RuntimeError('Unexpected path')
        sha = subprocess.check_output(['git', 'rev-parse', BASE + ':' + path], text=True).strip()
        if sha != item['base_blob']:
            raise RuntimeError('Baseline mismatch: ' + path)
        data = subprocess.check_output(['git', 'cat-file', 'blob', sha])
        text = data.decode('utf-8')
        for old, new in item['replacements']:
            if text.count(old) != 1:
                raise RuntimeError('Review anchor mismatch: ' + path)
            text = text.replace(old, new)
        data = text.encode('utf-8')
        if hashlib.sha256(data).hexdigest() != item['sha256']:
            raise RuntimeError('Reviewed result mismatch: ' + path)
        outputs[path] = data
    receipt = {'base': BASE, 'source': os.environ['GITHUB_SHA'], 'refs_updated': False, 'files': []}
    for path, data in outputs.items():
        Path(path).write_bytes(data)
        sha = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if args.publish:
            payload = json.dumps({'encoding': 'base64', 'content': base64.b64encode(data).decode()}).encode()
            request = urllib.request.Request('https://api.github.com/repos/' + REPO + '/git/blobs',
                data=payload, headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                'Accept': 'application/vnd.github+json', 'Content-Type': 'application/json'})
            with urllib.request.urlopen(request, timeout=60) as response:
                result = json.load(response)
            if result['sha'] != sha:
                raise RuntimeError('Uploaded blob mismatch')
        receipt['files'].append({'path': path, 'sha': sha, 'sha256': hashlib.sha256(data).hexdigest()})
    Path('native-offline-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('PASS exact reviewed source transfer:', len(outputs), 'files; no refs changed')


if __name__ == '__main__':
    main()
