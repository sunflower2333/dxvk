# SPDX-License-Identifier: MIT
"""Source transport only: reconstruct one reviewed file and store a Git blob.
No builds, tests, driver execution, installation, commits or ref writes occur.
"""
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import urllib.request

BASE = '2f0c9205524667c9291ebc7ee2198ebf87419a8a'
REPO = 'sunflower2333/dxvk'

# Check exact Git byte identity, not checkout line endings.
def blob_id(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()

# Store only the reviewed implementation blob; leave final commits to the connector.
def main():
    if os.environ['GITHUB_REPOSITORY'] != REPO or os.environ['GITHUB_REF'] != 'refs/heads/work/source-transfer-texture1d-20260915':
        raise RuntimeError('Unexpected repository or branch')
    parent = subprocess.check_output(['git', 'rev-parse', 'HEAD^'], text=True).strip()
    if parent != BASE:
        raise RuntimeError('Unexpected source parent')
    plan = json.loads(Path('source-plan.json').read_bytes())
    if plan['base'] != BASE or len(plan['files']) != 1:
        raise RuntimeError('Unexpected source plan')
    entry = plan['files'][0]
    if entry['path'] != 'src/umd/umd_ddi.cpp':
        raise RuntimeError('Unexpected destination')
    source = subprocess.check_output(['git', 'show', BASE + ':' + entry['path']])
    if blob_id(source) != entry['base_blob']:
        raise RuntimeError('Base blob mismatch')
    lines = source.decode('utf-8').splitlines(keepends=True)
    chunks = []
    for part in entry['parts']:
        if isinstance(part, str):
            chunks.append(part)
        else:
            start, count = part
            if start < 0 or count < 1 or start + count > len(lines):
                raise RuntimeError('Invalid source range')
            chunks.extend(lines[start:start + count])
    data = ''.join(chunks).encode('utf-8')
    if blob_id(data) != entry['git_blob'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise RuntimeError('Reviewed output hash mismatch')
    request = urllib.request.Request(
        'https://api.github.com/repos/' + REPO + '/git/blobs',
        data=json.dumps({'encoding': 'base64', 'content': base64.b64encode(data).decode()}).encode(),
        headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                 'Accept': 'application/vnd.github+json', 'Content-Type': 'application/json'},
        method='POST')
    with urllib.request.urlopen(request, timeout=60) as response:
        result = json.load(response)
    if result['sha'] != entry['git_blob']:
        raise RuntimeError('Remote blob mismatch')
    receipt = {key: entry[key] for key in ('path', 'base_blob', 'git_blob', 'sha256')}
    receipt.update(base=BASE, build_run=False, tests_run=False, refs_changed=False)
    Path('source-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt))

if __name__ == '__main__':
    main()
