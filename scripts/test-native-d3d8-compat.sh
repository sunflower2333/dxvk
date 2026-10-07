#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
task_build="$(mktemp -d /tmp/dxvk-d3d8-compat.XXXXXX)"
trap 'rm -rf "$task_build"' EXIT
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer "$root/tests/umd-d3d8-compat.cpp" -o "$task_build/compat"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$task_build/compat"
