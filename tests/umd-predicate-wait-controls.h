#pragma once
// SPDX-License-Identifier: MIT
#include "../src/umd/umd_predicate_wait.h"
#include <initializer_list>

// The native query fixture supplies original SDK HRESULT constants. The
// portable runner supplies the same signed metadata values; it proves the
// production state machine, not Windows COM, GPU availability or DDI activation.
template<typename Check>
inline void predicateWaitControls(const Check& check,
    const dxvk::umd::PredicateWaitCodes& codes, std::int32_t removed,
    std::int32_t reset, std::int32_t outOfMemory) {
  using namespace dxvk::umd;
  for (unsigned combination = 0; combination < 4; ++combination) {
    const bool queryValue = (combination & 1) != 0;
    const bool predicateValue = (combination & 2) != 0;
    unsigned statuses = 0, reads = 0, pauses = 0;
    // Every modeled yield advances one millisecond. A live query deliberately
    // stays pending longer than the old two-second fabricated-removal limit.
    const auto resolved = resolveNativePredicate(false, predicateValue, codes,
      [] { return true; },
      [&] { check(statuses == reads); ++statuses; return codes.available; },
      [&](bool& value) {
        ++reads; check(statuses == reads); value = !queryValue;
        if (reads <= 5001) return codes.pending;
        value = queryValue; return codes.available;
      }, [&] { ++pauses; check(statuses == reads); });
    check(resolved.state == PredicateResolutionState::Available);
    check(resolved.error == codes.available && resolved.suppress == (queryValue == predicateValue));
    check(reads == 5002 && statuses == reads && pauses == 5001);
  }
  for (bool value : {false, true}) {
    unsigned calls = 0;
    const auto hinted = resolveNativePredicate(true, value, codes,
      [] { return true; }, [&] { ++calls; return removed; },
      [&](bool&) { ++calls; return codes.pending; }, [&] { ++calls; });
    check(hinted.state == PredicateResolutionState::Available && !hinted.suppress);
    check(hinted.error == codes.available && calls == 0);
  }
  for (std::int32_t failure : {removed, reset, outOfMemory}) {
    unsigned statuses = 0, reads = 0, pauses = 0;
    const auto observedDeviceFailure = resolveNativePredicate(false, false, codes,
      [] { return true; }, [&] { ++statuses; return statuses == 8 ? failure : codes.available; },
      [&](bool& value) { ++reads; value = true; return codes.pending; }, [&] { ++pauses; });
    check(observedDeviceFailure.state == PredicateResolutionState::Failed);
    check(observedDeviceFailure.error == failure && !observedDeviceFailure.suppress);
    check(statuses == 8 && reads == 7 && pauses == 7);
    const auto observedQueryFailure = resolveNativePredicate(false, true, codes,
      [] { return true; }, [&] { return codes.available; },
      [&](bool& value) { value = true; return failure; }, [] {});
    check(observedQueryFailure.state == PredicateResolutionState::Failed);
    check(observedQueryFailure.error == failure && !observedQueryFailure.suppress);
  }
  for (unsigned phase = 0; phase < 4; ++phase) {
    bool live = phase != 0;
    unsigned statuses = 0, reads = 0, pauses = 0;
    const auto retired = resolveNativePredicate(false, false, codes,
      [&] { return live; },
      [&] { ++statuses; if (phase == 1) live = false; return codes.available; },
      [&](bool& value) { ++reads; value = true; if (phase == 2) live = false;
        return phase == 2 ? codes.available : codes.pending; },
      [&] { ++pauses; live = false; });
    check(retired.state == PredicateResolutionState::Retired && !retired.suppress);
    check(retired.error == 0);
    check(statuses == unsigned(phase != 0));
    check(reads == unsigned(phase >= 2) && pauses == unsigned(phase == 3));
  }
  for (bool retireInStatus : {false, true}) {
    bool live = true;
    const auto retiredFailure = resolveNativePredicate(false, false, codes,
      [&] { return live; },
      [&] { if (retireInStatus) live = false; return retireInStatus ? removed : codes.available; },
      [&](bool&) { live = false; return reset; }, [] {});
    check(retiredFailure.state == PredicateResolutionState::Retired && retiredFailure.error == 0);
  }
  for (bool unexpectedStatus : {false, true}) {
    unsigned reads = 0, pauses = 0;
    const auto unexpected = resolveNativePredicate(false, false, codes,
      [] { return true; }, [&] { return unexpectedStatus ? 2 : codes.available; },
      [&](bool& value) { ++reads; value = true; return 2; }, [&] { ++pauses; });
    check(unexpected.state == PredicateResolutionState::Failed && unexpected.error == codes.unexpected);
    check(!unexpected.suppress && reads == unsigned(!unexpectedStatus) && pauses == 0);
  }
  // A caller commits only Available decisions. Pending/error/retirement cannot
  // replace a prior binding or expose a boolean written by a failed query.
  bool boundSuppression = true;
  auto publish = [&](const PredicateResolution& resolved) {
    if (resolved.state == PredicateResolutionState::Available) boundSuppression = resolved.suppress;
  };
  publish(resolveNativePredicate(false, true, codes, [] { return true; },
    [&] { return removed; }, [&](bool&) { return codes.available; }, [] {}));
  check(boundSuppression);
  publish(resolveNativePredicate(false, true, codes, [] { return false; },
    [&] { return codes.available; }, [&](bool&) { return codes.available; }, [] {}));
  check(boundSuppression);
  publish(resolveNativePredicate(false, true, codes, [] { return true; },
    [&] { return codes.available; }, [&](bool& value) { value = false; return codes.available; }, [] {}));
  check(!boundSuppression);
}
