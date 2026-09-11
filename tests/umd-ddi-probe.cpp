#include "../src/umd/umd_ddi.h"
#include <d3dkmthk.h>
#include "umd-probe-shaders.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

static HRESULT lastError = S_OK;
static void APIENTRY setError(D3D10DDI_HRTCORELAYER, HRESULT error) { lastError = error; }
using Memory = std::unique_ptr<void, decltype(&std::free)>;
static Memory allocate(size_t size) { return Memory(std::calloc(1, size), &std::free); }

// Explicit development harness: this callback reads the real KMD's private
// data, but is not supplied by the Microsoft D3D runtime. No runtime handle
// is ever cast to a KMT handle.
struct AdapterSession {
  D3DKMT_HANDLE kmt = 0;
  D3D10DDI_HADAPTER adapter = {};
  D3D10DDI_ADAPTERFUNCS functions = {};
  ~AdapterSession() {
    if (adapter.pDrvPrivate) functions.pfnCloseAdapter(adapter);
    if (kmt) {
      D3DKMT_CLOSEADAPTER close = {}; close.hAdapter = kmt;
      D3DKMTCloseAdapter(&close);
    }
  }
  static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* args) {
    if (!runtime || !args) return E_INVALIDARG;
    auto self = static_cast<AdapterSession*>(runtime);
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = self->kmt;
    query.Type = KMTQAITYPE_UMDRIVERPRIVATE;
    query.pPrivateDriverData = args->pPrivateDriverData;
    query.PrivateDriverDataSize = args->PrivateDriverDataSize;
    const NTSTATUS status = D3DKMTQueryAdapterInfo(&query);
    return status < 0 ? HRESULT_FROM_NT(status) : S_OK;
  }
  HRESULT open(const LUID& luid) {
    D3DKMT_OPENADAPTERFROMLUID kmtOpen = {};
    kmtOpen.AdapterLuid = luid;
    const NTSTATUS status = D3DKMTOpenAdapterFromLuid(&kmtOpen);
    if (status < 0) return HRESULT_FROM_NT(status);
    kmt = kmtOpen.hAdapter;
    D3DDDI_ADAPTERCALLBACKS callbacks = {};
    callbacks.pfnQueryAdapterInfoCb = query;
    D3D10DDIARG_OPENADAPTER args = {};
    args.hRTAdapter.handle = this;
    args.Interface = D3D10_0_DDI_INTERFACE_VERSION;
    args.Version = D3D10_0_DDI_BUILD_VERSION << 16;
    args.pAdapterCallbacks = &callbacks; args.pAdapterFuncs = &functions;
    const HRESULT hr = VioGpuDxvkOpenAdapterForTest(&args);
    if (SUCCEEDED(hr)) adapter = args.hAdapter;
    return hr;
  }
};

