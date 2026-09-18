#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
"${CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror -DNDEBUG \
  -fsanitize=address,undefined -fno-pie -no-pie \
  "$root/tests/umd-transfer-policy.cpp" -o "$tmp/transfer-policy"
"$tmp/transfer-policy"
# The shared-surface refresh/publish rules. Separated from the transfers they
# drive so they can be checked here, without a device or a kernel allocation.
"${CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror -DNDEBUG \
  -fsanitize=address,undefined -fno-pie -no-pie \
  "$root/tests/umd-shared-policy.cpp" -o "$tmp/shared-policy"
"$tmp/shared-policy"
