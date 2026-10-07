#include "umd_d3d8_compat.h"
#include <d3d9.h>

namespace dxvk::umd {
static_assert(d3d8CapsBytes == 212);
static_assert(offsetof(D3DCAPS9, DevCaps2) == d3d8CapsBytes);
static_assert(offsetof(D3DCAPS9, VertexShaderVersion) == 49 * sizeof(uint32_t));
static_assert(offsetof(D3DCAPS9, MaxVertexShaderConst) == 50 * sizeof(uint32_t));
static_assert(offsetof(D3DCAPS9, PixelShaderVersion) == 51 * sizeof(uint32_t));
static_assert(offsetof(D3DCAPS9, PixelShader1xMaxValue) == 52 * sizeof(uint32_t));

D3D8CapsResult projectD3D8Caps9(const _D3DCAPS9& source, void* output, size_t bytes) {
  D3D8CapsPrefix snapshot;
  std::memcpy(snapshot.data(), &source, sizeof(snapshot));
  return projectD3D8CapsPrefix(snapshot, output, bytes);
}
}
