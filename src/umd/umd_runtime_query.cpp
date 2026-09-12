#include "umd_adapter.h"
#include <cstring>

HRESULT dxvk::umd::queryRuntimeIdentity(D3D10DDI_HRTADAPTER runtime,
    PFND3DDDI_QUERYADAPTERINFOCB query, RuntimeIdentity& result) {
  result = {};
  if (!runtime.handle || !query) return E_INVALIDARG;
  std::array<uint8_t, RuntimeIdentityReplySize> reply = {};
  D3DDDICB_QUERYADAPTERINFO args = {};
  args.pPrivateDriverData = reply.data();
  args.PrivateDriverDataSize = static_cast<UINT>(reply.size());
  const HRESULT hr = query(runtime.handle, &args);
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  return readRuntimeIdentity(reply.data(), args.PrivateDriverDataSize, result)
    ? S_OK : DXGI_ERROR_UNSUPPORTED;
}

extern "C" HRESULT APIENTRY VioGpuDxvkQueryRuntimeAdapterLuid(
    D3D10DDI_HRTADAPTER runtime, PFND3DDDI_QUERYADAPTERINFOCB query, LUID* luid) {
  if (!luid) return E_INVALIDARG;
  *luid = {};
  if (!runtime.handle || !query) return E_INVALIDARG;
  std::array<uint8_t, dxvk::umd::RuntimeIdentityReplySize> reply = {};
  D3DDDICB_QUERYADAPTERINFO args = {};
  args.pPrivateDriverData = reply.data();
  args.PrivateDriverDataSize = static_cast<UINT>(reply.size());
  // The opaque runtime handle is passed back only to its own callback.
  const HRESULT hr = query(runtime.handle, &args);
  if (FAILED(hr)) return hr;
  dxvk::umd::AdapterLuid identity;
  if (!dxvk::umd::readProposedRuntimeIdentity(reply.data(), args.PrivateDriverDataSize, identity))
    return DXGI_ERROR_UNSUPPORTED;
  static_assert(sizeof(*luid) == sizeof(identity));
  std::memcpy(luid, identity.data(), sizeof(*luid));
  return S_OK;
}
