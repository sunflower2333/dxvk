#pragma once
#include "umd_ddi.h"
#include <d3d11.h>

namespace dxvk::umd {

inline HRESULT ddiResult(HRESULT result) {
  if (result == DXGI_ERROR_WAS_STILL_DRAWING) return DXGI_DDI_ERR_WASSTILLDRAWING;
  if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET
      || result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
    return D3DDDIERR_DEVICEREMOVED;
  return result;
}

}
