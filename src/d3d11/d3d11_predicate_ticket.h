#pragma once
// SPDX-License-Identifier: Zlib
#include "d3d11_query_ticket.h"
#include "../dxvk/dxvk_gpu_query_data.h"

namespace dxvk {

  enum class D3D11PredicateKind {
    Invalid,
    Occlusion,
    StreamOverflow,
  };

  // One nonblocking exact-ticket poll. Caller owns the ticket/backend query.
  // Hint/invalid/unissued/pending/error paths leave the output untouched.
  template<typename Poll>
  DxvkGpuQueryStatus D3D11ReadPredicateTicket(
    const D3D11QueryTicketState* ticket,
          D3D11PredicateKind kind,
          bool hint,
          bool* result,
          Poll poll) {
    if (hint || (kind != D3D11PredicateKind::Occlusion && kind != D3D11PredicateKind::StreamOverflow)
     || !ticket || !ticket->endIssued())
      return DxvkGpuQueryStatus::Invalid;
    if (!ticket->endRecorded())
      return DxvkGpuQueryStatus::Pending;

    DxvkQueryData data = { };
    const auto status = poll(data);
    if (status != DxvkGpuQueryStatus::Available)
      return status;

    if (result) {
      *result = kind == D3D11PredicateKind::Occlusion
        ? data.occlusion.samplesPassed != 0
        : data.xfbStream.primitivesNeeded > data.xfbStream.primitivesWritten;
    }
    return DxvkGpuQueryStatus::Available;
  }

}
