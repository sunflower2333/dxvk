#pragma once
// SPDX-License-Identifier: MIT
#include <cstdint>

namespace dxvk::umd {

enum class PredicateResolutionState { Available, Retired, Failed };
struct PredicateResolution {
  PredicateResolutionState state = PredicateResolutionState::Retired;
  std::int32_t error = 0;
  bool suppress = false;
};
struct PredicateWaitCodes {
  std::int32_t available = 0;
  std::int32_t pending = 1;
  std::int32_t unexpected = 0;
};

// A pending query is not a failed device. The DDI may resolve the predicate
// synchronously, but it must not manufacture removal from a wall-clock limit.
// The caller retains query/context/device owners; every backend operation can
// reenter runtime callbacks, so check retirement before and after each one.
template<typename Live, typename Status, typename Read, typename Pause>
inline PredicateResolution resolveNativePredicate(bool hint, bool value,
    const PredicateWaitCodes& codes, const Live& live, const Status& status,
    const Read& read, const Pause& pause) {
  if (!live()) return {};
  // Hints need not suppress any operations and never require query readiness.
  if (hint) return {PredicateResolutionState::Available, codes.available, false};
  for (;;) {
    if (!live()) return {};
    const std::int32_t deviceStatus = status();
    if (!live()) return {};
    if (deviceStatus != codes.available)
      return {PredicateResolutionState::Failed,
        deviceStatus < 0 ? deviceStatus : codes.unexpected, false};
    bool result = false;
    const std::int32_t queried = read(result);
    if (!live()) return {};
    if (queried == codes.available)
      return {PredicateResolutionState::Available, codes.available, result == value};
    if (queried != codes.pending)
      return {PredicateResolutionState::Failed,
        queried < 0 ? queried : codes.unexpected, false};
    pause();
  }
}

}