int main(int argc, char** argv) {
  LUID luid = {};
  const bool adapterMode = argc == 3 && std::strcmp(argv[2], "--adapter") == 0;
  if ((argc != 2 && !adapterMode) || std::strlen(argv[1]) != 16) return 2;
  for (size_t i = 0; i < sizeof(luid); i++) {
    char value[3] = {argv[1][2*i], argv[1][2*i+1], 0};
    char* end = nullptr;
    const auto byte = std::strtoul(value, &end, 16);
    if (end != value + 2 || byte > 255) return 2;
    reinterpret_cast<unsigned char*>(&luid)[i] = static_cast<unsigned char>(byte);
  }
  AdapterSession session;
  size_t deviceSize = VioGpuDxvkPrivateDeviceSize();
  if (adapterMode) {
    const HRESULT hr = session.open(luid);
    std::printf("KMD_ADAPTER_HARNESS hr=%08lx (not Microsoft runtime activation)\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return 10;
    D3D10DDIARG_CALCPRIVATEDEVICESIZE args = {
      D3D10_0_DDI_INTERFACE_VERSION, D3D10_0_DDI_BUILD_VERSION << 16, 0};
    deviceSize = session.functions.pfnCalcPrivateDeviceSize(session.adapter, &args);
    if (!deviceSize) return 11;
  }
  auto deviceMemory = allocate(deviceSize);
  if (!deviceMemory) return 3;
  D3D10DDI_HDEVICE device = {deviceMemory.get()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS callbacks = {};
  callbacks.pfnSetErrorCb = setError;
  D3D10DDI_DEVICEFUNCS table = {};
  HRESULT hr;
  if (adapterMode) {
    D3DDDI_DEVICECALLBACKS kernel = {};
    D3D10DDIARG_CREATEDEVICE args = {};
    args.Interface = D3D10_0_DDI_INTERFACE_VERSION;
    args.Version = D3D10_0_DDI_BUILD_VERSION << 16;
    args.hRTDevice.handle = &session; args.pKTCallbacks = &kernel;
    args.hRTCoreLayer.handle = &session; args.pUMCallbacks = &callbacks;
    args.hDrvDevice = device; args.pDeviceFuncs = &table;
    hr = session.functions.pfnCreateDevice(session.adapter, &args);
  } else {
    hr = VioGpuDxvkCreateDdiTestDevice(&luid, device, {}, &callbacks, &table);
  }
  std::printf("DDI_CREATE hr=%08lx\n", static_cast<unsigned long>(hr));
  if (FAILED(hr)) return 4;
  D3D10DDI_MIPINFO mip = {64,64,1,64,64,1};
  D3D10DDIARG_CREATERESOURCE desc = {};
  desc.pMipInfoList = &mip;
  desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  desc.Usage = D3D10_DDI_USAGE_DEFAULT;
  desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1; desc.MipLevels = 1; desc.ArraySize = 1;
  auto targetMemory = allocate(table.pfnCalcPrivateResourceSize(device, &desc));
  auto stagingMemory = allocate(table.pfnCalcPrivateResourceSize(device, &desc));
  if (!targetMemory || !stagingMemory) { table.pfnDestroyDevice(device); return 5; }
  D3D10DDI_HRESOURCE target = {targetMemory.get()}, staging = {stagingMemory.get()};
  table.pfnCreateResource(device, &desc, target, {});
  desc.Usage = D3D10_DDI_USAGE_STAGING; desc.BindFlags = 0;
  desc.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  table.pfnCreateResource(device, &desc, staging, {});
  D3D10DDIARG_CREATERENDERTARGETVIEW viewDesc = {};
  viewDesc.hDrvResource = target; viewDesc.Format = desc.Format;
  viewDesc.ResourceDimension = desc.ResourceDimension;
  viewDesc.Tex2D.ArraySize = 1;
  auto viewMemory = allocate(table.pfnCalcPrivateRenderTargetViewSize(device, &viewDesc));
  if (!viewMemory) return 6;
  D3D10DDI_HRENDERTARGETVIEW view = {viewMemory.get()};
  table.pfnCreateRenderTargetView(device, &viewDesc, view, {});
  std::vector<uint32_t> vs, ps;
  if (!compileProbeShader(true, vs) || !compileProbeShader(false, ps)) return 8;
  D3D10DDIARG_SIGNATURE_ENTRY input = {D3D10_SB_NAME_VERTEX_ID,0,1};
  D3D10DDIARG_SIGNATURE_ENTRY position = {D3D10_SB_NAME_POSITION,0,15};
  D3D10DDIARG_SIGNATURE_ENTRY colorOutput = {D3D10_SB_NAME_UNDEFINED,0,15};
  D3D10DDIARG_STAGE_IO_SIGNATURES vsSignature = {&input,1,&position,1};
  D3D10DDIARG_STAGE_IO_SIGNATURES psSignature = {nullptr,0,&colorOutput,1};
  auto vsMemory = allocate(table.pfnCalcPrivateShaderSize(device, vs.data(), &vsSignature));
  auto psMemory = allocate(table.pfnCalcPrivateShaderSize(device, ps.data(), &psSignature));
  D3D10_DDI_RASTERIZER_DESC rasterDesc = {};
  rasterDesc.FillMode = D3D10_DDI_FILL_SOLID;
  rasterDesc.CullMode = D3D10_DDI_CULL_NONE;
  rasterDesc.DepthClipEnable = TRUE;
  auto rasterMemory = allocate(table.pfnCalcPrivateRasterizerStateSize(device, &rasterDesc));
  if (!vsMemory || !psMemory || !rasterMemory) return 9;
  D3D10DDI_HSHADER vertex = {vsMemory.get()}, pixel = {psMemory.get()};
  D3D10DDI_HRASTERIZERSTATE raster = {rasterMemory.get()};
  table.pfnCreateVertexShader(device, vs.data(), vertex, {}, &vsSignature);
  table.pfnCreatePixelShader(device, ps.data(), pixel, {}, &psSignature);
  table.pfnCreateRasterizerState(device, &rasterDesc, raster, {});
  unsigned mismatches = 4096;
  if (SUCCEEDED(lastError)) {
    FLOAT color[4] = {0,1,0,1};
    table.pfnClearRenderTargetView(device, view, color);
    table.pfnVsSetShader(device, vertex);
    table.pfnPsSetShader(device, pixel);
    table.pfnSetRasterizerState(device, raster);
    table.pfnSetRenderTargets(device, &view, 1, 0, {});
    D3D10_DDI_VIEWPORT viewport = {0,0,64,64,0,1};
    table.pfnSetViewports(device, 1, 0, &viewport);
    table.pfnIaSetTopology(device, D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    table.pfnDraw(device, 3, 0);
    table.pfnResourceCopy(device, staging, target);
    D3D10DDI_MAPPED_SUBRESOURCE mapped = {};
    table.pfnStagingResourceMap(device, staging, 0, D3D10_DDI_MAP_READ, 0, &mapped);
    if (SUCCEEDED(lastError) && mapped.pData && mapped.RowPitch >= 256) {
      mismatches = 0;
      const unsigned char expected[4] = {255,0,0,255};
      for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++)
          mismatches += std::memcmp(static_cast<unsigned char*>(mapped.pData) + y*mapped.RowPitch + x*4, expected, 4) != 0;
      table.pfnStagingResourceUnmap(device, staging, 0);
    }
  }
  table.pfnDestroyRasterizerState(device, raster);
  table.pfnDestroyShader(device, pixel);
  table.pfnDestroyShader(device, vertex);
  table.pfnDestroyRenderTargetView(device, view);
  table.pfnDestroyResource(device, staging);
  table.pfnDestroyResource(device, target);
  table.pfnDestroyDevice(device);
  std::printf("DDI_DRAW_PIXELS %s pixels=4096 mismatches=%u error=%08lx\n",
    !mismatches && SUCCEEDED(lastError) ? "PASS" : "FAIL", mismatches, static_cast<unsigned long>(lastError));
  return !mismatches && SUCCEEDED(lastError) ? 0 : 7;
}
