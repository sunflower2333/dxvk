#pragma once
// SPDX-License-Identifier: MIT
#include "umd_allocation.h"
#include "umd_shared_policy.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

namespace dxvk::umd {

// A D3D resource that more than one process can name.
//
// The embedded renderer allocates its images from Turnip through the native
// context; the kernel allocation that other processes open is a separate,
// CPU-visible linear buffer described by AllocationInfo. Nothing imports one
// into the other, so a shared surface is a pair: the renderable cache the
// pipeline actually draws with, and the kernel allocation that carries its
// pixels across the process boundary. Ownership transitions copy between them
// -- refresh reads the allocation into the cache, publish writes the cache
// back out.
//
// This is the design the Mesa UMD already ships on this stack, and it is
// chosen for the same reason: no Vulkan external-memory path exists here that
// could alias a guest KMT allocation as a Turnip image. It is deliberately not
// zero copy.
//
// What is NOT inherited is Mesa's refresh policy. There, a read-only shared
// surface is refreshed before every draw that binds it, because nothing
// records that the previous draw already refreshed it -- so compositing N
// windows with M draws each performs N*M full-surface reads per frame. At
// 3040x1904 that path measures 17.35 ms p50 against a 6.06 ms frame budget.
//
// Here a refresh is memoized against an ownership epoch (see sharedEpoch).
// Within one epoch a surface is read at most once however many draws bind it,
// which turns the per-frame cost from per-draw into per-surface.
//
// The correctness argument for memoizing: a plain D3D shared resource carries
// no cross-process synchronization contract at all. Callers that need one use
// a keyed mutex, and the D3D10 DDI has no flag for that -- 0x100 is reserved,
// never passed -- so CreateResource rejects everything except plain SHARED and
// this bridge never promises coherence it cannot deliver. Sampling a snapshot
// taken at the last epoch boundary is exactly what the contract allows, and is
// what a compositor wants in any case: a frame composed from surfaces that all
// changed underneath it mid-draw would tear per window.
struct SharedSurface {
  RuntimeAllocation allocation;
  // The renderable copy. This is the same object as the owning Resource's
  // backend; the surface holds it so a view can outlive the resource DDI.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> cache;
  // CPU-visible transfer buffer, created on first use. The cache itself can
  // never be mapped: it is a tiled Turnip image, and a tiled staging read
  // returns zeros rather than pixels.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
  // When this cache last read the allocation and whether it owes it a write.
  // The rules live in umd_shared_policy.h, where they are tested without a
  // device; this struct only carries them alongside the D3D objects.
  SharedState state;
};

// Lazily create the transfer buffer matching the cache.
inline HRESULT sharedStaging(ID3D11Device* device, SharedSurface& surface) {
  if (surface.staging) return S_OK;
  if (!device || !surface.cache) return E_INVALIDARG;
  D3D11_TEXTURE2D_DESC desc = {};
  surface.cache->GetDesc(&desc);
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
  return device->CreateTexture2D(&desc, nullptr, &surface.staging);
}

// One body for both directions, for the same reason RuntimeMemory has one:
// the staging hop, the synchronized map that acts as the GPU barrier and the
// balancing unmap are identical, and only the order of the two copies differs.
inline HRESULT transferSharedSurface(ID3D11Device* device, ID3D11DeviceContext* context,
    RuntimeMemory& memory, SharedSurface& surface, bool publish) {
  if (!device || !context || !surface.cache || !surface.allocation.handle())
    return E_INVALIDARG;
  HRESULT hr = sharedStaging(device, surface);
  if (FAILED(hr)) return hr;
  if (publish) context->CopyResource(surface.staging.Get(), surface.cache.Get());
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  // The synchronized map is the completion barrier in both directions: it
  // waits for the copy just queued, or for the pipeline to finish reading the
  // staging buffer that the previous refresh left in flight. STAGING cannot
  // take MAP_WRITE_DISCARD, so this wait is structural; a second staging
  // buffer would hide it and is the obvious next optimization, not a fix.
  hr = context->Map(surface.staging.Get(), 0,
    publish ? D3D11_MAP_READ : D3D11_MAP_WRITE, 0, &mapped);
  if (FAILED(hr)) return hr;
  hr = publish ? memory.upload(surface.allocation, mapped.pData, mapped.RowPitch)
               : memory.download(surface.allocation, mapped.pData, mapped.RowPitch);
  context->Unmap(surface.staging.Get(), 0);
  if (FAILED(hr)) return hr;
  if (!publish) context->CopyResource(surface.cache.Get(), surface.staging.Get());
  return S_OK;
}

// Read the owning process's pixels into the cache, at most once per epoch.
inline HRESULT refreshSharedSurface(ID3D11Device* device, ID3D11DeviceContext* context,
    RuntimeMemory& memory, SharedSurface& surface, uint64_t epoch) {
  if (!sharedNeedsRefresh(surface.state, epoch)) {
    sharedRefreshed(surface.state, epoch);
    return S_OK;
  }
  const HRESULT hr = transferSharedSurface(device, context, memory, surface, false);
  if (SUCCEEDED(hr)) sharedRefreshed(surface.state, epoch);
  return hr;
}

// Write this device's pixels back out. A no-op unless the cache is dirty.
inline HRESULT publishSharedSurface(ID3D11Device* device, ID3D11DeviceContext* context,
    RuntimeMemory& memory, SharedSurface& surface, uint64_t epoch) {
  if (!surface.state.dirty) return S_OK;
  const HRESULT hr = transferSharedSurface(device, context, memory, surface, true);
  if (SUCCEEDED(hr)) sharedPublished(surface.state, epoch);
  return hr;
}

// Drop the memoized read after re-pairing this surface with a different
// allocation. See sharedInvalidate in umd_shared_policy.h for why a rotation
// that skips this shows the previous frame, and why callers must publish
// before they rotate rather than after.
inline void invalidateSharedSurface(SharedSurface& surface) {
  sharedInvalidate(surface.state);
}

}
