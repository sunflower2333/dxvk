#pragma once

#include "umd_ddi.h"
#include "umd_result.h"
#include <d3d11.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dxvk::umd {

struct QueryInfo {
  D3D11_QUERY type = D3D11_QUERY_EVENT;
  UINT size = 0;
  bool beginRequired = false;
  bool predicate = false;
  bool hint = false;
  UINT backendSize = 0;
};

inline bool queryInfo(D3D10DDI_QUERY type, UINT miscFlags, QueryInfo& result) {
  result = {};
  if (miscFlags & ~D3D10DDI_QUERY_MISCFLAG_PREDICATEHINT) return false;
  if (miscFlags && type != D3D10DDI_QUERY_OCCLUSIONPREDICATE) return false;
  switch (type) {
    case D3D10DDI_QUERY_EVENT:
      result = {D3D11_QUERY_EVENT, sizeof(BOOL), false}; return true;
    case D3D10DDI_QUERY_OCCLUSION:
      result = {D3D11_QUERY_OCCLUSION, sizeof(UINT64), true}; return true;
    case D3D10DDI_QUERY_TIMESTAMP:
      result = {D3D11_QUERY_TIMESTAMP, sizeof(UINT64), false}; return true;
    case D3D10DDI_QUERY_TIMESTAMPDISJOINT:
      static_assert(sizeof(D3D10_DDI_QUERY_DATA_TIMESTAMP_DISJOINT) == sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT));
      static_assert(offsetof(D3D10_DDI_QUERY_DATA_TIMESTAMP_DISJOINT, Disjoint)
        == offsetof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT, Disjoint));
      result = {D3D11_QUERY_TIMESTAMP_DISJOINT, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), true}; return true;
    case D3D10DDI_QUERY_OCCLUSIONPREDICATE:
      result = {D3D11_QUERY_OCCLUSION_PREDICATE, sizeof(BOOL), true, true, miscFlags != 0}; return true;
    case D3D10DDI_QUERY_STREAMOUTPUTSTATS:
      static_assert(sizeof(D3D10_DDI_QUERY_DATA_SO_STATISTICS) == sizeof(D3D11_QUERY_DATA_SO_STATISTICS));
      static_assert(offsetof(D3D10_DDI_QUERY_DATA_SO_STATISTICS, PrimitivesStorageNeeded)
        == offsetof(D3D11_QUERY_DATA_SO_STATISTICS, PrimitivesStorageNeeded));
      result = {D3D11_QUERY_SO_STATISTICS, sizeof(D3D10_DDI_QUERY_DATA_SO_STATISTICS), true}; return true;
    case D3D10DDI_QUERY_STREAMOVERFLOWPREDICATE:
      result = {D3D11_QUERY_SO_OVERFLOW_PREDICATE, sizeof(BOOL), true, true}; return true;
    case D3D10DDI_QUERY_PIPELINESTATS:
      // D3D10 has only the first eight counters. Never pass its smaller
      // caller buffer directly to the D3D11 backend's eleven-counter query.
      static_assert(sizeof(D3D10_DDI_QUERY_DATA_PIPELINE_STATISTICS) == 8*sizeof(UINT64));
      static_assert(offsetof(D3D10_DDI_QUERY_DATA_PIPELINE_STATISTICS, PSInvocations)
        == offsetof(D3D11_QUERY_DATA_PIPELINE_STATISTICS, PSInvocations));
      result = {D3D11_QUERY_PIPELINE_STATISTICS, sizeof(D3D10_DDI_QUERY_DATA_PIPELINE_STATISTICS),
        true, false, false, sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS)}; return true;
    default: return false;
  }
}

template<typename GetData>
HRESULT readQueryData(const QueryInfo& info, void* data, UINT size, UINT flags, GetData&& getData) {
  // A pending or failed GetData must leave caller memory untouched. Read
  // through local storage even when the backend writes before returning.
  alignas(D3D11_QUERY_DATA_PIPELINE_STATISTICS)
  std::array<unsigned char, sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS)> scratch = {};
  const UINT backendSize = info.backendSize ? info.backendSize : info.size;
  if (info.hint || !info.size || info.size > backendSize || backendSize > scratch.size() || (flags & ~D3D10_DDI_GET_DATA_DO_NOT_FLUSH)
      || (data ? size != info.size : size != 0)) return E_INVALIDARG;
  const UINT apiFlags = (flags & D3D10_DDI_GET_DATA_DO_NOT_FLUSH) ? D3D11_ASYNC_GETDATA_DONOTFLUSH : 0;
  const HRESULT hr = getData(data ? scratch.data() : nullptr, data ? backendSize : 0, apiFlags);
  if (hr == S_FALSE) return DXGI_DDI_ERR_WASSTILLDRAWING;
  if (hr != S_OK) return FAILED(hr) ? ddiResult(hr) : E_FAIL;
  if (data) std::memcpy(data, scratch.data(), size);
  return S_OK;
}

}
