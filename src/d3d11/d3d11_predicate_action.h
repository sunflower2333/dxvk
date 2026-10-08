#pragma once
// SPDX-License-Identifier: Zlib
#include "../dxvk/dxvk_gpu_query_data.h"
#include <cstdint>

namespace dxvk {

  enum class D3D11PredicateDecision {
    Execute,
    Suppress,
    Invalid,
    Failed,
  };

  // Runs on the API dispatch thread. Submit must complete the preceding End's
  // CS recording and GPU submission before returning. It is never a CS task.
  // Null and hint bindings never poll; guaranteed predicates never time out
  // into unconditional execution. The caller retains the exact query/ticket.
  template<typename Poll, typename Submit, typename Healthy, typename Yield>
  D3D11PredicateDecision D3D11EvaluatePredicateAction(
          bool bound, bool hint, int32_t value,
          Poll poll, Submit submit, Healthy healthy, Yield yield) {
    if (!bound || hint)
      return D3D11PredicateDecision::Execute;
    if (!healthy())
      return D3D11PredicateDecision::Failed;

    bool result = false;
    auto status = poll(&result);
    if (status == DxvkGpuQueryStatus::Pending) {
      if (!submit())
        return D3D11PredicateDecision::Failed;
      do {
        if (!healthy())
          return D3D11PredicateDecision::Failed;
        status = poll(&result);
        if (status == DxvkGpuQueryStatus::Pending)
          yield();
      } while (status == DxvkGpuQueryStatus::Pending);
    }

    if (status == DxvkGpuQueryStatus::Invalid)
      return D3D11PredicateDecision::Invalid;
    if (status != DxvkGpuQueryStatus::Available)
      return D3D11PredicateDecision::Failed;
    return result == bool(value)
      ? D3D11PredicateDecision::Suppress
      : D3D11PredicateDecision::Execute;
  }

}
