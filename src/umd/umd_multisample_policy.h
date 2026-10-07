#pragma once
// SPDX-License-Identifier: MIT
#include <cstdint>

namespace dxvk::umd {

// The native D3D10 DDI defines one quality for a successful single-sample
// query, independent of the public renderer's reported quality count. Keep
// backend output private until it succeeds; errors and exceptions publish 0.
template<typename Query>
bool multisampleQualityLevels(uint32_t count, uint32_t& output, Query&& query) {
  output = 0;
  if (!count || count > 32) return true;
  uint32_t levels = 0;
  if (!query(levels)) return false;
  output = count == 1 ? 1 : levels;
  return true;
}

}
