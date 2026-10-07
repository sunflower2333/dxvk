#pragma once
#include <cstdint>

namespace dxvk::umd {
enum class LegacyD3DApi : uint32_t { D3D8 = 8, D3D9 = 9 };
inline bool validLegacyD3DApi(uint32_t value) { return value == 8 || value == 9; }
inline bool legacyD3DApiMatches(LegacyD3DApi expected, uint32_t actual) {
  return validLegacyD3DApi(uint32_t(expected)) && uint32_t(expected) == actual;
}
inline bool legacyShaderModelAllowed(LegacyD3DApi api, uint32_t version, bool vertex) {
  if (!validLegacyD3DApi(uint32_t(api))) return false;
  if (api == LegacyD3DApi::D3D9) return true; // Shared framing/renderer validation follows.
  return (version >> 16) == (vertex ? 0xfffeu : 0xffffu)
    && ((version >> 8) & 255) == 1 && (version & 255) <= (vertex ? 1u : 4u);
}
inline uint32_t legacyFloatConstantLimit(LegacyD3DApi api, bool vertex, uint32_t nativeLimit) {
  if (api == LegacyD3DApi::D3D9) return nativeLimit;
  const uint32_t legacyLimit = vertex ? 96u : 8u;
  return api == LegacyD3DApi::D3D8 ? (nativeLimit < legacyLimit ? nativeLimit : legacyLimit) : 0;
}
}
