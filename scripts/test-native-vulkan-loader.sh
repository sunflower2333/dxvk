#!/usr/bin/env bash
set -euo pipefail
dxvk_loader_test_dir=$(mktemp -d /tmp/dxvk-vulkan-loader.XXXXXX)
trap 'rm -rf "$dxvk_loader_test_dir"' EXIT
printf '#define DXVK_PRIVATE_VULKAN_LOADER ""\n' > "$dxvk_loader_test_dir/vulkan_loader_config.h"
"${CXX:-clang++}" -std=c++17 -g -O1 -Wall -Wextra -Werror \
  -Wno-unused-parameter \
  -fsanitize=address,undefined -pthread \
  -Iinclude/native/windows -Iinclude/vulkan/include -I"$dxvk_loader_test_dir" \
  tests/umd-vulkan-loader.cpp src/vulkan/vulkan_loader.cpp -ldl \
  -o "$dxvk_loader_test_dir/loader"
"$dxvk_loader_test_dir/loader"
