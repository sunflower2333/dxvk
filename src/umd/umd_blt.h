#pragma once
// SPDX-License-Identifier: MIT
#include "umd_blt_shader.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <algorithm>

namespace dxvk::umd {

inline constexpr DXGI_FORMAT bltLinearFormat(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
    default: return DXGI_FORMAT_UNKNOWN;
  }
}
struct BltPlan {
  UINT sourceWidth = 0, sourceHeight = 0, width = 0, height = 0;
  DXGI_FORMAT sourceFormat = DXGI_FORMAT_UNKNOWN, destinationFormat = DXGI_FORMAT_UNKNOWN;
  bool resolve = false;
};

// The original DDI bind metadata is validated separately. Reject unsupported
// display formats (including float/gamma conversion) rather than advertise a
// conversion the embedded renderer has not implemented here.
inline HRESULT bltPlan(const D3D11_TEXTURE2D_DESC& src,
    const D3D11_TEXTURE2D_DESC& dst, UINT sourceSubresource, UINT destinationSubresource,
    UINT left, UINT top, UINT right, UINT bottom, UINT flags, UINT rotation,
    BltPlan& output) {
  if ((flags & ~15u) || rotation < 1 || rotation > 4
      || !src.Width || !src.Height || !dst.Width || !dst.Height
      || src.Width > 16384 || src.Height > 16384 || dst.Width > 16384 || dst.Height > 16384
      || !src.MipLevels || !dst.MipLevels || src.MipLevels > 15 || dst.MipLevels > 15
      || !src.ArraySize || !dst.ArraySize || src.ArraySize > 2048 || dst.ArraySize > 2048
      || sourceSubresource >= src.MipLevels * src.ArraySize
      || destinationSubresource >= dst.MipLevels * dst.ArraySize
      || right <= left || bottom <= top) return E_INVALIDARG;
  const UINT dstWidth = std::max(1u, dst.Width >> (destinationSubresource % dst.MipLevels));
  const UINT dstHeight = std::max(1u, dst.Height >> (destinationSubresource % dst.MipLevels));
  if (right > dstWidth || bottom > dstHeight) return E_INVALIDARG;
  if (!(dst.BindFlags & D3D11_BIND_RENDER_TARGET) || dst.Usage != D3D11_USAGE_DEFAULT
      || src.Usage != D3D11_USAGE_DEFAULT || dst.SampleDesc.Count != 1
      || dst.SampleDesc.Quality || !src.SampleDesc.Count) return DXGI_ERROR_UNSUPPORTED;
  BltPlan staged;
  staged.sourceFormat = bltLinearFormat(src.Format);
  staged.destinationFormat = bltLinearFormat(dst.Format);
  if (staged.sourceFormat == DXGI_FORMAT_UNKNOWN || staged.destinationFormat == DXGI_FORMAT_UNKNOWN)
    return DXGI_ERROR_UNSUPPORTED;
  staged.resolve = (flags & 1) != 0;
  if (staged.resolve != (src.SampleDesc.Count > 1)) return E_INVALIDARG;
  // Encoded sRGB copies must not decode before filtering. Single-sample
  // copies below reinterpret via compatible UNORM scratch. sRGB MSAA remains
  // rejected until its encoded-domain resolve semantics are verified.
  if (staged.resolve && src.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
    return DXGI_ERROR_UNSUPPORTED;
  if (src.Format != dst.Format && !(flags & 2)) return E_INVALIDARG;
  staged.sourceWidth = std::max(1u, src.Width >> (sourceSubresource % src.MipLevels));
  staged.sourceHeight = std::max(1u, src.Height >> (sourceSubresource % src.MipLevels));
  staged.width = right - left; staged.height = bottom - top;
  const bool swapped = rotation == 2 || rotation == 4;
  if (!(flags & 4) && (staged.width != (swapped ? staged.sourceHeight : staged.sourceWidth)
      || staged.height != (swapped ? staged.sourceWidth : staged.sourceHeight))) return E_INVALIDARG;
  output = staged;
  return S_OK;
}

// All intermediates and immutable pipeline objects exist before any commands
// are recorded. The private deferred context starts with no predication or
// application bindings; ExecuteCommandList(TRUE) restores the original state.
// Only the final GPU copy modifies the destination rectangle, including Present.
template<typename Live>
inline HRESULT bltTexture2D(ID3D11Device* device, ID3D11DeviceContext* context,
    ID3D11Texture2D* source, ID3D11Texture2D* destination, UINT sourceSubresource,
    UINT destinationSubresource, UINT left, UINT top, UINT rotation, const BltPlan& plan, const Live& live) {
  using Microsoft::WRL::ComPtr;
  if (!device || !context || !source || !destination) return E_INVALIDARG;
  std::vector<unsigned char> vsBytes, psBytes;
  if (!bltShaderContainers(vsBytes, psBytes)) return E_FAIL;
  ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11PixelShader> ps;
  HRESULT hr = device->CreateVertexShader(vsBytes.data(), vsBytes.size(), nullptr, &vs);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  hr = device->CreatePixelShader(psBytes.data(), psBytes.size(), nullptr, &ps);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  const D3D11_INPUT_ELEMENT_DESC elements[] = {
    {inputRegisterSemantic, 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {inputRegisterSemantic, 1, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}};
  ComPtr<ID3D11InputLayout> layout;
  hr = device->CreateInputLayout(elements, 2, vsBytes.data(), vsBytes.size(), &layout);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  const auto vertices = bltVertices(rotation);
  D3D11_BUFFER_DESC vbDesc{}; vbDesc.ByteWidth = sizeof(vertices);
  vbDesc.Usage = D3D11_USAGE_IMMUTABLE; vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA data{vertices.data(), 0, 0}; ComPtr<ID3D11Buffer> vb;
  hr = device->CreateBuffer(&vbDesc, &data, &vb);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  D3D11_TEXTURE2D_DESC scratchDesc{};
  scratchDesc.Width = plan.sourceWidth; scratchDesc.Height = plan.sourceHeight;
  scratchDesc.MipLevels = scratchDesc.ArraySize = scratchDesc.SampleDesc.Count = 1;
  scratchDesc.Format = plan.sourceFormat; scratchDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  ComPtr<ID3D11Texture2D> sampled, rendered;
  hr = device->CreateTexture2D(&scratchDesc, nullptr, &sampled);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  scratchDesc.Width = plan.width; scratchDesc.Height = plan.height;
  scratchDesc.Format = plan.destinationFormat; scratchDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
  hr = device->CreateTexture2D(&scratchDesc, nullptr, &rendered);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  ComPtr<ID3D11ShaderResourceView> srv; ComPtr<ID3D11RenderTargetView> rtv;
  hr = device->CreateShaderResourceView(sampled.Get(), nullptr, &srv);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  hr = device->CreateRenderTargetView(rendered.Get(), nullptr, &rtv);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  D3D11_SAMPLER_DESC samplerDesc{};
  samplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
  samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.MaxAnisotropy = 1;
  samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  ComPtr<ID3D11SamplerState> sampler;
  hr = device->CreateSamplerState(&samplerDesc, &sampler);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  D3D11_RASTERIZER_DESC rasterDesc{};
  rasterDesc.FillMode = D3D11_FILL_SOLID; rasterDesc.CullMode = D3D11_CULL_NONE;
  rasterDesc.DepthClipEnable = TRUE; ComPtr<ID3D11RasterizerState> raster;
  hr = device->CreateRasterizerState(&rasterDesc, &raster);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  ComPtr<ID3D11DeviceContext> commands;
  hr = device->CreateDeferredContext(0, &commands);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  if (!vs || !ps || !layout || !vb || !sampled || !rendered || !srv || !rtv || !sampler || !raster || !commands)
    return E_FAIL;
  if (plan.resolve) commands->ResolveSubresource(sampled.Get(), 0, source, sourceSubresource, plan.sourceFormat);
  else commands->CopySubresourceRegion(sampled.Get(), 0, 0, 0, 0, source, sourceSubresource, nullptr);
  ID3D11Buffer* buffers[] = {vb.Get()}; const UINT stride = sizeof(BltVertex), offset = 0;
  commands->IASetInputLayout(layout.Get()); commands->IASetVertexBuffers(0, 1, buffers, &stride, &offset);
  commands->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commands->VSSetShader(vs.Get(), nullptr, 0); commands->PSSetShader(ps.Get(), nullptr, 0);
  ID3D11ShaderResourceView* views[] = {srv.Get()}; ID3D11SamplerState* samplers[] = {sampler.Get()};
  commands->PSSetShaderResources(0, 1, views); commands->PSSetSamplers(0, 1, samplers);
  ID3D11RenderTargetView* targets[] = {rtv.Get()}; commands->OMSetRenderTargets(1, targets, nullptr);
  commands->RSSetState(raster.Get());
  const D3D11_VIEWPORT viewport{0, 0, FLOAT(plan.width), FLOAT(plan.height), 0, 1};
  commands->RSSetViewports(1, &viewport); commands->Draw(3, 0);
  commands->CopySubresourceRegion(destination, destinationSubresource, left, top, 0, rendered.Get(), 0, nullptr);
  ComPtr<ID3D11CommandList> list;
  hr = commands->FinishCommandList(FALSE, &list);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  if (!list) return E_FAIL;
  context->ExecuteCommandList(list.Get(), TRUE);
  return live() ? device->GetDeviceRemovedReason() : DXGI_ERROR_DEVICE_REMOVED;
}
}
