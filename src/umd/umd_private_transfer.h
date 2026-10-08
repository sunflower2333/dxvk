#pragma once
// SPDX-License-Identifier: MIT
#include <d3d11.h>
#include <wrl/client.h>

namespace dxvk::umd {

// Internal ownership/readback work must run independently of application
// predication and pipeline bindings. Keep the owner's pins and reentrant live
// checks at the call site; only a complete live list may enter the GPU stream.
template<typename Live, typename Record>
inline HRESULT submitPrivateTransfer(ID3D11Device* device, ID3D11DeviceContext* context,
    const Live& live, const Record& record) {
  using Microsoft::WRL::ComPtr;
  if (!device || !context || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
    return E_INVALIDARG;
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  ComPtr<ID3D11DeviceContext> commands;
  HRESULT hr = device->CreateDeferredContext(0, &commands);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  if (!commands) return E_FAIL;
  hr = record(commands.Get());
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  ComPtr<ID3D11CommandList> list;
  hr = commands->FinishCommandList(FALSE, &list);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  if (!list) return E_FAIL;
  context->ExecuteCommandList(list.Get(), TRUE);
  return live() ? device->GetDeviceRemovedReason() : DXGI_ERROR_DEVICE_REMOVED;
}

}
