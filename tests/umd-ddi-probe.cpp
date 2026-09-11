#include "../src/umd/umd_ddi.h"
#include <d3dkmthk.h>
#include "umd-probe-shaders.h"
#include "umd-kmt-publication.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <array>

static HRESULT lastError = S_OK;
static void APIENTRY setError(D3D10DDI_HRTCORELAYER, HRESULT error) { lastError = error; }
using Memory = std::unique_ptr<void, decltype(&std::free)>;
static Memory allocate(size_t size) { return Memory(std::calloc(1, size), &std::free); }

struct ProbeResource {
  D3D10DDI_HDEVICE device;
  const D3D10DDI_DEVICEFUNCS& functions;
  Memory memory;
  D3D10DDI_HRESOURCE handle;
  ProbeResource(D3D10DDI_HDEVICE device, const D3D10DDI_DEVICEFUNCS& functions,
      const D3D10DDIARG_CREATERESOURCE& desc)
  : device(device), functions(functions),
    memory(allocate(functions.pfnCalcPrivateResourceSize(device, &desc))), handle{memory.get()} {
    if (handle.pDrvPrivate) functions.pfnCreateResource(device, &desc, handle, {});
  }
  ~ProbeResource() { if (handle.pDrvPrivate) functions.pfnDestroyResource(device, handle); }
  ProbeResource(const ProbeResource&) = delete;
  ProbeResource& operator=(const ProbeResource&) = delete;
};

static bool testBufferTransfers(D3D10DDI_HDEVICE device, const D3D10DDI_DEVICEFUNCS& functions) {
  std::array<unsigned char, 64> zeros = {};
  D3D10DDI_MIPINFO mip = {64,1,1,64,1,1};
  D3D10_DDIARG_SUBRESOURCE_UP initial = {zeros.data(),64,64};
  D3D10DDIARG_CREATERESOURCE desc = {};
  desc.pMipInfoList = &mip; desc.pInitialDataUP = &initial;
  desc.ResourceDimension = D3D10DDIRESOURCE_BUFFER; desc.Usage = D3D10_DDI_USAGE_DEFAULT;
  desc.SampleDesc.Count = 1; desc.MipLevels = 1; desc.ArraySize = 1;
  ProbeResource source(device, functions, desc);
  desc.Usage = D3D10_DDI_USAGE_STAGING; desc.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  ProbeResource destination(device, functions, desc);
  if (!source.memory || !destination.memory || FAILED(lastError)) return false;
  std::array<unsigned char, 16> pattern;
  for (size_t i = 0; i < pattern.size(); i++) pattern[i] = static_cast<unsigned char>(0x70+i);
  D3D10_DDI_BOX box = {4,0,0,20,1,1};
  functions.pfnResourceUpdateSubresourceUP(device, source.handle, 0, &box, pattern.data(), 0, 0);
  functions.pfnResourceCopyRegion(device, destination.handle, 0, 8, 0, 0, source.handle, 0, &box);
  if (FAILED(lastError)) return false;
  const BOOL invalidBusy = functions.pfnResourceIsStagingBusy(device, source.handle);
  if (!invalidBusy || lastError != E_INVALIDARG) return false;
  lastError = S_OK;
  const BOOL before = functions.pfnResourceIsStagingBusy(device, destination.handle);
  if (FAILED(lastError)) return false;
  D3D10DDI_MAPPED_SUBRESOURCE mapped = {};
  functions.pfnStagingResourceMap(device, destination.handle, 0, D3D10_DDI_MAP_READ, 0, &mapped);
  if (FAILED(lastError) || !mapped.pData) return false;
  const BOOL whileMapped = functions.pfnResourceIsStagingBusy(device, destination.handle);
  unsigned mismatches = 0;
  auto bytes = static_cast<const unsigned char*>(mapped.pData);
  for (size_t i = 0; i < zeros.size(); i++)
    mismatches += bytes[i] != (i >= 8 && i < 24 ? pattern[i-8] : 0);
  functions.pfnStagingResourceUnmap(device, destination.handle, 0);
  std::printf("DDI_BUFFER_COPY bytes=64 mismatches=%u\n", mismatches);
  std::printf("DDI_STAGING_BUSY buffer_pre=%u mapped=%u invalid_rejected=1\n", UINT(before), UINT(whileMapped));
  return !mismatches && !whileMapped && SUCCEEDED(lastError);
}

