#pragma once
#include "umd_primary_policy.h"
#include <limits>

namespace dxvk::umd {

// The standard shared-primary allocation is an existing v0 linear surface.
// Its pitch remains authoritative for both the borrowed primary and the new
// internal staging allocation; refresh fields apply only to the primary.
template<typename Info>
inline PrimaryStatus openedPrimaryStatus(const Info& info) {
  if (info.magic != 0x504d5644 || info.version || info.headerSize != 80 || info.reserved
      || !info.width || !info.height || info.width > 16384 || info.height > 16384
      || info.pitch < uint64_t(info.width) * 4 || (info.pitch & 3)
      || info.size < uint64_t(info.pitch) * info.height
      || !info.refreshNumerator || !info.refreshDenominator)
    return PrimaryStatus::Invalid;
  if (info.flags != 1 || info.format < 1 || info.format > 3
      || info.alignment != 4096 || info.requestedIova || info.resetGeneration || info.contextId
      || info.size > std::numeric_limits<uint32_t>::max())
    return PrimaryStatus::Unsupported;
  return PrimaryStatus::Valid;
}

}
