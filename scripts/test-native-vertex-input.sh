#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
tmp=$(mktemp -d /tmp/dxvk-vertex-input.XXXXXX)
trap 'rm -rf -- "$tmp"' EXIT
compiler=${CXX:-clang++}
flags=(-std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined
  -fno-pie -no-pie -Iinclude/vulkan/include)
"$compiler" "${flags[@]}" -Isrc/dxvk tests/umd-vertex-input.cpp -o "$tmp/positive"
"$tmp/positive"
python3 - "$tmp/dxvk_vertex_input.h" <<'PY'
from pathlib import Path
import sys
source = Path('src/dxvk/dxvk_vertex_input.h').read_text()
anchor = 'i < state.viDivisorInfo.vertexBindingDivisorCount && eq;'
if source.count(anchor) != 1:
    raise RuntimeError('Divisor-comparison mutation anchor changed')
Path(sys.argv[1]).write_text(source.replace(anchor, anchor.replace(' && eq', '')))
PY
"$compiler" "${flags[@]}" -I"$tmp" tests/umd-vertex-input.cpp -o "$tmp/negative"
set +e
"$tmp/negative" > "$tmp/stdout" 2> "$tmp/stderr"
result=$?
set -e
python3 - "$result" "$tmp/stdout" "$tmp/stderr" <<'PY'
from pathlib import Path
import sys
lines = Path('tests/umd-vertex-input.cpp').read_text().splitlines()
matching = [i + 1 for i, line in enumerate(lines)
            if line.strip() == 'CHECK(!dxvk::equalVertexInputState(base, color));']
if len(matching) != 1:
    raise RuntimeError('Color-offset oracle anchor changed')
expected = f'FAIL vertex input line={matching[0]}'
if (sys.argv[1] != '1' or Path(sys.argv[2]).read_bytes()
        or Path(sys.argv[3]).read_text().strip() != expected):
    raise RuntimeError('Mutation failed somewhere other than the color-offset oracle')
print('PASS negative control: matching divisor cannot erase a color-offset mismatch')
PY
