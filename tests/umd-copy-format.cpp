// SPDX-License-Identifier: MIT
#include "../src/umd/umd_copy_format.h"
#include "../src/umd/umd_transfer_format.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"Copy format failure line=%d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
  // Independent DXGI group membership, including holes/noncontiguous groups.
  // Enumerate all pairs across the SDK's ordinary, video and reserved values.
  const std::vector<std::vector<unsigned>> groups = {
    {1,2,3,4},{5,6,7,8},{9,10,11,12,13,14},{15,16,17,18},{19,20,21,22},
    {23,24,25,89},{27,28,29,30,31,32},{33,34,35,36,37,38},{39,40,41,42,43},
    {44,45,46,47},{48,49,50,51,52},{53,54,55,56,57,58,59},{60,61,62,63,64},
    {70,71,72},{73,74,75},{76,77,78},{79,80,81},{82,83,84},{87,90,91},
    {88,92,93},{94,95,96},{97,98,99}
  };
  for (unsigned a = 0; a <= 191; ++a) for (unsigned b = 0; b <= 191; ++b) {
    bool expected = a == b;
    for (const auto& group : groups) {
      bool hasA = false, hasB = false;
      for (unsigned value : group) { hasA |= value == a; hasB |= value == b; }
      expected |= hasA && hasB;
    }
    expected |= (a == 67 && (b == 42 || b == 43)) || (b == 67 && (a == 42 || a == 43));
    const auto dst = static_cast<DXGI_FORMAT>(a), src = static_cast<DXGI_FORMAT>(b);
    CHECK(dxvk::umd::copyFormatsCompatible(dst,src) == expected);
    CHECK(dxvk::umd::copyFormatsCompatible(dst,src) == dxvk::umd::copyFormatsCompatible(src,dst));
    if (expected && a && b && dxvk::umd::transferTexelBytes(dst) && dxvk::umd::transferTexelBytes(src))
      CHECK(dxvk::umd::transferTexelBytes(dst) == dxvk::umd::transferTexelBytes(src));
  }
  CHECK(!dxvk::umd::copyFormatsCompatible(DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R8G8B8A8_UINT));
  CHECK(!dxvk::umd::copyFormatsCompatible(DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_B8G8R8X8_UNORM));
  CHECK(!dxvk::umd::copyFormatsCompatible(DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R9G9B9E5_SHAREDEXP));
  CHECK(!dxvk::umd::copyFormatsCompatible(DXGI_FORMAT_R32_TYPELESS,DXGI_FORMAT_R9G9B9E5_SHAREDEXP));
  CHECK(dxvk::umd::copyFormatsCompatible(DXGI_FORMAT_R32_SINT,DXGI_FORMAT_R9G9B9E5_SHAREDEXP));
  CHECK(dxvk::umd::copyFormatFamily(DXGI_FORMAT_R9G9B9E5_SHAREDEXP) == DXGI_FORMAT_R9G9B9E5_SHAREDEXP);
  std::printf("Copy format policy passed: %u checks, 36864 format pairs\n",checks);
}
