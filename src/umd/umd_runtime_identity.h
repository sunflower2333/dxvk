#pragma once

#include "umd_identity.h"
#include <cstddef>

namespace dxvk::umd {

// Proposed, not produced by the current KMD. See doc/umd-identity-proposal.md.
// The original 128-byte v0 reply remains unchanged; a separately versioned
// optional trailer carries the kernel-supplied runtime adapter LUID.
constexpr size_t RuntimeIdentityReplySize = 160;

inline bool readProposedRuntimeIdentity(const void* reply, size_t size, AdapterLuid& result) {
  result = {};
  if (!reply || size != RuntimeIdentityReplySize) return false;
  auto bytes = static_cast<const uint8_t*>(reply);
  auto u32 = [&](size_t offset) {
    return uint32_t(bytes[offset]) | (uint32_t(bytes[offset+1]) << 8) |
      (uint32_t(bytes[offset+2]) << 16) | (uint32_t(bytes[offset+3]) << 24);
  };
  if (u32(0) != 0x504d5644 || u32(4) != 0 || u32(8) != 128 || u32(12) != 0 ||
      u32(128) != 0x44494c56 || u32(132) != 1 || u32(136) != 32 || u32(140) != 1 ||
      u32(152) != 1 || u32(156) != 0)
    return false;
  AdapterLuid luid;
  for (size_t i = 0; i < luid.size(); i++) luid[i] = bytes[144+i];
  if (luid == AdapterLuid{}) return false;
  result = luid;
  return true;
}

}
