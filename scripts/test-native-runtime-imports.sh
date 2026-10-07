#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=${CXX:-c++}
task_build=$(mktemp -d "${TMPDIR:-/tmp}/dxvk-runtime-imports.XXXXXX")
trap 'rm -rf -- "$task_build"' EXIT
"$compiler" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer "$root/tests/umd-runtime-imports.cpp" -o "$task_build/runtime-imports"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$task_build/runtime-imports"
