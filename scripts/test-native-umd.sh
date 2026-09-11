#!/usr/bin/env bash
set -euo pipefail
dxvk_test_dir=$(mktemp -d /tmp/dxvk-umd-tests.XXXXXX)
trap 'rm -f "$dxvk_test_dir/identity" "$dxvk_test_dir/shader"; rmdir "$dxvk_test_dir"' EXIT
clang++ -std=c++17 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/umd-identity.cpp -o "$dxvk_test_dir/identity"
"$dxvk_test_dir/identity"
clang++ -std=c++17 -g -O1 -Wall -Wextra -Wno-unused-private-field -fsanitize=address,undefined \
  -Isubprojects/dxbc-spirv tests/umd-shader-container.cpp src/umd/umd_shader.cpp \
  subprojects/dxbc-spirv/dxbc/dxbc_container.cpp \
  subprojects/dxbc-spirv/dxbc/dxbc_parser.cpp \
  subprojects/dxbc-spirv/dxbc/dxbc_signature.cpp \
  subprojects/dxbc-spirv/dxbc/dxbc_types.cpp \
  subprojects/dxbc-spirv/ir/ir.cpp \
  subprojects/dxbc-spirv/util/util_swizzle.cpp \
  subprojects/dxbc-spirv/util/util_log.cpp \
  subprojects/dxbc-spirv/util/util_md5.cpp -o "$dxvk_test_dir/shader"
"$dxvk_test_dir/shader"
