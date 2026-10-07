#include "../src/umd/umd_d3d8_compat.h"
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include "umd-d3d8-api.h"
static_assert(sizeof(D3DCAPS8) == dxvk::umd::d3d8CapsBytes);
static_assert(offsetof(D3DCAPS8, VertexShaderVersion) == 49 * sizeof(uint32_t));
static_assert(offsetof(D3DCAPS8, MaxPixelShaderValue) == 52 * sizeof(uint32_t));
extern bool d3d8SdkProjectionContract();
#endif

namespace {
unsigned checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "CHECK failed line=%d: %s\n", __LINE__, #value); std::_Exit(1); } } while (0)
}
int main() {
  using namespace dxvk::umd;
  D3D8CapsPrefix input{};
  for (size_t i = 0; i < input.size(); ++i) input[i] = uint32_t(i + 1);
  input[3] = input[4] = input[8] = input[9] = input[11] = input[12] = input[34] = input[39] = 0xffffffff;
  input[49] = 0xfffe0200; input[50] = 256; input[51] = 0xffff0200;
  const auto original = input;
  std::array<uint32_t, 55> guarded; guarded.fill(0xa5a5a5a5);
  CHECK(projectD3D8CapsPrefix(input, guarded.data() + 1, d3d8CapsBytes) == D3D8CapsResult::Success);
  CHECK(guarded.front() == 0xa5a5a5a5 && guarded.back() == 0xa5a5a5a5 && input == original);
  const std::array<size_t, 11> changed{{3,4,8,9,11,12,34,39,49,50,51}};
  for (size_t i = 0; i < input.size(); ++i)
    if (std::find(changed.begin(), changed.end(), i) == changed.end()) CHECK(guarded[i + 1] == input[i]);
  CHECK(guarded[50] == 0xfffe0101 && guarded[51] == 96 && guarded[52] == 0xffff0104);
  CHECK(guarded[4] == 0x301a0002 && guarded[5] == 0x20 && guarded[9] == 0x3ff6);
  CHECK(guarded[10] == 0xffffff && guarded[12] == 0x1fff && guarded[13] == 0x1fff);
  CHECK(guarded[35] == 0xff && guarded[40] == 0xfb);
  const auto published = guarded;
  for (size_t bytes = 0; bytes <= d3d8CapsBytes + 16; ++bytes) {
    if (bytes == d3d8CapsBytes) continue;
    CHECK(projectD3D8CapsPrefix(input, guarded.data() + 1, bytes) == D3D8CapsResult::InvalidArgument);
    CHECK(guarded == published);
  }
  CHECK(projectD3D8CapsPrefix(input, nullptr, d3d8CapsBytes) == D3D8CapsResult::InvalidArgument);
  for (const uint32_t shader : {0u, 0xffff0101u, 0xffff0102u, 0xffff0103u, 0xffff0104u, 0xffff0300u}) {
    auto caps = input; caps[51] = shader;
    CHECK(projectD3D8CapsPrefix(caps, guarded.data() + 1, d3d8CapsBytes) == D3D8CapsResult::Success);
    CHECK(guarded[52] == (shader ? std::min(shader, 0xffff0104u) : 0));
  }
  for (const uint32_t bad : {0xfffe0100u, 0xfffe0102u, 0xfffe0201u, 0xffff0200u, 1u, 0xffffffffu}) {
    auto caps = input; caps[49] = bad; const auto before = guarded;
    CHECK(projectD3D8CapsPrefix(caps, guarded.data() + 1, d3d8CapsBytes) == D3D8CapsResult::InvalidArgument);
    CHECK(guarded == before);
  }
  for (const uint32_t bad : {0xffff0100u, 0xffff0105u, 0xffff0201u, 0xfffe0200u, 1u, 0xffffffffu}) {
    auto caps = input; caps[51] = bad; const auto before = guarded;
    CHECK(projectD3D8CapsPrefix(caps, guarded.data() + 1, d3d8CapsBytes) == D3D8CapsResult::InvalidArgument);
    CHECK(guarded == before);
  }
  auto noOptional = input; noOptional[17] = noOptional[18] = noOptional[20] = noOptional[24] = 0;
  noOptional[49] = noOptional[51] = 0;
  CHECK(projectD3D8CapsPrefix(noOptional, noOptional.data(), d3d8CapsBytes) == D3D8CapsResult::Success);
  CHECK(!noOptional[17] && !noOptional[18] && !noOptional[20] && !noOptional[24] && !noOptional[50]);
#ifdef _WIN32
  CHECK(d3d8SdkProjectionContract());
#endif
  std::printf("native D3D8 caps projection PASS checks=%u caps_bytes=%zu VS1.1/PS1.4 bound; admission unchanged\n", checks, d3d8CapsBytes);
}
