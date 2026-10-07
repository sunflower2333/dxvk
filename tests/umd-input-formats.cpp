#include "../src/umd/umd_input_format.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, "input format line %d: %s\n", __LINE__, #value); std::abort(); } } while (0)
int main() {
  using namespace dxvk::umd;
  struct Case { DXGI_FORMAT format; ShaderScalar scalar; unsigned mask, bytes; };
  const Case cases[] = {
    {DXGI_FORMAT_R8_UINT, ShaderScalar::Uint32, 1, 1},
    {DXGI_FORMAT_R8G8_SINT, ShaderScalar::Sint32, 3, 2},
    {DXGI_FORMAT_R8G8B8A8_UNORM, ShaderScalar::Float32, 15, 4},
    {DXGI_FORMAT_R16_FLOAT, ShaderScalar::Float32, 1, 2},
    {DXGI_FORMAT_R16G16_SNORM, ShaderScalar::Float32, 3, 4},
    {DXGI_FORMAT_R16G16B16A16_UINT, ShaderScalar::Uint32, 15, 8},
    {DXGI_FORMAT_R32G32B32_SINT, ShaderScalar::Sint32, 7, 12},
    {DXGI_FORMAT_R32G32B32A32_FLOAT, ShaderScalar::Float32, 15, 16},
    {DXGI_FORMAT_R10G10B10A2_UINT, ShaderScalar::Uint32, 15, 4},
    {DXGI_FORMAT_R10G10B10A2_UNORM, ShaderScalar::Float32, 15, 4},
    {DXGI_FORMAT_R11G11B10_FLOAT, ShaderScalar::Float32, 7, 4},
  };
  for (const auto& value : cases) {
    const auto format = inputFormat(value.format);
    CHECK(format.scalar == value.scalar && format.mask == value.mask && format.bytes == value.bytes);
    CHECK(format.alignment == std::min<unsigned>(value.bytes, 4));
  }
  for (DXGI_FORMAT invalid : {DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R16G16_TYPELESS,
      DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
      DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_NV12})
    CHECK(inputFormat(invalid).scalar == ShaderScalar::Unknown && !inputFormat(invalid).bytes);
  uint32_t offset = 0xa5a55a5a;
  const auto byte = inputFormat(DXGI_FORMAT_R8_SINT), half = inputFormat(DXGI_FORMAT_R16_FLOAT);
  const auto vector = inputFormat(DXGI_FORMAT_R32G32B32_FLOAT);
  CHECK(inputElementOffset(byte, 1, 0, offset) && offset == 1);
  CHECK(inputElementOffset(half, UINT32_MAX, 3, offset) && offset == 4);
  CHECK(inputElementOffset(vector, UINT32_MAX, 7, offset) && offset == 8);
  CHECK(inputElementOffset(vector, 2036, 0, offset) && offset == 2036);
  const uint32_t saved = offset;
  CHECK(!inputElementOffset(half, 1, 0, offset));
  CHECK(!inputElementOffset(vector, 2040, 0, offset));
  CHECK(!inputElementOffset(vector, UINT32_MAX, 2047, offset));
  CHECK(!inputElementOffset(byte, UINT32_MAX-1, 0, offset));
  CHECK(!inputElementOffset(byte, UINT32_MAX, UINT32_MAX, offset));
  CHECK(!inputElementOffset({}, 0, 0, offset) && offset == saved);
  std::printf("input formats PASS checks=%u; scalar/packed offsets/overflow, no GPU\n", checks);
}
