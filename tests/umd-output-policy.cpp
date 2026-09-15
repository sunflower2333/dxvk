// SPDX-License-Identifier: MIT
#include "../src/umd/umd_output_policy.h"
#include <cstdio>
#include <cstdlib>
#include <climits>

using namespace dxvk::umd;
static unsigned checks;
// Test verdicts must not depend on NDEBUG or an installed graphics runtime.
static void check(bool condition, unsigned line) {
  checks++;
  if (!condition) { std::fprintf(stderr, "FAIL output policy line=%u\n", line); std::exit(1); }
}
#define CHECK(x) check(!!(x), __LINE__)

// Exercise actual production policy with sparse/permuted signatures and hostile bounds.
int main() {
  ShaderSignatureEntry outputs[9] = {};
  for (unsigned i = 0; i < 9; i++) outputs[i] = {0, i, 15};
  for (size_t n = 1; n <= 8; n++) CHECK(validFloatColorOutputs(outputs, n));
  CHECK(!validFloatColorOutputs(outputs, 0));
  CHECK(!validFloatColorOutputs(outputs, 9));
  CHECK(!validFloatColorOutputs(nullptr, 1));
  for (uint32_t slot = 0; slot < 8; slot++) {
    for (uint8_t mask = 1; mask < 16; mask++) {
      ShaderSignatureEntry value = {0, slot, mask};
      CHECK(validFloatColorOutputs(&value, 1));
      value.scalar = ShaderScalar::Float32; CHECK(validFloatColorOutputs(&value, 1));
      value.scalar = ShaderScalar::Uint32; CHECK(!validFloatColorOutputs(&value, 1));
      value.scalar = ShaderScalar::Sint32; CHECK(!validFloatColorOutputs(&value, 1));
    }
  }
  ShaderSignatureEntry sparse[] = {{0,7,15},{0,0,3},{0,3,1}};
  CHECK(validFloatColorOutputs(sparse, 3));
  auto bad = sparse[2]; bad.registerIndex = 7; sparse[2] = bad;
  CHECK(!validFloatColorOutputs(sparse, 3));
  for (uint32_t slot : {8u,31u,UINT32_MAX}) {
    ShaderSignatureEntry value = {0,slot,15}; CHECK(!validFloatColorOutputs(&value, 1));
  }
  for (uint8_t mask : {uint8_t(0),uint8_t(16),uint8_t(255)}) {
    ShaderSignatureEntry value = {0,0,mask}; CHECK(!validFloatColorOutputs(&value, 1));
  }
  for (uint32_t sys : {1u,6u,UINT32_MAX}) {
    ShaderSignatureEntry value = {sys,0,15}; CHECK(!validFloatColorOutputs(&value, 1));
  }
  for (uint32_t n = 0; n < 11; n++)
    for (uint32_t clear = 0; clear < 11; clear++)
      CHECK(validRenderTargetRange(n, clear) == (n + clear <= 8));
  CHECK(!validRenderTargetRange(UINT32_MAX, 1));
  CHECK(!validRenderTargetRange(1, UINT32_MAX));
  OutputShape shape{}, good{16,16,1,1,0};
  CHECK(mergeOutputShape(shape,good));
  CHECK(mergeOutputShape(shape,good));
  for (unsigned field = 0; field < 5; field++) {
    auto mismatch = good;
    switch (field) {
      case 0: mismatch.width = 8; break;
      case 1: mismatch.height = 8; break;
      case 2: mismatch.layers = 2; break;
      case 3: mismatch.samples = 4; break;
      case 4: mismatch.quality = 1; break;
    }
    CHECK(!mergeOutputShape(shape,mismatch));
    CHECK(mergeOutputShape(shape,good)); // A reject must not replace the prior shape.
  }
  CHECK(!mergeOutputShape(shape,{}));
  CHECK(outputRangesOverlap(1,2,2,2));
  CHECK(outputRangesOverlap(2,2,1,2));
  CHECK(!outputRangesOverlap(0,1,1,1));
  CHECK(!outputRangesOverlap(1,1,0,1));
  CHECK(outputRangesOverlap(UINT32_MAX-2,2,UINT32_MAX-1,1));
  CHECK(!outputRangesOverlap(UINT32_MAX-2,2,UINT32_MAX,1));
  std::printf("PASS output policy: %u checks; no graphics runtime\n",checks);
}
