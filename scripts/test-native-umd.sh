#!/usr/bin/env bash
set -euo pipefail
bash scripts/test-native-vulkan-loader.sh
bash scripts/test-native-vertex-input.sh
dxvk_test_dir=$(mktemp -d /tmp/dxvk-umd-tests.XXXXXX)
trap 'rm -f "$dxvk_test_dir/identity" "$dxvk_test_dir/runtime-identity" "$dxvk_test_dir/runtime-backend" "$dxvk_test_dir/shader" "$dxvk_test_dir/sm41" "$dxvk_test_dir/d3d9-buffer-copy" "$dxvk_test_dir/private-children" "$dxvk_test_dir/multisample-policy" "$dxvk_test_dir/cube-array-policy"; rmdir "$dxvk_test_dir"' EXIT
for name in d3d9-buffer-copy private-children multisample-policy cube-array-policy; do
  clang++ -std=c++17 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
    "tests/umd-$name.cpp" -o "$dxvk_test_dir/$name"
  "$dxvk_test_dir/$name"
done
clang++ -std=c++17 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/umd-identity.cpp -o "$dxvk_test_dir/identity"
"$dxvk_test_dir/identity"
clang++ -std=c++17 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/umd-runtime-identity.cpp -o "$dxvk_test_dir/runtime-identity"
"$dxvk_test_dir/runtime-identity"
clang++ -std=c++17 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/umd-runtime-backend.cpp -o "$dxvk_test_dir/runtime-backend"
"$dxvk_test_dir/runtime-backend"
for fixture in shader-container sm41-container; do
  executable=shader
  if [[ "$fixture" == sm41-container ]]; then executable=sm41; fi
  clang++ -std=c++17 -g -O1 -Wall -Wextra -Wno-unused-private-field -fsanitize=address,undefined \
    -Isubprojects/dxbc-spirv "tests/umd-$fixture.cpp" src/umd/umd_shader.cpp \
    subprojects/dxbc-spirv/dxbc/dxbc_container.cpp \
    subprojects/dxbc-spirv/dxbc/dxbc_parser.cpp \
    subprojects/dxbc-spirv/dxbc/dxbc_signature.cpp \
    subprojects/dxbc-spirv/dxbc/dxbc_types.cpp \
    subprojects/dxbc-spirv/ir/ir.cpp \
    subprojects/dxbc-spirv/util/util_swizzle.cpp \
    subprojects/dxbc-spirv/util/util_log.cpp \
    subprojects/dxbc-spirv/util/util_md5.cpp -o "$dxvk_test_dir/$executable"
  "$dxvk_test_dir/$executable"
done
