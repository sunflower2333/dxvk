#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
task_build="$(mktemp -d /tmp/dxvk-legacy-api.XXXXXX)"
trap 'rm -rf "$task_build"' EXIT
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer "$root/tests/umd-legacy-api.cpp" -o "$task_build/legacy"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$task_build/legacy"
