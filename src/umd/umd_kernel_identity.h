#pragma once

#include "umd_adapter_identity.h"
#include <winternl.h>
#include <d3dkmthk.h>

namespace dxvk::umd {

// Kept internal to the UMD. The explicit API table lets the CPU fixture check
// kernel failures and handle ownership without installing a display driver.
struct KernelIdentityApi {
  decltype(&D3DKMTOpenAdapterFromLuid) open;
  decltype(&D3DKMTQueryAdapterInfo) query;
  decltype(&D3DKMTCloseAdapter) close;
  decltype(&D3DKMTEscape) escape = nullptr;
  decltype(&D3DKMTGetDisplayModeList) modes = nullptr;
};

HRESULT openKernelIdentityWithApi(const RuntimeIdentity& initial,
  const KernelIdentityApi& api, std::shared_ptr<const AdapterIdentitySource>& source);

}
