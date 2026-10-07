#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

struct _D3DCAPS9;

namespace dxvk::umd {

// D3DCAPS8 is the 53-DWORD prefix of D3DCAPS9. The Windows translation unit
// proves each boundary against the SDK; no D3D9 tail is published to DX8.
using D3D8CapsPrefix = std::array<uint32_t, 53>;
constexpr size_t d3d8CapsBytes = sizeof(D3D8CapsPrefix);
enum class D3D8CapsResult { Success, InvalidArgument };

inline bool d3d8ShaderVersion(uint32_t value, uint32_t stage) {
  if (!value) return true;
  const uint32_t version = value & 0xffff;
  return (value & 0xffff0000) == stage
    && (version == 0x0101 || (stage == 0xffff0000 && version >= 0x0102 && version <= 0x0104)
        || version == 0x0200 || version == 0x0300);
}

// Preserve the native profile's limits and optional resource support. Strip
// D3D9-only flags and bound shader versions to the D3D8.1 API. This does not
// invent windowed/gamma/cube/volume/MSAA/managed-resource capability bits.
inline D3D8CapsResult projectD3D8CapsPrefix(const D3D8CapsPrefix& source,
                                         void* output, size_t bytes) {
  if (!output || bytes != d3d8CapsBytes) return D3D8CapsResult::InvalidArgument;
  auto local = source; // Snapshot before publishing, including aliased output.
  if (!d3d8ShaderVersion(local[49], 0xfffe0000)
      || !d3d8ShaderVersion(local[51], 0xffff0000)) return D3D8CapsResult::InvalidArgument;
  local[3] &= 0x301a0002u; // D3DCAPS2: legacy bits, excludes AUTOGENMIPMAP/reserved.
  local[4] &= 0x00000020u; // D3DCAPS3: no D3D9 COPY_TO_* flags.
  local[8] &= 0x00003ff6u; // PrimitiveMisc: legacy FOGANDSPECULARALPHA included.
  local[9] &= 0x00ffffffu; // No SCISSORTEST/SLOPESCALEDEPTHBIAS/DEPTHBIAS.
  local[11] &= 0x00001fffu; local[12] &= 0x00001fffu; // No BLENDFACTOR.
  local[34] &= 0x000000ffu; // No two-sided stencil.
  local[39] &= 0x000000fbu; // No D3D9 TEXGEN_SPHEREMAP.
  if (local[49]) local[49] = std::min(local[49], 0xfffe0101u);
  local[50] = local[49] ? std::min(local[50], 96u) : 0;
  if (local[51]) local[51] = std::min(local[51], 0xffff0104u);
  std::memcpy(output, local.data(), d3d8CapsBytes);
  return D3D8CapsResult::Success;
}

// The integration caller maps InvalidArgument to E_INVALIDARG, snapshots its
// GetCaps arguments, validates identity and epoch, then publishes atomically.
D3D8CapsResult projectD3D8Caps9(const _D3DCAPS9& source, void* output, size_t bytes);

}
