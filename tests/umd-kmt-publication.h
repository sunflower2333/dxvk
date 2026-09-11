#pragma once

#include "../src/umd/umd_allocation.h"
#include <d3dkmthk.h>
#include <cstdio>
#include <cstring>

// Device-only harness, never linked into viogpudxvk.dll. Allocation, context
// and CPU access use actual KMT operations. The final Present callback checks
// allocation contents without changing the screen or claiming DXGI activation.
class KmtPublication {
public:
  KmtPublication() = default;
  KmtPublication(const KmtPublication&) = delete;
  KmtPublication& operator=(const KmtPublication&) = delete;
  ~KmtPublication() { close(); }

  HRESULT open(D3DKMT_HANDLE adapter) {
    if (m_device || !adapter) return E_INVALIDARG;
    D3DKMT_CREATEDEVICE request = {}; request.hAdapter = adapter;
    HRESULT hr = result(D3DKMTCreateDevice(&request));
    if (SUCCEEDED(hr)) m_device = request.hDevice;
    return SUCCEEDED(hr) && !m_device ? E_FAIL : hr;
  }
  D3D10DDI_HRTRESOURCE runtimeResource() { return {&m_resourceCookie}; }
  void initialize(D3D10DDIARG_CREATEDEVICE& args, D3DDDI_DEVICECALLBACKS& kernel,
      DXGI_DDI_BASE_CALLBACKS& dxgi, DXGI_DDI_BASE_FUNCTIONS& table) {
    args.hRTDevice.handle = this;
    kernel.pfnAllocateCb = allocate; kernel.pfnDeallocateCb = deallocate;
    kernel.pfnLockCb = lock; kernel.pfnUnlockCb = unlock;
    kernel.pfnCreateContextCb = createContext; kernel.pfnDestroyContextCb = destroyContext;
    dxgi.pfnPresentCb = verifyPublished;
    args.DXGIBaseDDI.pDXGIBaseCallbacks = &dxgi;
    args.DXGIBaseDDI.pDXGIDDIBaseFunctions = &table;
  }
  bool verified() const { return m_verified; }
  HRESULT close() {
    HRESULT hr = S_OK;
    if (m_context) {
      D3DKMT_DESTROYCONTEXT request = {}; request.hContext = m_context;
      hr = result(D3DKMTDestroyContext(&request)); m_context = 0;
    }
    if (m_resource) {
      D3DKMT_DESTROYALLOCATION request = {};
      request.hDevice = m_device; request.hResource = m_resource;
      HRESULT released = result(D3DKMTDestroyAllocation(&request));
      if (FAILED(released)) hr = released;
      m_resource = 0; m_allocation = 0;
    }
    if (m_device) {
      D3DKMT_DESTROYDEVICE request = {}; request.hDevice = m_device;
      HRESULT released = result(D3DKMTDestroyDevice(&request));
      if (FAILED(released)) hr = released;
      m_device = 0;
    }
    return hr;
  }
private:
  static HRESULT result(NTSTATUS status) { return status < 0 ? HRESULT_FROM_NT(status) : S_OK; }
  static KmtPublication* self(HANDLE device) { return static_cast<KmtPublication*>(device); }
  static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* args) {
    auto s = self(device);
    if (!s || !s->m_device || s->m_resource || !args || args->hResource != &s->m_resourceCookie
        || args->NumAllocations != 1 || !args->pAllocationInfo) return E_INVALIDARG;
    auto& allocation = args->pAllocationInfo[0];
    if (!allocation.pPrivateDriverData || allocation.PrivateDriverDataSize != sizeof(dxvk::umd::AllocationInfo))
      return E_INVALIDARG;
    dxvk::umd::AllocationInfo info;
    std::memcpy(&info, allocation.pPrivateDriverData, sizeof(info));
    if (info.width != 64 || info.height != 64 || info.pitch != 256 || info.format != 3
        || info.flags != 2 || info.size != 16384) return E_INVALIDARG;
    D3DKMT_CREATEALLOCATION request = {};
    request.hDevice = s->m_device; request.NumAllocations = 1;
    request.pAllocationInfo = args->pAllocationInfo;
    request.Flags.CreateResource = 1;
    HRESULT hr = result(D3DKMTCreateAllocation(&request));
    if (SUCCEEDED(hr)) {
      s->m_resource = request.hResource; s->m_allocation = allocation.hAllocation;
      args->hKMResource = request.hResource;
    }
    return hr;
  }
  static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
    auto s = self(device);
    if (!s || !args || args->hResource != &s->m_resourceCookie || !s->m_resource
        || args->NumAllocations || args->HandleList) return E_INVALIDARG;
    D3DKMT_DESTROYALLOCATION request = {};
    request.hDevice = s->m_device; request.hResource = s->m_resource;
    HRESULT hr = result(D3DKMTDestroyAllocation(&request));
    if (SUCCEEDED(hr)) { s->m_resource = 0; s->m_allocation = 0; }
    return hr;
  }
  static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* args) {
    auto s = self(device);
    if (!s || !args || !s->m_allocation || args->hAllocation != s->m_allocation) return E_INVALIDARG;
    D3DKMT_LOCK request = {};
    request.hDevice = s->m_device; request.hAllocation = args->hAllocation;
    request.PrivateDriverData = args->PrivateDriverData;
    request.NumPages = args->NumPages; request.pPages = args->pPages; request.Flags = args->Flags;
    HRESULT hr = result(D3DKMTLock(&request));
    if (SUCCEEDED(hr)) { args->hAllocation = request.hAllocation; args->pData = request.pData; }
    return hr;
  }
  static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* args) {
    auto s = self(device);
    if (!s || !args || args->NumAllocations != 1 || !args->phAllocations
        || *args->phAllocations != s->m_allocation) return E_INVALIDARG;
    D3DKMT_UNLOCK request = {};
    request.hDevice = s->m_device;
    request.NumAllocations = args->NumAllocations; request.phAllocations = args->phAllocations;
    return result(D3DKMTUnlock(&request));
  }
  static HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* args) {
    auto s = self(device);
    if (!s || !s->m_device || s->m_context || !args) return E_INVALIDARG;
    D3DKMT_CREATECONTEXT request = {};
    request.hDevice = s->m_device; request.NodeOrdinal = args->NodeOrdinal;
    request.EngineAffinity = args->EngineAffinity; request.Flags = args->Flags;
    request.pPrivateDriverData = args->pPrivateDriverData; request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    HRESULT hr = result(D3DKMTCreateContext(&request));
    if (SUCCEEDED(hr)) {
      s->m_context = request.hContext;
      if (!s->m_context) return E_FAIL;
      args->hContext = &s->m_context;
      args->pCommandBuffer = request.pCommandBuffer; args->CommandBufferSize = request.CommandBufferSize;
      args->pAllocationList = request.pAllocationList; args->AllocationListSize = request.AllocationListSize;
      args->pPatchLocationList = request.pPatchLocationList; args->PatchLocationListSize = request.PatchLocationListSize;
    }
    return hr;
  }
  static HRESULT APIENTRY destroyContext(HANDLE device, const D3DDDICB_DESTROYCONTEXT* args) {
    auto s = self(device);
    if (!s || !args || !s->m_context || args->hContext != &s->m_context) return E_INVALIDARG;
    D3DKMT_DESTROYCONTEXT request = {}; request.hContext = s->m_context;
    HRESULT hr = result(D3DKMTDestroyContext(&request));
    if (SUCCEEDED(hr)) s->m_context = 0;
    return hr;
  }
  static HRESULT APIENTRY verifyPublished(HANDLE device, DXGIDDICB_PRESENT* args) {
    auto s = self(device);
    if (!s || !args || !s->m_allocation || args->hSrcAllocation != s->m_allocation
        || args->hDstAllocation || args->hContext != &s->m_context
        || !s->m_context || args->pDXGIContext != s) return E_INVALIDARG;
    s->m_verified = false;
    D3DDDICB_LOCK mapped = {}; mapped.hAllocation = s->m_allocation;
    mapped.Flags.ReadOnly = 1; mapped.Flags.LockEntire = 1;
    HRESULT hr = lock(device, &mapped);
    if (FAILED(hr)) return hr;
    unsigned mismatches = 4096;
    if (mapped.pData) {
      mismatches = 0;
      const unsigned char red[4] = {255,0,0,255};
      for (unsigned i = 0; i < 4096; i++)
        mismatches += std::memcmp(static_cast<const unsigned char*>(mapped.pData) + i*4, red, 4) != 0;
    }
    D3DDDICB_UNLOCK unmap = {}; unmap.NumAllocations = 1; unmap.phAllocations = &mapped.hAllocation;
    hr = unlock(device, &unmap);
    s->m_verified = SUCCEEDED(hr) && !mismatches;
    std::printf("KMT_ALLOCATION_PUBLICATION %s pixels=4096 mismatches=%u (no screen Present)\n",
      s->m_verified ? "PASS" : "FAIL", mismatches);
    return FAILED(hr) ? hr : mismatches ? E_FAIL : S_OK;
  }
  D3DKMT_HANDLE m_device = 0, m_resource = 0, m_allocation = 0, m_context = 0;
  char m_resourceCookie = 0;
  bool m_verified = false;
};
