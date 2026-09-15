# SPDX-License-Identifier: MIT
"""Store reviewed source bytes as blobs. No build, test, tree or ref write."""
import base64, hashlib, json, os, pathlib, subprocess, urllib.request, zlib
BASE = '049ba459aa7b6d0e97ec8d05abec197ca56e5e97'
assert os.environ['GITHUB_REPOSITORY'] == 'sunflower2333/dxvk'
assert os.environ['GITHUB_REF'] == 'refs/heads/work/source-transfer-texture1d-20260915'
payload = zlib.decompress(base64.b64decode(pathlib.Path('.github/repair.b64').read_text().strip(), validate=True))
assert hashlib.sha256(payload).hexdigest() == 'ca36110726571cdf8b62a77355ca837a25bfe22b873711bce58e0f0ae27bc6d0'
plan = json.loads(payload)
assert plan['base'] == BASE
outputs = []
seen = set()
for item in plan['files']:
    path = item['path']
    assert path not in seen and '..' not in pathlib.PurePosixPath(path).parts
    assert path.startswith(('src/umd/', 'tests/umd-', 'docs/native-', 'scripts/test-umd-', '.github/workflows/'))
    seen.add(path)
    source = b''
    if item['base_blob']:
        sha = subprocess.check_output(['git','rev-parse',BASE + ':' + path], text=True).strip()
        assert sha == item['base_blob'], path
        source = subprocess.check_output(['git','cat-file','blob',sha])
    if item['parts'] is None:
        outputs.append((path, None)); continue
    lines = source.decode('utf-8').splitlines(keepends=True)
    chunks = []
    for part in item['parts']:
        if isinstance(part, str): chunks.append(part)
        else:
            start, count = part
            assert start >= 0 and count > 0 and start + count <= len(lines)
            chunks.extend(lines[start:start + count])
    data = ''.join(chunks).encode('utf-8')
    assert hashlib.sha256(data).hexdigest() == item['sha256'], path
    outputs.append((path, data))
receipt = {'base':BASE, 'refs_updated':False, 'files':[]}
for path, data in outputs:
    sha = None
    if data is not None:
        sha = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        request = urllib.request.Request('https://api.github.com/repos/sunflower2333/dxvk/git/blobs',
            data=json.dumps({'encoding':'base64','content':base64.b64encode(data).decode()}).encode(),
            headers={'Authorization':'Bearer ' + os.environ['GH_TOKEN'], 'Accept':'application/vnd.github+json', 'Content-Type':'application/json'}, method='POST')
        with urllib.request.urlopen(request, timeout=60) as response: result = json.load(response)
        assert result['sha'] == sha, path
    receipt['files'].append({'path':path,'sha':sha,'mode':'100644','type':'blob'})
pathlib.Path('repair-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt))
