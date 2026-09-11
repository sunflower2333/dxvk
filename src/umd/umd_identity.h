#pragma once

#include <array>
#include <cstdint>

namespace dxvk::umd {

using AdapterLuid = std::array<uint8_t, 8>;

// Kept independent of the Vulkan/Windows headers so identity rejection can
// be exercised on all CI hosts with adversarial adapter enumerations.
inline bool matchesAdapter(const AdapterLuid& requested,
                           bool luidValid,
                           const AdapterLuid& actual,
                           uint32_t requiredDriver,
                           uint32_t actualDriver) {
  const AdapterLuid zero = {};
  return requested != zero && luidValid && requested == actual
      && requiredDriver != 0 && requiredDriver == actualDriver;
}

}
