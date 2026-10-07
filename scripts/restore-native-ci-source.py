#!/usr/bin/env python3
"""Restore canonical tracked bytes in the fresh GitHub Actions checkout."""
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import subprocess


def git(*args, input=None):
    return subprocess.run(
        ['git', *args], input=input, stdout=subprocess.PIPE,
        check=True, timeout=60).stdout


def main():
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('Canonical source restoration requires a CI checkout')
    source = git('rev-parse', 'HEAD').decode('ascii').strip()
    if source != os.environ.get('GITHUB_SHA'):
        raise SystemExit('CI checkout differs from its exact source identity')
    root = Path(git('rev-parse', '--show-toplevel').decode().strip()).resolve()
    if Path.cwd().resolve() != root:
        raise SystemExit('Run from the root of the fresh CI checkout')

    entries = []
    for entry in git('ls-tree', '-rz', '--full-tree', source).split(b'\0'):
        if not entry:
            continue
        metadata, raw_name = entry.split(b'\t', 1)
        mode, kind, blob = metadata.decode('ascii').split()
        if mode == '160000' and kind == 'commit':
            continue  # Submodules retain their separately pinned checkout.
        name = raw_name.decode('utf-8')
        relative = PurePosixPath(name)
        if (kind != 'blob' or mode not in ('100644', '100755')
                or relative.is_absolute() or '..' in relative.parts
                or '\\' in name or ':' in name):
            raise SystemExit(f'Unsupported tracked CI input: {name}')
        entries.append((name, mode, blob))

    # checkout-index can leave stat-clean CRLF files untouched even with
    # --force. Raw objects also bypass text filters and archive attributes.
    objects = io.BytesIO(git('cat-file', '--batch', input=(
        ''.join(blob + '\n' for _, _, blob in entries)).encode('ascii')))
    rows = []
    for name, mode, blob in entries:
        identity, kind, size = objects.readline().decode('ascii').split()
        size = int(size)
        data = objects.read(size)
        if (identity != blob or kind != 'blob' or len(data) != size
                or objects.read(1) != b'\n'
                or hashlib.sha1(b'blob ' + str(size).encode('ascii')
                    + b'\0' + data).hexdigest() != blob):
            raise SystemExit(f'Raw Git object identity differs: {name}')
        destination = root / name
        if destination.is_symlink() or root not in destination.resolve().parents:
            raise SystemExit(f'Tracked CI input leaves the checkout: {name}')
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        rows.append({'path': name, 'mode': mode, 'git_blob': blob,
                     'bytes': size, 'sha256': hashlib.sha256(data).hexdigest()})
    if objects.read():
        raise SystemExit('Unexpected trailing Git object output')
    output = root / 'artifacts'
    output.mkdir(exist_ok=True)
    (output / 'native-canonical-source.json').write_text(json.dumps({
        'source_commit': source, 'run_id': os.environ.get('GITHUB_RUN_ID'),
        'method': 'raw-git-cat-file-batch', 'tracked_files': len(rows),
        'sources': rows}, indent=2) + '\n', encoding='utf-8')
    print(f'Restored {len(rows)} canonical tracked files from {source}')


if __name__ == '__main__':
    main()
