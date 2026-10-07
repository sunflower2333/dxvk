#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if [[ $# -eq 2 && $1 == --out ]]; then
  dx8_sm_build="$2"
  mkdir "$dx8_sm_build"
elif [[ $# -eq 0 ]]; then
  dx8_sm_build="$(mktemp -d /tmp/dxvk-d3d8-sm1.XXXXXX)"
  trap 'rm -rf "$dx8_sm_build"' EXIT
else
  echo 'usage: test-native-d3d8-sm1.sh [--out <fresh-owned-build-directory>]' >&2
  exit 64
fi
dep="$root/subprojects/dxbc-spirv"
sources=(sm3/sm3_types.cpp sm3/sm3_parser.cpp sm3/sm3_disasm.cpp
  sm3/sm3_converter.cpp sm3/sm3_io_map.cpp sm3/sm3_registers.cpp sm3/sm3_resources.cpp
  ir/ir.cpp ir/ir_builder.cpp ir/ir_utils.cpp
  util/util_float16.cpp util/util_swizzle.cpp util/util_log.cpp util/util_md5.cpp)
compiler="${CXX:-c++}"
flags=(-std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined
  -fno-omit-frame-pointer -DDXBC_SPV_ENABLE_SM3 -isystem "$dep")
objects=()
run() {
  printf '%q ' "$@" >> "$dx8_sm_build/commands.txt"
  printf '\n' >> "$dx8_sm_build/commands.txt"
  "$@"
}
# Our regression is warning-fatal. Pinned upstream translation units keep the
# upstream warning policy (C++17 structured-binding capture and an unused
# constant already warn under Clang); no dependency source is modified.
run "$compiler" "${flags[@]}" -Werror -c "$root/tests/umd-d3d8-sm1.cpp" -o "$dx8_sm_build/test.o"
for index in "${!sources[@]}"; do
  objects+=("$dx8_sm_build/dep-$index.o")
  run "$compiler" "${flags[@]}" -c "$dep/${sources[$index]}" -o "${objects[$index]}"
done
run "$compiler" -fsanitize=address,undefined "$dx8_sm_build/test.o" "${objects[@]}" -o "$dx8_sm_build/sm1"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 run "$dx8_sm_build/sm1"
