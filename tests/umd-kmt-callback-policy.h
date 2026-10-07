// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <limits>

namespace dxvk::umd::probe {

// An operation admits one named failing HRESULT, exactly one new callback,
// and no overwritten error. Publication is transactional on every rejection.
inline bool admitExpectedCoreCallback(unsigned accepted, unsigned raw,
    int32_t expectedHr, int32_t lastHr, unsigned& next) {
  if (expectedHr >= 0 || lastHr != expectedHr || accepted == std::numeric_limits<unsigned>::max()
      || raw != accepted + 1) return false;
  next = accepted + 1;
  return true;
}

} // namespace dxvk::umd::probe
