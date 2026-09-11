#pragma once
#include "umd_result.h"
#include <d3d11.h>

namespace dxvk::umd {

template<typename Map>
HRESULT mapSubresource(D3D10_DDI_MAP type, UINT flags, D3D10DDI_MAPPED_SUBRESOURCE* output, Map&& map) {
  if (!output) return E_INVALIDARG;
  *output = {};
  if (type < D3D10_DDI_MAP_READ || type > D3D10_DDI_MAP_WRITE_NOOVERWRITE
      || (flags & ~D3D10_DDI_MAP_FLAG_MASK)) return E_INVALIDARG;
  const UINT apiFlags = (flags & D3D10_DDI_MAP_FLAG_DONOTWAIT) ? D3D11_MAP_FLAG_DO_NOT_WAIT : 0;
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  const HRESULT hr = map(static_cast<D3D11_MAP>(type), apiFlags, &mapped);
  if (hr != S_OK) return FAILED(hr) ? ddiResult(hr) : E_FAIL;
  if (!mapped.pData) return E_FAIL;
  output->pData = mapped.pData; output->RowPitch = mapped.RowPitch; output->DepthPitch = mapped.DepthPitch;
  return S_OK;
}

}