static bool testMultisample(D3D10DDI_HDEVICE device, const D3D10DDI_DEVICEFUNCS& functions) {
  constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
  UINT support = 0, levels = 0;
  functions.pfnCheckFormatSupport(device, format, &support);
  const UINT required = D3D10_DDI_FORMAT_SUPPORT_RENDERTARGET
    | D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET | D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_LOAD;
  if (FAILED(lastError) || (support & required) != required) return false;
  functions.pfnCheckMultisampleQualityLevels(device, format, 1, &levels);
  if (FAILED(lastError) || levels != 1) return false;
  functions.pfnCheckMultisampleQualityLevels(device, format, 0, &levels);
  if (FAILED(lastError) || levels) return false;
  functions.pfnCheckMultisampleQualityLevels(device, format, 33, &levels);
  if (FAILED(lastError) || levels) return false;
  functions.pfnCheckMultisampleQualityLevels(device, format, 4, &levels);
  if (FAILED(lastError) || !levels) return false;

  D3D10DDI_MIPINFO msMip = {8,8,1,8,8,1};
  D3D10DDIARG_CREATERESOURCE desc = {};
  desc.pMipInfoList = &msMip; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  desc.Usage = D3D10_DDI_USAGE_DEFAULT; desc.Format = format;
  desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_SHADER_RESOURCE;
  desc.MipLevels = 1; desc.ArraySize = 2; desc.SampleDesc.Count = 4;
  ProbeResource source(device, functions, desc);
  if (!source.memory || FAILED(lastError)) return false;
  D3D10DDIARG_CREATESHADERRESOURCEVIEW srv = {};
  srv.hDrvResource = source.handle; srv.Format = format; srv.ResourceDimension = desc.ResourceDimension;
  srv.Tex2D.FirstArraySlice = 1; srv.Tex2D.ArraySize = 1; srv.Tex2D.MipLevels = 1;
  auto srvMemory = allocate(functions.pfnCalcPrivateShaderResourceViewSize(device, &srv));
  if (!srvMemory) return false;
  D3D10DDI_HSHADERRESOURCEVIEW shaderView = {srvMemory.get()};
  functions.pfnCreateShaderResourceView(device, &srv, shaderView, {});
  functions.pfnDestroyShaderResourceView(device, shaderView);
  if (FAILED(lastError)) return false;
  D3D10DDIARG_CREATERENDERTARGETVIEW viewDesc = {};
  viewDesc.hDrvResource = source.handle; viewDesc.Format = format;
  viewDesc.ResourceDimension = desc.ResourceDimension; viewDesc.Tex2D.ArraySize = 1;
  auto viewMemory = allocate(functions.pfnCalcPrivateRenderTargetViewSize(device, &viewDesc));
  if (!viewMemory) return false;
  D3D10DDI_HRENDERTARGETVIEW view = {viewMemory.get()};
  FLOAT colors[2][4] = {{0,0,1,1}, {1,0,0,1}};
  for (UINT i = 0; i < 2; i++) {
    viewDesc.Tex2D.FirstArraySlice = i;
    functions.pfnCreateRenderTargetView(device, &viewDesc, view, {});
    if (SUCCEEDED(lastError)) functions.pfnClearRenderTargetView(device, view, colors[i]);
    functions.pfnDestroyRenderTargetView(device, view);
    if (FAILED(lastError)) return false;
  }
  std::array<UINT,256> zeros = {};
  D3D10DDI_MIPINFO mips[2] = {{16,16,1,16,16,1},{8,8,1,8,8,1}};
  D3D10_DDIARG_SUBRESOURCE_UP initial[4] = {{zeros.data(),64,1024}, {zeros.data(),32,256},
    {zeros.data(),64,1024}, {zeros.data(),32,256}};
  desc.pMipInfoList = mips; desc.pInitialDataUP = initial;
  desc.MipLevels = 2; desc.SampleDesc.Count = 1; desc.BindFlags = 0;
  ProbeResource resolved(device, functions, desc);
  desc.Usage = D3D10_DDI_USAGE_STAGING; desc.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  desc.pInitialDataUP = nullptr;
  ProbeResource staging(device, functions, desc);
  if (!resolved.memory || !staging.memory || FAILED(lastError)) return false;
  functions.pfnResourceResolveSubresource(device, resolved.handle, 3, source.handle, 1, format);
  if (FAILED(lastError)) return false;
  // Invalid late range must not alter the valid destination or queue work.
  functions.pfnResourceResolveSubresource(device, resolved.handle, 4, source.handle, 0, format);
  if (lastError != E_INVALIDARG) return false;
  lastError = S_OK;
  functions.pfnResourceCopy(device, staging.handle, resolved.handle);
  if (FAILED(lastError)) return false;
  const BOOL before = functions.pfnResourceIsStagingBusy(device, staging.handle);
  if (FAILED(lastError)) return false;
  std::array<D3D10DDI_MAPPED_SUBRESOURCE,4> maps = {};
  UINT mappedCount = 0;
  for (; mappedCount < maps.size(); mappedCount++) {
    functions.pfnStagingResourceMap(device, staging.handle, mappedCount, D3D10_DDI_MAP_READ, 0, &maps[mappedCount]);
    if (FAILED(lastError) || !maps[mappedCount].pData) break;
  }
  if (mappedCount != maps.size()) {
    for (UINT i = 0; i < mappedCount; i++) functions.pfnStagingResourceUnmap(device, staging.handle, i);
    return false;
  }
  // Query all four simultaneously mapped subresources, without remapping any.
  const BOOL whileMapped = functions.pfnResourceIsStagingBusy(device, staging.handle);
  UINT mismatches = 0, pixelCount = 0;
  for (UINT i = 0; i < 4; i++) {
    const auto& map = maps[i];
    const UINT size = i & 1 ? 8 : 16;
    const UINT expected = i == 3 ? 0xff0000ff : 0;
    for (UINT y = 0; y < size; y++) {
      const auto row = reinterpret_cast<const UINT*>(static_cast<const char*>(map.pData) + y * map.RowPitch);
      for (UINT x = 0; x < size; x++) { pixelCount++; mismatches += row[x] != expected; }
    }
    functions.pfnStagingResourceUnmap(device, staging.handle, i);
  }
  std::printf("DDI_MSAA_RESOLVE samples=4 src_slice=1 dst_slice=1 dst_mip=1 pixels=%u mismatches=%u\n", pixelCount, mismatches);
  std::printf("DDI_STAGING_BUSY texture_pre=%u mapped=%u mapped_subresources=4\n", UINT(before), UINT(whileMapped));
  return !mismatches && !whileMapped && SUCCEEDED(lastError);
}

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
  const bool nativeCopy = argc == 3 && std::strcmp(argv[2], "--native-copy") == 0;
  const bool adapterMode = nativeCopy || (argc == 3 && std::strcmp(argv[2], "--adapter") == 0);
  if ((argc != 2 && !adapterMode) || std::strlen(argv[1]) != 16) return 2;
  for (size_t i = 0; i < sizeof(luid); i++) {
    char value[3] = {argv[1][2*i], argv[1][2*i+1], 0};
    char* end = nullptr;
    const auto byte = std::strtoul(value, &end, 16);
    if (end != value + 2 || byte > 255) return 2;
    reinterpret_cast<unsigned char*>(&luid)[i] = static_cast<unsigned char>(byte);
  }
  AdapterSession session;
  KmtPublication publication;
  size_t deviceSize = VioGpuDxvkPrivateDeviceSize();
  if (adapterMode) {
    const HRESULT hr = session.open(luid);
    std::printf("KMD_ADAPTER_HARNESS hr=%08lx (not Microsoft runtime activation)\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return 10;
    if (nativeCopy) {
      const HRESULT opened = publication.open(session.kmt);
      std::printf("KMT_PUBLICATION_DEVICE hr=%08lx\n", static_cast<unsigned long>(opened));
      if (FAILED(opened)) return 14;
    }
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
  DXGI_DDI_BASE_FUNCTIONS dxgiFunctions = {};
  HRESULT hr;
  if (adapterMode) {
    D3DDDI_DEVICECALLBACKS kernel = {};
    DXGI_DDI_BASE_CALLBACKS dxgi = {};
    D3D10DDIARG_CREATEDEVICE args = {};
    args.Interface = D3D10_0_DDI_INTERFACE_VERSION;
    args.Version = D3D10_0_DDI_BUILD_VERSION << 16;
    args.hRTDevice.handle = &session; args.pKTCallbacks = &kernel;
    args.hRTCoreLayer.handle = &session; args.pUMCallbacks = &callbacks;
    args.hDrvDevice = device; args.pDeviceFuncs = &table;
    if (nativeCopy) publication.initialize(args, kernel, dxgi, dxgiFunctions);
    hr = session.functions.pfnCreateDevice(session.adapter, &args);
  } else {
    hr = VioGpuDxvkCreateDdiTestDevice(&luid, device, {}, &callbacks, &table);
  }
  std::printf("DDI_CREATE hr=%08lx\n", static_cast<unsigned long>(hr));
  if (FAILED(hr)) return 4;
  if (!table.pfnResourceUpdateSubresourceUP || !table.pfnResourceCopyRegion
      || !table.pfnCalcPrivateQuerySize || !table.pfnCreateQuery || !table.pfnQueryEnd || !table.pfnQueryGetData
      || !table.pfnPsSetConstantBuffers || !table.pfnVsSetConstantBuffers || !table.pfnCalcPrivateShaderResourceViewSize
      || !table.pfnCreateShaderResourceView || !table.pfnDestroyShaderResourceView || !table.pfnPsSetShaderResources
      || !table.pfnCalcPrivateSamplerSize || !table.pfnCreateSampler || !table.pfnDestroySampler || !table.pfnPsSetSamplers
      || !table.pfnCalcPrivateBlendStateSize || !table.pfnCreateBlendState || !table.pfnDestroyBlendState
      || !table.pfnSetBlendState || !table.pfnIaSetIndexBuffer || !table.pfnDrawIndexed || !table.pfnSetScissorRects
      || !table.pfnCalcPrivateDepthStencilViewSize || !table.pfnCreateDepthStencilView
      || !table.pfnDestroyDepthStencilView || !table.pfnClearDepthStencilView || !table.pfnQueryBegin
      || !table.pfnCalcPrivateDepthStencilStateSize || !table.pfnCreateDepthStencilState
      || !table.pfnDestroyDepthStencilState || !table.pfnSetDepthStencilState
      || !table.pfnIaSetVertexBuffers || !table.pfnCalcPrivateElementLayoutSize
      || !table.pfnCreateElementLayout || !table.pfnDestroyElementLayout || !table.pfnIaSetInputLayout
      || !table.pfnDynamicIABufferMapDiscard || !table.pfnDynamicIABufferUnmap
      || !table.pfnResourceResolveSubresource || !table.pfnCheckFormatSupport
      || !table.pfnCheckMultisampleQualityLevels || !table.pfnResourceIsStagingBusy
      || !table.pfnResourceReadAfterWriteHazard || !table.pfnShaderResourceViewReadAfterWriteHazard) {
    std::fputs("Required development DDI absent; use the probe and DLL from one exact build\n", stderr);
    if (table.pfnDestroyDevice) table.pfnDestroyDevice(device);
    return 15;
  }
  if (!testBufferTransfers(device, table)) { table.pfnDestroyDevice(device); return 13; }
  if (!testMultisample(device, table)) { table.pfnDestroyDevice(device); return 17; }
  D3D10DDIARG_CREATEQUERY eventDesc = {D3D10DDI_QUERY_EVENT, 0};
  auto eventMemory = allocate(table.pfnCalcPrivateQuerySize(device, &eventDesc));
  if (!eventMemory) { table.pfnDestroyDevice(device); return 12; }
  D3D10DDI_HQUERY event = {eventMemory.get()};
  table.pfnCreateQuery(device, &eventDesc, event, {});
  D3D10DDI_MIPINFO mip = {64,64,1,64,64,1};
  D3D10DDIARG_CREATERESOURCE desc = {};
  desc.pMipInfoList = &mip;
  desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  desc.Usage = D3D10_DDI_USAGE_DEFAULT;
  desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET;
  if (nativeCopy) desc.BindFlags |= D3D10_DDI_BIND_PRESENT;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1; desc.MipLevels = 1; desc.ArraySize = 1;
  auto targetMemory = allocate(table.pfnCalcPrivateResourceSize(device, &desc));
  auto stagingMemory = allocate(table.pfnCalcPrivateResourceSize(device, &desc));
  if (!targetMemory || !stagingMemory) { table.pfnDestroyDevice(device); return 5; }
  D3D10DDI_HRESOURCE target = {targetMemory.get()}, staging = {stagingMemory.get()};
  table.pfnCreateResource(device, &desc, target, nativeCopy ? publication.runtimeResource() : D3D10DDI_HRTRESOURCE{});
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
  std::vector<dxvk::umd::ShaderSignatureEntry> vsInputs, vsOutputs, psInputs, psOutputs;
  if (!compileLinkageProbeShader(true, vs, vsInputs, vsOutputs) ||
      !compileLinkageProbeShader(false, ps, psInputs, psOutputs)) return 8;
  auto nativeSignature = [](const std::vector<dxvk::umd::ShaderSignatureEntry>& entries) {
    std::vector<D3D10DDIARG_SIGNATURE_ENTRY> result;
    for (const auto& entry : entries)
      result.push_back({static_cast<D3D10_SB_NAME>(entry.systemValue), entry.registerIndex, entry.mask});
    return result;
  };
  auto vi = nativeSignature(vsInputs), vo = nativeSignature(vsOutputs);
  auto pi = nativeSignature(psInputs), po = nativeSignature(psOutputs);
  D3D10DDIARG_STAGE_IO_SIGNATURES vsSignature = {vi.data(),UINT(vi.size()),vo.data(),UINT(vo.size())};
  D3D10DDIARG_STAGE_IO_SIGNATURES psSignature = {pi.data(),UINT(pi.size()),po.data(),UINT(po.size())};
  auto vsMemory = allocate(table.pfnCalcPrivateShaderSize(device, vs.data(), &vsSignature));
  auto psMemory = allocate(table.pfnCalcPrivateShaderSize(device, ps.data(), &psSignature));
  D3D10_DDI_RASTERIZER_DESC rasterDesc = {};
  rasterDesc.FillMode = D3D10_DDI_FILL_SOLID;
  rasterDesc.CullMode = D3D10_DDI_CULL_NONE;
  rasterDesc.DepthClipEnable = TRUE;
  rasterDesc.ScissorEnable = TRUE;
  auto rasterMemory = allocate(table.pfnCalcPrivateRasterizerStateSize(device, &rasterDesc));
  if (!vsMemory || !psMemory || !rasterMemory) return 9;
  D3D10DDI_HSHADER vertex = {vsMemory.get()}, pixel = {psMemory.get()};
  D3D10DDI_HRASTERIZERSTATE raster = {rasterMemory.get()};
  table.pfnCreateVertexShader(device, vs.data(), vertex, {}, &vsSignature);
  table.pfnCreatePixelShader(device, ps.data(), pixel, {}, &psSignature);
  table.pfnCreateRasterizerState(device, &rasterDesc, raster, {});
  D3D10DDI_MIPINFO constantMip = {16,1,1,16,1,1};
  FLOAT pixelColor[4] = {0.5f,0,0,1};
  D3D10_DDIARG_SUBRESOURCE_UP constantData = {pixelColor,16,16};
  D3D10DDIARG_CREATERESOURCE constantDesc = {};
  constantDesc.pMipInfoList = &constantMip; constantDesc.pInitialDataUP = &constantData;
  constantDesc.ResourceDimension = D3D10DDIRESOURCE_BUFFER;
  constantDesc.Usage = D3D10_DDI_USAGE_DEFAULT;
  constantDesc.BindFlags = D3D10_DDI_BIND_CONSTANT_BUFFER;
  constantDesc.MipLevels = 1; constantDesc.ArraySize = 1; constantDesc.SampleDesc.Count = 1;
  auto constant = std::make_unique<ProbeResource>(device, table, constantDesc);
  uint32_t samplePixels[4] = {};
  D3D10DDI_MIPINFO sampleMip = {2,2,1,2,2,1};
  D3D10_DDIARG_SUBRESOURCE_UP sampleData = {samplePixels,8,16};
  D3D10DDIARG_CREATERESOURCE sampleDesc = {};
  sampleDesc.pMipInfoList = &sampleMip; sampleDesc.pInitialDataUP = &sampleData;
  sampleDesc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  sampleDesc.Usage = D3D10_DDI_USAGE_DEFAULT;
  sampleDesc.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE | D3D10_DDI_BIND_RENDER_TARGET;
  sampleDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sampleDesc.MipLevels = 1; sampleDesc.ArraySize = 1; sampleDesc.SampleDesc.Count = 1;
  auto sample = std::make_unique<ProbeResource>(device, table, sampleDesc);
  D3D10DDIARG_CREATESHADERRESOURCEVIEW sampleViewDesc = {};
  sampleViewDesc.hDrvResource = sample->handle; sampleViewDesc.Format = sampleDesc.Format;
  sampleViewDesc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  sampleViewDesc.Tex2D.MipLevels = 1; sampleViewDesc.Tex2D.ArraySize = 1;
  auto sampleViewMemory = allocate(table.pfnCalcPrivateShaderResourceViewSize(device, &sampleViewDesc));
  D3D10DDI_HSHADERRESOURCEVIEW sampleView = {sampleViewMemory.get()};
  table.pfnCreateShaderResourceView(device, &sampleViewDesc, sampleView, {});
  D3D10DDIARG_CREATERENDERTARGETVIEW sampleTargetDesc = {};
  sampleTargetDesc.hDrvResource = sample->handle; sampleTargetDesc.Format = sampleDesc.Format;
  sampleTargetDesc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; sampleTargetDesc.Tex2D.ArraySize = 1;
  auto sampleTargetMemory = allocate(table.pfnCalcPrivateRenderTargetViewSize(device, &sampleTargetDesc));
  D3D10DDI_HRENDERTARGETVIEW sampleTarget = {sampleTargetMemory.get()};
  table.pfnCreateRenderTargetView(device, &sampleTargetDesc, sampleTarget, {});
  if (SUCCEEDED(lastError)) {
    FLOAT white[4] = {1,1,1,1};
    table.pfnClearRenderTargetView(device, sampleTarget, white);
  }
  table.pfnDestroyRenderTargetView(device, sampleTarget);
  if (SUCCEEDED(lastError)) {
    table.pfnShaderResourceViewReadAfterWriteHazard(device, sampleView, target);
    if (lastError == E_INVALIDARG) lastError = S_OK;
    else lastError = E_FAIL;
  }
  D3D10_DDI_SAMPLER_DESC samplerDesc = {};
  samplerDesc.Filter = D3D10_DDI_FILTER_MIN_MAG_MIP_POINT;
  samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D10_DDI_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.MaxAnisotropy = 1; samplerDesc.ComparisonFunc = D3D10_DDI_COMPARISON_NEVER;
  samplerDesc.MaxLOD = 1;
  auto samplerMemory = allocate(table.pfnCalcPrivateSamplerSize(device, &samplerDesc));
  D3D10DDI_HSAMPLER sampler = {samplerMemory.get()};
  table.pfnCreateSampler(device, &samplerDesc, sampler, {});
  uint32_t indices[3] = {0,1,2};
  D3D10DDI_MIPINFO indexMip = {12,1,1,12,1,1};
  D3D10DDIARG_CREATERESOURCE indexDesc = {};
  indexDesc.pMipInfoList = &indexMip;
  indexDesc.ResourceDimension = D3D10DDIRESOURCE_BUFFER;
  indexDesc.Usage = D3D10_DDI_USAGE_DEFAULT; indexDesc.BindFlags = D3D10_DDI_BIND_INDEX_BUFFER;
  indexDesc.MipLevels = 1; indexDesc.ArraySize = 1; indexDesc.SampleDesc.Count = 1;
  auto index = std::make_unique<ProbeResource>(device, table, indexDesc);
  if (SUCCEEDED(lastError))
    table.pfnResourceUpdateSubresourceUP(device, index->handle, 0, nullptr, indices, 0, 0);
  FLOAT positions[6] = {-1,1,3,1,-1,-3};
  D3D10DDI_MIPINFO vertexMip = {24,1,1,24,1,1};
  D3D10DDIARG_CREATERESOURCE vertexDesc = indexDesc;
  vertexDesc.pMipInfoList = &vertexMip; vertexDesc.pInitialDataUP = nullptr;
  vertexDesc.BindFlags = D3D10_DDI_BIND_VERTEX_BUFFER;
  vertexDesc.Usage = D3D10_DDI_USAGE_DYNAMIC; vertexDesc.MapFlags = D3D10_DDI_CPU_ACCESS_WRITE;
  auto vertices = std::make_unique<ProbeResource>(device, table, vertexDesc);
  D3D10DDI_MAPPED_SUBRESOURCE vertexMapping = {};
  if (SUCCEEDED(lastError)) {
    table.pfnDynamicIABufferMapDiscard(device, vertices->handle, 0, D3D10_DDI_MAP_WRITE_DISCARD, 0, &vertexMapping);
    if (SUCCEEDED(lastError) && vertexMapping.pData) {
      std::memcpy(vertexMapping.pData, positions, sizeof(positions));
      table.pfnDynamicIABufferUnmap(device, vertices->handle, 0);
    }
  }
  D3D10DDIARG_INPUT_ELEMENT_DESC inputElement = {};
  inputElement.Format = DXGI_FORMAT_R32G32_FLOAT;
  inputElement.InputSlotClass = D3D10_DDI_INPUT_PER_VERTEX_DATA;
  D3D10DDIARG_CREATEELEMENTLAYOUT layoutDesc = {&inputElement,1};
  auto layoutMemory = allocate(table.pfnCalcPrivateElementLayoutSize(device, &layoutDesc));
  D3D10DDI_HELEMENTLAYOUT layout = {layoutMemory.get()};
  table.pfnCreateElementLayout(device, &layoutDesc, layout, {});
  D3D10_DDI_BLEND_DESC blendDesc = {};
  blendDesc.BlendEnable[0] = TRUE; blendDesc.RenderTargetWriteMask[0] = 15;
  blendDesc.SrcBlend = blendDesc.DestBlend = D3D10_DDI_BLEND_ONE;
  blendDesc.BlendOp = blendDesc.BlendOpAlpha = D3D10_DDI_BLEND_OP_ADD;
  blendDesc.SrcBlendAlpha = D3D10_DDI_BLEND_ONE; blendDesc.DestBlendAlpha = D3D10_DDI_BLEND_ZERO;
  auto blendMemory = allocate(table.pfnCalcPrivateBlendStateSize(device, &blendDesc));
  D3D10DDI_HBLENDSTATE blend = {blendMemory.get()};
  table.pfnCreateBlendState(device, &blendDesc, blend, {});
  D3D10DDIARG_CREATERESOURCE depthResourceDesc = {};
  D3D10DDI_MIPINFO depthMip = {64,64,1,64,64,1};
  depthResourceDesc.pMipInfoList = &depthMip;
  depthResourceDesc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  depthResourceDesc.Usage = D3D10_DDI_USAGE_DEFAULT;
  depthResourceDesc.BindFlags = D3D10_DDI_BIND_DEPTH_STENCIL;
  depthResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depthResourceDesc.MipLevels = 1; depthResourceDesc.ArraySize = 1; depthResourceDesc.SampleDesc.Count = 1;
  auto depthResource = std::make_unique<ProbeResource>(device, table, depthResourceDesc);
  D3D10DDIARG_CREATEDEPTHSTENCILVIEW depthViewDesc = {};
  depthViewDesc.hDrvResource = depthResource->handle; depthViewDesc.Format = depthResourceDesc.Format;
  depthViewDesc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; depthViewDesc.Tex2D.ArraySize = 1;
  auto depthViewMemory = allocate(table.pfnCalcPrivateDepthStencilViewSize(device, &depthViewDesc));
  D3D10DDI_HDEPTHSTENCILVIEW depthView = {depthViewMemory.get()};
  table.pfnCreateDepthStencilView(device, &depthViewDesc, depthView, {});
  D3D10_DDI_DEPTH_STENCIL_DESC depthStateDesc = {};
  depthStateDesc.DepthEnable = TRUE; depthStateDesc.DepthWriteMask = D3D10_DDI_DEPTH_WRITE_MASK_ALL;
  depthStateDesc.DepthFunc = D3D10_DDI_COMPARISON_LESS;
  depthStateDesc.StencilEnable = depthStateDesc.FrontEnable = depthStateDesc.BackEnable = TRUE;
  depthStateDesc.StencilReadMask = depthStateDesc.StencilWriteMask = 0xff;
  depthStateDesc.FrontFace.StencilFunc = D3D10_DDI_COMPARISON_EQUAL;
  depthStateDesc.FrontFace.StencilFailOp = depthStateDesc.FrontFace.StencilDepthFailOp
    = depthStateDesc.FrontFace.StencilPassOp = D3D10_DDI_STENCIL_OP_KEEP;
  depthStateDesc.BackFace = depthStateDesc.FrontFace;
  auto depthStateMemory = allocate(table.pfnCalcPrivateDepthStencilStateSize(device, &depthStateDesc));
  D3D10DDI_HDEPTHSTENCILSTATE depthState = {depthStateMemory.get()};
  table.pfnCreateDepthStencilState(device, &depthStateDesc, depthState, {});
  D3D10DDIARG_CREATEQUERY occlusionDesc = {}; occlusionDesc.Query = D3D10DDI_QUERY_OCCLUSION;
  std::array<Memory,3> occlusionMemory = {allocate(table.pfnCalcPrivateQuerySize(device, &occlusionDesc)),
    allocate(table.pfnCalcPrivateQuerySize(device, &occlusionDesc)), allocate(table.pfnCalcPrivateQuerySize(device, &occlusionDesc))};
  D3D10DDI_HQUERY occlusion[3] = {};
  for (unsigned i = 0; i < 3; i++) {
    occlusion[i].pDrvPrivate = occlusionMemory[i].get();
    table.pfnCreateQuery(device, &occlusionDesc, occlusion[i], {});
  }
  UINT64 visibleSamples[3] = {~UINT64(0),~UINT64(0),~UINT64(0)};
  unsigned mismatches = 4096;
  unsigned linkageFailures[16] = {}, unknownFailures = 0;
  BOOL eventComplete = FALSE;
  if (SUCCEEDED(lastError)) {
    // Half-red destination plus half-red shader output must become full red.
    // Omitting blending, sampling, constant binding or the draw fails pixels.
    FLOAT color[4] = {0.5f,0,0,0};
    table.pfnClearRenderTargetView(device, view, color);
    table.pfnVsSetShader(device, vertex);
    table.pfnPsSetShader(device, pixel);
    table.pfnPsSetConstantBuffers(device, 0, 1, &constant->handle);
    table.pfnVsSetConstantBuffers(device, 0, 1, &constant->handle);
    table.pfnShaderResourceViewReadAfterWriteHazard(device, sampleView, sample->handle);
    table.pfnPsSetShaderResources(device, 0, 1, &sampleView);
    table.pfnPsSetSamplers(device, 0, 1, &sampler);
    const FLOAT factor[4] = {1,1,1,1};
    table.pfnSetBlendState(device, blend, factor, 0xffffffff);
    table.pfnSetRasterizerState(device, raster);
    table.pfnSetRenderTargets(device, &view, 1, 0, depthView);
    table.pfnSetDepthStencilState(device, depthState, 1);
    table.pfnClearDepthStencilView(device, depthView, D3D10_DDI_CLEAR_DEPTH | D3D10_DDI_CLEAR_STENCIL, 0, 1);
    D3D10_DDI_VIEWPORT viewport = {0,0,64,64,0,1};
    table.pfnSetViewports(device, 1, 0, &viewport);
    const D3D10_DDI_RECT scissor = {0,0,64,64};
    table.pfnSetScissorRects(device, 1, 0, &scissor);
    table.pfnIaSetTopology(device, D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    table.pfnResourceReadAfterWriteHazard(device, index->handle);
    table.pfnIaSetIndexBuffer(device, index->handle, DXGI_FORMAT_R32_UINT, 0);
    table.pfnIaSetInputLayout(device, layout);
    const UINT stride = 8, offset = 0;
    table.pfnIaSetVertexBuffers(device, 0, 1, &vertices->handle, &stride, &offset);
    // Reject by depth, pass both tests, then reject by stencil alone. Exact
    // occlusion counts detect ignored depth/stencil state even when additive
    // color saturation would hide an extra draw in the final red image.
    table.pfnQueryBegin(device, occlusion[0]);
    table.pfnDrawIndexed(device, 3, 0, 0);
    table.pfnQueryEnd(device, occlusion[0]);
    table.pfnClearDepthStencilView(device, depthView, D3D10_DDI_CLEAR_DEPTH, 1, 0);
    table.pfnQueryBegin(device, occlusion[1]);
    table.pfnDrawIndexed(device, 3, 0, 0);
    table.pfnQueryEnd(device, occlusion[1]);
    table.pfnClearDepthStencilView(device, depthView, D3D10_DDI_CLEAR_DEPTH, 1, 0);
    table.pfnSetDepthStencilState(device, depthState, 0);
    table.pfnQueryBegin(device, occlusion[2]);
    table.pfnDrawIndexed(device, 3, 0, 0);
    table.pfnQueryEnd(device, occlusion[2]);
    table.pfnResourceCopy(device, staging, target);
    table.pfnQueryEnd(device, event);
    D3D10DDI_MAPPED_SUBRESOURCE mapped = {};
    table.pfnStagingResourceMap(device, staging, 0, D3D10_DDI_MAP_READ, 0, &mapped);
    if (SUCCEEDED(lastError) && mapped.pData && mapped.RowPitch >= 256) {
      mismatches = 0;
      const unsigned char expected[4] = {255,0,0,255};
      for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++) {
          const auto* value = static_cast<unsigned char*>(mapped.pData) + y*mapped.RowPitch + x*4;
          if (std::memcmp(value, expected, 4)) {
            if (!mismatches) std::printf("DDI_FIRST_PIXEL x=%u y=%u rgba=%u,%u,%u,%u\n",x,y,value[0],value[1],value[2],value[3]);
            mismatches++;
            if (value[0] == 128 && value[1] > 0 && value[1] < 16 && !value[2] && value[3] == 255)
              linkageFailures[value[1]]++;
            else unknownFailures++;
          }
        }
      table.pfnStagingResourceUnmap(device, staging, 0);
    }
    if (SUCCEEDED(lastError)) {
      const ULONGLONG deadline = GetTickCount64() + 5000;
      do {
        lastError = S_OK;
        table.pfnQueryGetData(device, event, &eventComplete, sizeof(eventComplete), 0);
        if (lastError != DXGI_DDI_ERR_WASSTILLDRAWING) break;
        Sleep(1);
      } while (GetTickCount64() < deadline);
    }
    for (unsigned i = 0; i < 3 && SUCCEEDED(lastError); i++) {
      const ULONGLONG deadline = GetTickCount64() + 5000;
      do {
        lastError = S_OK;
        table.pfnQueryGetData(device, occlusion[i], &visibleSamples[i], sizeof(visibleSamples[i]), 0);
        if (lastError != DXGI_DDI_ERR_WASSTILLDRAWING) break;
        Sleep(1);
      } while (GetTickCount64() < deadline);
    }
  }
  const bool depthStencilPass = visibleSamples[0] == 0 && visibleSamples[1] == 4096 && visibleSamples[2] == 0;
  std::printf("DDI_DEPTH_STENCIL %s depth_rejected=%llu visible=%llu stencil_rejected=%llu\n",
    depthStencilPass ? "PASS" : "FAIL", static_cast<unsigned long long>(visibleSamples[0]),
    static_cast<unsigned long long>(visibleSamples[1]), static_cast<unsigned long long>(visibleSamples[2]));
  if (mismatches && SUCCEEDED(lastError)) {
    std::printf("DDI_LINKAGE_FAILURES bits=UV:1,raw_float:2,immediate_array:4,fixed_integer:8 unknown=%u",unknownFailures);
    for (unsigned i = 1; i < 16; ++i) if (linkageFailures[i]) std::printf(" mask%u=%u",i,linkageFailures[i]);
    std::puts("");
    // One bounded diagnostic draw exposes actual payload words. No state from
    // this follow-up can turn the failed acceptance image into a pass.
    std::vector<uint32_t> inspectTokens;
    std::vector<dxvk::umd::ShaderSignatureEntry> inspectInputs, inspectOutputs;
    if (compileLinkageProbeShader(false,inspectTokens,inspectInputs,inspectOutputs,true)) {
      auto ii = nativeSignature(inspectInputs), io = nativeSignature(inspectOutputs);
      D3D10DDIARG_STAGE_IO_SIGNATURES signature = {ii.data(),UINT(ii.size()),io.data(),UINT(io.size())};
      auto inspectMemory = allocate(table.pfnCalcPrivateShaderSize(device,inspectTokens.data(),&signature));
      D3D10DDI_HSHADER inspectShader = {inspectMemory.get()};
      if (inspectMemory) {
        table.pfnCreatePixelShader(device,inspectTokens.data(),inspectShader,{},&signature);
        table.pfnPsSetShader(device,inspectShader);
        table.pfnSetRenderTargets(device,&view,1,0,{});
        table.pfnSetDepthStencilState(device,{},0);
        const FLOAT factor[4] = {1,1,1,1};
        table.pfnSetBlendState(device,{},factor,0xffffffff);
        table.pfnDrawIndexed(device,3,0,0);
        table.pfnResourceCopy(device,staging,target);
        D3D10DDI_MAPPED_SUBRESOURCE inspected = {};
        table.pfnStagingResourceMap(device,staging,0,D3D10_DDI_MAP_READ,0,&inspected);
        if (SUCCEEDED(lastError) && inspected.pData && inspected.RowPitch >= 256) {
          std::printf("DDI_LINKAGE_WORDS order=immediate[4],raw_float[4],fixed_integer[4],uv_x_at12,uv_y_at13,pos_x_at14,pos_y_at15");
          for (unsigned i = 0; i < 16; ++i) {
            uint32_t word; std::memcpy(&word,static_cast<unsigned char*>(inspected.pData)+i*4,4);
            std::printf(" %08x",word);
          }
          std::puts("");
          table.pfnStagingResourceUnmap(device,staging,0);
        }
        table.pfnPsSetShader(device,pixel);
        table.pfnDestroyShader(device,inspectShader);
      }
    }
  }
  if (nativeCopy && SUCCEEDED(lastError) && eventComplete && !mismatches && depthStencilPass) {
    if (!dxgiFunctions.pfnPresent) lastError = E_NOTIMPL;
    else {
      DXGI_DDI_ARG_PRESENT args = {};
      args.hDevice = reinterpret_cast<UINT_PTR>(device.pDrvPrivate);
      args.hSurfaceToPresent = reinterpret_cast<UINT_PTR>(target.pDrvPrivate);
      args.pDXGIContext = &publication; args.Flags.Blt = 1;
      lastError = dxgiFunctions.pfnPresent(&args);
    }
  }
  table.pfnSetRenderTargets(device, nullptr, 0, 1, {});
  table.pfnSetDepthStencilState(device, {}, 0);
  table.pfnIaSetInputLayout(device, {});
  if (layout.pDrvPrivate) table.pfnDestroyElementLayout(device, layout);
  vertices.reset();
  for (auto query : occlusion) if (query.pDrvPrivate) table.pfnDestroyQuery(device, query);
  if (depthState.pDrvPrivate) table.pfnDestroyDepthStencilState(device, depthState);
  if (depthView.pDrvPrivate) table.pfnDestroyDepthStencilView(device, depthView);
  depthResource.reset();
  table.pfnDestroyRasterizerState(device, raster);
  table.pfnDestroyShader(device, pixel);
  table.pfnDestroyShader(device, vertex);
  table.pfnDestroyRenderTargetView(device, view);
  table.pfnDestroyResource(device, staging);
  table.pfnDestroyResource(device, target);
  table.pfnDestroyQuery(device, event);
  if (blend.pDrvPrivate) table.pfnDestroyBlendState(device, blend);
  index.reset();
  if (sampler.pDrvPrivate) table.pfnDestroySampler(device, sampler);
  if (sampleView.pDrvPrivate) table.pfnDestroyShaderResourceView(device, sampleView);
  sample.reset();
  constant.reset();
  table.pfnDestroyDevice(device);
  const HRESULT closed = publication.close();
  if (FAILED(closed)) lastError = closed;
  const bool pass = !mismatches && eventComplete && depthStencilPass && SUCCEEDED(lastError)
      && (!nativeCopy || publication.verified());
  std::printf("DDI_DRAW_PIXELS %s pixels=4096 mismatches=%u event=%d error=%08lx\n",
    pass ? "PASS" : "FAIL", mismatches, eventComplete, static_cast<unsigned long>(lastError));
  return pass ? 0 : 7;
}
