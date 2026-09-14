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
common=(src/umd/umd_shader.cpp subprojects/dxbc-spirv/dxbc/dxbc_container.cpp
  subprojects/dxbc-spirv/dxbc/dxbc_parser.cpp subprojects/dxbc-spirv/dxbc/dxbc_signature.cpp
  subprojects/dxbc-spirv/dxbc/dxbc_types.cpp subprojects/dxbc-spirv/ir/ir.cpp
  subprojects/dxbc-spirv/util/util_swizzle.cpp subprojects/dxbc-spirv/util/util_log.cpp
  subprojects/dxbc-spirv/util/util_md5.cpp)
"$compiler" -std=c++17 -O1 -g -fsanitize=address,undefined -fno-pie -no-pie \
  -Isubprojects/dxbc-spirv tests/umd-mrt-signature.cpp "${common[@]}" -o "$tmp/signature"
"$tmp/signature"
