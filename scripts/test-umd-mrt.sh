#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
compiler=${CXX:-g++}
"$compiler" -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-pie -no-pie tests/umd-output-policy.cpp -o "$tmp/policy"
"$tmp/policy"
if [[ $# -gt 1 || ( $# -eq 1 && "$1" != "--negative-control" ) ]]; then
  echo 'usage: test-umd-mrt.sh [--negative-control]' >&2
  exit 2
fi
shader=src/umd/umd_shader.cpp
if [[ $# -eq 1 ]]; then
  python3 - "$tmp/shader.cpp" <<'PYCODE'
from pathlib import Path
import sys
source = Path('src/umd/umd_shader.cpp').read_text()
anchor = 'dxbc::SignatureEntry("SV_Target", entry.registerIndex,'
if source.count(anchor) != 1:
    raise RuntimeError('Target-index mutation anchor changed')
Path(sys.argv[1]).write_text(source.replace(anchor, 'dxbc::SignatureEntry("SV_Target", 0,'))
PYCODE
  shader="$tmp/shader.cpp"
fi
common=("$shader" subprojects/dxbc-spirv/dxbc/dxbc_container.cpp
  subprojects/dxbc-spirv/dxbc/dxbc_parser.cpp subprojects/dxbc-spirv/dxbc/dxbc_signature.cpp
  subprojects/dxbc-spirv/dxbc/dxbc_types.cpp subprojects/dxbc-spirv/ir/ir.cpp
  subprojects/dxbc-spirv/util/util_swizzle.cpp subprojects/dxbc-spirv/util/util_log.cpp
  subprojects/dxbc-spirv/util/util_md5.cpp)
"$compiler" -std=c++17 -O1 -g -fsanitize=address,undefined -fno-pie -no-pie \
  -Isrc/umd -Isubprojects/dxbc-spirv tests/umd-mrt-signature.cpp "${common[@]}" -o "$tmp/signature"
if [[ $# -eq 0 ]]; then
  "$tmp/signature"
else
  set +e
  "$tmp/signature" > "$tmp/stdout" 2> "$tmp/stderr"
  result=$?
  set -e
  python3 - "$result" "$tmp/stderr" <<'PYCODE'
from pathlib import Path
import sys
lines = Path('tests/umd-mrt-signature.cpp').read_text().splitlines()
matching = [i + 1 for i, line in enumerate(lines)
            if 'CHECK(entry.getSemanticIndex()==expected.registerIndex);' in line]
if len(matching) != 1:
    raise RuntimeError('Expected-oracle anchor changed')
expected = f'FAIL MRT signature line={matching[0]}'
if sys.argv[1] != '1' or Path(sys.argv[2]).read_text().strip() != expected:
    raise RuntimeError('Mutation failed somewhere other than target-index oracle')
print('PASS negative control: collapsed semantic index rejected at exact target-index oracle')
PYCODE
fi
