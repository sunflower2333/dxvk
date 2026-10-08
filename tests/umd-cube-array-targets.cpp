// SPDX-License-Identifier: MIT
// Source-linked typed production DDIs against an explicit public WARP reference.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_cube_target.h"
#include "../src/umd/umd_view.h"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using Microsoft::WRL::ComPtr;
static_assert(sizeof(D3D11_RENDER_TARGET_VIEW_DESC) == 20 && sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC) == 24);
static_assert(D3D11_RTV_DIMENSION_TEXTURE2DARRAY == 5 && D3D11_DSV_DIMENSION_TEXTURE2DARRAY == 4);
static unsigned checks, callbacks, alternateCallbacks, backendCalls, views, snapshots, comparedWords;
static HRESULT lastError = S_OK;
static DWORD callerThread;
static const LUID expectedLuid = {0x4f3a9127u, -28};
static ComPtr<ID3D11DeviceContext> createdContext;
static void check(bool value, unsigned line, const char* expression) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "FAIL cube-array targets line=%u expression=%s checks=%u callbacks=%u hr=%08lx\n",
      line, expression, checks, callbacks, static_cast<unsigned long>(lastError));
    std::exit(1);
  }
}
#define CHECK(x) check(!!(x), __LINE__, #x)
static void ok() { CHECK(lastError == S_OK); }
struct Storage {
  static constexpr UINT64 canary = 0xeba12973f04d865cull;
  std::vector<UINT64> words;
  explicit Storage(SIZE_T bytes) : words((bytes + 7) / 8 + 4, 0) {
    CHECK(bytes); words[0] = words[1] = words[words.size()-2] = words.back() = canary;
  }
  void* data() { return words.data() + 2; }
  void poison() { std::fill(words.begin()+2, words.end()-2, 0xcdcdcdcdcdcdcdcdull); }
  void guards() const { CHECK(words[0] == canary && words[1] == canary && words[words.size()-2] == canary && words.back() == canary); }
};
static const Storage* failedStorage;
static const std::vector<UINT64>* failedOriginal;
static ID3D11DeviceContext* failedContext;
static ID3D11RenderTargetView* retainedTarget;
static ID3D11DepthStencilView* retainedDepth;
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  CHECK(runtime.handle && GetCurrentThreadId() == callerThread && FAILED(result));
  ++callbacks; lastError = result;
  if (failedStorage) {
    CHECK(failedOriginal && failedStorage->words == *failedOriginal); failedStorage->guards();
    ComPtr<ID3D11RenderTargetView> rt; ComPtr<ID3D11DepthStencilView> ds;
    failedContext->OMGetRenderTargets(1, &rt, &ds);
    CHECK(rt.Get() == retainedTarget && ds.Get() == retainedDepth);
  }
}
static void APIENTRY alternateError(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  ++alternateCallbacks; error(runtime, result);
}
HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  CHECK(!runtime && !std::memcmp(&luid, &expectedLuid, sizeof(luid))); ++backendCalls;
  level = implementationFeatureLevel(level);
  const HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (result == S_OK) createdContext = *context;
  return result;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }

template<unsigned Interface> struct Fixture {
  using Table = std::conditional_t<Interface == 2, D3D11DDI_DEVICEFUNCS,
    std::conditional_t<Interface == 1, D3D10_1DDI_DEVICEFUNCS, D3D10DDI_DEVICEFUNCS>>;
  using Core = std::conditional_t<Interface == 2, D3D11DDI_CORELAYER_DEVICECALLBACKS, D3D10DDI_CORELAYER_DEVICECALLBACKS>;
  using ResourceArgs = std::conditional_t<Interface == 2, D3D11DDIARG_CREATERESOURCE, D3D10DDIARG_CREATERESOURCE>;
  using DepthArgs = std::conditional_t<Interface == 2, D3D11DDIARG_CREATEDEPTHSTENCILVIEW, D3D10DDIARG_CREATEDEPTHSTENCILVIEW>;
  Storage storage{VioGpuDxvkPrivateDeviceSize()}; D3D10DDI_HDEVICE device{storage.data()}; Core core{};
  struct { UINT64 before = Storage::canary; Table table{}; UINT64 after = Storage::canary; } output;
  ComPtr<ID3D11DeviceContext> context; ComPtr<ID3D11Device> backend;
  Fixture() {
    core.pfnSetErrorCb = error;
    if constexpr (Interface == 2) CHECK(VioGpuDxvkCreateDdiTestDevice11(&expectedLuid, device, {&core}, &core, &output.table, D3D_FEATURE_LEVEL_11_0) == S_OK);
    else if constexpr (Interface == 1) CHECK(VioGpuDxvkCreateDdiTestDevice10_1(&expectedLuid, device, {&core}, &core, &output.table) == S_OK);
    else CHECK(VioGpuDxvkCreateDdiTestDevice(&expectedLuid, device, {&core}, &core, &output.table) == S_OK);
    guards(); context = createdContext; createdContext.Reset(); CHECK(context); context->GetDevice(&backend); CHECK(backend);
  }
  void guards() const { storage.guards(); CHECK(output.before == Storage::canary && output.after == Storage::canary); }
  void bind(D3D10DDI_HRENDERTARGETVIEW rt = {}, D3D10DDI_HDEPTHSTENCILVIEW ds = {}) {
    const UINT count = rt.pDrvPrivate ? 1 : 0;
    if constexpr (Interface == 2) output.table.pfnSetRenderTargets(device, count ? &rt : nullptr, count, 1-count, ds, nullptr, nullptr, count, 0, count, 0);
    else output.table.pfnSetRenderTargets(device, count ? &rt : nullptr, count, 1-count, ds);
    ok(); guards();
  }
  ~Fixture() { bind(); context->ClearState(); context.Reset(); backend.Reset(); output.table.pfnDestroyDevice(device); ok(); guards(); }
};
using Pixels = std::vector<std::vector<UINT>>;
constexpr UINT Edge = 8, Mips = 4;
static Pixels initialPixels(UINT faces, bool depth) {
  Pixels data(faces * Mips);
  for (UINT face = 0; face < faces; ++face) for (UINT mip = 0; mip < Mips; ++mip) {
    const UINT edge = Edge >> mip; auto& row = data[face * Mips + mip]; row.resize(edge * edge);
    for (UINT word = 0; word < row.size(); ++word)
      row[word] = depth ? ((0x40u + face) << 24) | (0x400000u + 32u * face + mip)
        : 0xff000000u | (face << 19) | (mip << 16) | (word + 1);
  }
  return data;
}
static void patch(Pixels& data, UINT mip, UINT first, UINT count, UINT value) {
  for (UINT face = first; face < first + count; ++face)
    std::fill(data[face * Mips + mip].begin(), data[face * Mips + mip].end(), value);
}
static std::vector<UINT> flatten(const Pixels& data) {
  std::vector<UINT> words; for (const auto& row : data) words.insert(words.end(), row.begin(), row.end()); return words;
}
static void save(const std::string& path, const void* bytes, size_t size) {
  std::ofstream file(path, std::ios::binary); CHECK(file.is_open());
  file.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size)); file.close(); CHECK(!file.fail());
}
template<unsigned Interface> struct Resource {
  using Args = typename Fixture<Interface>::ResourceArgs;
  Fixture<Interface>& owner; Storage storage; D3D10DDI_HRESOURCE handle;
  Resource(Fixture<Interface>& f, const Args& args) : owner(f), storage(f.output.table.pfnCalcPrivateResourceSize(f.device, &args)), handle{storage.data()} {
    ok(); f.output.table.pfnCreateResource(f.device, &args, handle, {}); ok(); storage.guards();
  }
  ~Resource() { owner.output.table.pfnDestroyResource(owner.device, handle); ok(); storage.guards(); }
};
template<unsigned Interface> struct Cube {
  using Args = typename Fixture<Interface>::ResourceArgs;
  Fixture<Interface>& owner; UINT faces; bool depth;
  Pixels initial; std::array<D3D10DDI_MIPINFO, Mips> mips{};
  std::vector<D3D10_DDIARG_SUBRESOURCE_UP> uploads;
  Args args{}; std::unique_ptr<Resource<Interface>> native; ComPtr<ID3D11Texture2D> reference;
  Cube(Fixture<Interface>& f, UINT faceCount, bool isDepth) : owner(f), faces(faceCount), depth(isDepth), initial(initialPixels(faces, depth)), uploads(initial.size()) {
    for (UINT mip = 0; mip < Mips; ++mip) { const UINT edge = Edge >> mip; mips[mip] = {edge,edge,1,16,16,1}; }
    for (UINT index = 0; index < uploads.size(); ++index) {
      uploads[index] = {initial[index].data(), (Edge >> (index % Mips)) * 4, UINT(initial[index].size()) * 4};
    }
    args.pMipInfoList = mips.data(); args.pInitialDataUP = uploads.data(); args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE;
    args.Usage = D3D10_DDI_USAGE_DEFAULT; args.BindFlags = depth ? D3D10_DDI_BIND_DEPTH_STENCIL : D3D10_DDI_BIND_RENDER_TARGET;
    args.Format = depth ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R8G8B8A8_UNORM;
    args.SampleDesc.Count = 1; args.MipLevels = Mips; args.ArraySize = faces;
    native = std::make_unique<Resource<Interface>>(owner, args);
    D3D11_TEXTURE2D_DESC desc{}; desc.Width = desc.Height = Edge; desc.MipLevels = Mips; desc.ArraySize = faces;
    desc.Format = args.Format; desc.SampleDesc.Count = 1; desc.BindFlags = args.BindFlags; desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    std::vector<D3D11_SUBRESOURCE_DATA> publicInitial(uploads.size());
    for (UINT index = 0; index < uploads.size(); ++index) publicInitial[index] = {uploads[index].pSysMem, uploads[index].SysMemPitch, uploads[index].SysMemSlicePitch};
    CHECK(owner.backend->CreateTexture2D(&desc, publicInitial.data(), &reference) == S_OK);
  }
  std::vector<UINT> readNative() {
    auto desc = args; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; desc.pInitialDataUP = nullptr;
    desc.Usage = D3D10_DDI_USAGE_STAGING; desc.MapFlags = D3D10_DDI_CPU_ACCESS_READ; desc.BindFlags = desc.MiscFlags = 0;
    Resource<Interface> staging(owner, desc); owner.output.table.pfnResourceCopy(owner.device, staging.handle, native->handle); ok();
    std::vector<UINT> words;
    for (UINT index = 0; index < faces * Mips; ++index) {
      D3D10DDI_MAPPED_SUBRESOURCE map{};
      owner.output.table.pfnStagingResourceMap(owner.device, staging.handle, index, D3D10_DDI_MAP_READ, 0, &map); ok();
      const UINT edge = Edge >> (index % Mips); CHECK(map.pData && map.RowPitch >= edge * 4);
      for (UINT y = 0; y < edge; ++y) for (UINT x = 0; x < edge; ++x) { UINT word;
        std::memcpy(&word, static_cast<const char*>(map.pData) + y * map.RowPitch + x * 4, 4); words.push_back(word); }
      owner.output.table.pfnStagingResourceUnmap(owner.device, staging.handle, index); ok();
    }
    return words;
  }
  std::vector<UINT> readReference() {
    D3D11_TEXTURE2D_DESC desc{}; reference->GetDesc(&desc); desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.BindFlags = desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging; CHECK(owner.backend->CreateTexture2D(&desc, nullptr, &staging) == S_OK);
    owner.context->CopyResource(staging.Get(), reference.Get()); std::vector<UINT> words;
    for (UINT index = 0; index < faces * Mips; ++index) {
      D3D11_MAPPED_SUBRESOURCE map{}; CHECK(owner.context->Map(staging.Get(), index, D3D11_MAP_READ, 0, &map) == S_OK);
      const UINT edge = Edge >> (index % Mips); CHECK(map.pData && map.RowPitch >= edge * 4);
      for (UINT y = 0; y < edge; ++y) for (UINT x = 0; x < edge; ++x) { UINT word;
        std::memcpy(&word, static_cast<const char*>(map.pData) + y * map.RowPitch + x * 4, 4); words.push_back(word); }
      owner.context->Unmap(staging.Get(), index);
    }
    return words;
  }
  void read(const Pixels& expected, unsigned step) {
    const auto actual = readNative(), publicActual = readReference(), wanted = flatten(expected);
    CHECK(actual.size() == wanted.size() && publicActual.size() == wanted.size());
    for (size_t word = 0; word < wanted.size(); ++word) { CHECK(actual[word] == wanted[word] && publicActual[word] == wanted[word]); ++comparedWords; }
    const std::string name = "cube-target-" + std::to_string(Interface) + (depth ? "-depth-" : "-color-") + std::to_string(step);
    save(name+".actual.u32", actual.data(), actual.size()*4); save(name+".public.u32", publicActual.data(), publicActual.size()*4); save(name+".expected.u32", wanted.data(), wanted.size()*4);
    std::printf("CUBE_TARGET_READBACK profile=%u depth=%u step=%u faces=%u words=%zu mismatches=0\n", Interface, unsigned(depth), step, faces, wanted.size()); ++snapshots;
  }
};

template<unsigned Interface> static void rejection(Fixture<Interface>& f, bool depth, const std::function<void()>& call, Storage& storage) {
  const auto before = storage.words; const unsigned originalCallbacks = callbacks;
  ComPtr<ID3D11RenderTargetView> rt; ComPtr<ID3D11DepthStencilView> ds; f.context->OMGetRenderTargets(1, &rt, &ds);
  failedStorage = &storage; failedOriginal = &before; failedContext = f.context.Get(); retainedTarget = rt.Get(); retainedDepth = ds.Get();
  call(); CHECK(lastError == E_INVALIDARG && callbacks == originalCallbacks + 1 && storage.words == before); storage.guards();
  const HRESULT originalError = lastError;
  failedStorage = nullptr; failedOriginal = nullptr; failedContext = nullptr; retainedTarget = nullptr; retainedDepth = nullptr;
  lastError = S_OK; f.guards();
  std::printf("CUBE_TARGET_NEGATIVE profile=%u depth=%u callbacks=%u hr=%08lx atomic=1 binding_retained=1\n", Interface, unsigned(depth), callbacks, static_cast<unsigned long>(originalError));
}

template<unsigned Interface> static void exercise(bool depth) {
  Fixture<Interface> f; constexpr UINT faces = Interface == 0 ? 6 : 18;
  Cube<Interface> cube(f, faces, depth); auto expected = cube.initial;
  const std::array<std::array<UINT,3>,4> ranges = Interface == 0
    ? std::array<std::array<UINT,3>,4>{{{0,5,1},{1,1,3},{2,0,6},{3,5,1}}}
    : std::array<std::array<UINT,3>,4>{{{0,5,3},{1,11,2},{2,17,1},{3,0,18}}};
  std::unique_ptr<Storage> retained;
  D3D10DDI_HRENDERTARGETVIEW rtHandle{}; D3D10DDI_HDEPTHSTENCILVIEW dsHandle{};
  for (UINT step = 0; step < ranges.size(); ++step) {
    const auto& range = ranges[step];
    if (retained) { f.bind(); if (depth) f.output.table.pfnDestroyDepthStencilView(f.device, dsHandle); else f.output.table.pfnDestroyRenderTargetView(f.device, rtHandle); ok(); retained->guards(); }
    const UINT viewFormat = depth ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_R8G8B8A8_UNORM;
    if (depth) {
      typename Fixture<Interface>::DepthArgs args{}; args.hDrvResource = cube.native->handle; args.Format = static_cast<DXGI_FORMAT>(viewFormat);
      args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE; args.TexCube = {range[0],range[1],range[2]};
      UINT flags = 0; if constexpr (Interface == 2) { flags = step; args.Flags = flags; }
      retained = std::make_unique<Storage>(f.output.table.pfnCalcPrivateDepthStencilViewSize(f.device, &args)); ok(); dsHandle = {retained->data()};
      f.output.table.pfnCreateDepthStencilView(f.device, &args, dsHandle, {}); ok(); f.bind({}, dsHandle);
      ComPtr<ID3D11DepthStencilView> bound; f.context->OMGetRenderTargets(0, nullptr, &bound); CHECK(bound);
      D3D11_DEPTH_STENCIL_VIEW_DESC observed{}; bound->GetDesc(&observed);
      D3D11_DEPTH_STENCIL_VIEW_DESC desc{}; desc.Format = static_cast<DXGI_FORMAT>(viewFormat); desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
      desc.Flags = flags; desc.Texture2DArray = {range[0],range[1],range[2]}; CHECK(!std::memcmp(&desc,&observed,sizeof(desc)));
      ComPtr<ID3D11DepthStencilView> publicView; CHECK(f.backend->CreateDepthStencilView(cube.reference.Get(), &desc, &publicView) == S_OK);
      D3D11_DEPTH_STENCIL_VIEW_DESC publicDesc{}; publicView->GetDesc(&publicDesc); CHECK(!std::memcmp(&desc,&publicDesc,sizeof(desc)));
      if (!flags) {
        f.output.table.pfnClearDepthStencilView(f.device, dsHandle, D3D10_DDI_CLEAR_DEPTH | D3D10_DDI_CLEAR_STENCIL, 1.0f, UINT8(0x29+step)); ok();
        f.context->ClearDepthStencilView(publicView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, UINT8(0x29+step));
        patch(expected,range[0],range[1],range[2],((0x29u+step)<<24)|0xffffffu);
      }
      save("cube-target-desc-"+std::to_string(Interface)+"-depth-"+std::to_string(step)+".u32",&observed,sizeof(observed));
      save("cube-target-desc-"+std::to_string(Interface)+"-depth-"+std::to_string(step)+".public-u32",&publicDesc,sizeof(publicDesc));
    } else {
      D3D10DDIARG_CREATERENDERTARGETVIEW args{}; args.hDrvResource = cube.native->handle; args.Format = static_cast<DXGI_FORMAT>(viewFormat);
      args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE; args.TexCube = {range[0],range[1],range[2]};
      retained = std::make_unique<Storage>(f.output.table.pfnCalcPrivateRenderTargetViewSize(f.device,&args)); ok(); rtHandle = {retained->data()};
      f.output.table.pfnCreateRenderTargetView(f.device,&args,rtHandle,{}); ok(); f.bind(rtHandle);
      ComPtr<ID3D11RenderTargetView> bound; f.context->OMGetRenderTargets(1,&bound,nullptr); CHECK(bound);
      D3D11_RENDER_TARGET_VIEW_DESC observed{}; bound->GetDesc(&observed);
      D3D11_RENDER_TARGET_VIEW_DESC desc{}; desc.Format = static_cast<DXGI_FORMAT>(viewFormat); desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
      desc.Texture2DArray = {range[0],range[1],range[2]}; CHECK(!std::memcmp(&desc,&observed,sizeof(desc)));
      ComPtr<ID3D11RenderTargetView> publicView; CHECK(f.backend->CreateRenderTargetView(cube.reference.Get(),&desc,&publicView) == S_OK);
      D3D11_RENDER_TARGET_VIEW_DESC publicDesc{}; publicView->GetDesc(&publicDesc); CHECK(!std::memcmp(&desc,&publicDesc,sizeof(desc)));
      FLOAT color[4] = {step & 1 ? 1.0f : 0.0f, step & 2 ? 1.0f : 0.0f, 1.0f, 1.0f};
      f.output.table.pfnClearRenderTargetView(f.device,rtHandle,color); ok(); f.context->ClearRenderTargetView(publicView.Get(),color);
      patch(expected,range[0],range[1],range[2],0xffff0000u | ((step & 2) ? 0xff00u : 0u) | ((step & 1) ? 0xffu : 0u));
      save("cube-target-desc-"+std::to_string(Interface)+"-color-"+std::to_string(step)+".u32",&observed,sizeof(observed));
      save("cube-target-desc-"+std::to_string(Interface)+"-color-"+std::to_string(step)+".public-u32",&publicDesc,sizeof(publicDesc));
    }
    retained->guards(); ++views; cube.read(expected,step);
  }
  f.core.pfnSetErrorCb = alternateError;
  const std::array<std::array<UINT,3>,7> malformed{{{Mips,0,1},{0,faces,1},{0,0,0},{0,faces-1,2},{0,UINT(-1),1},{0,1,UINT(-1)},{UINT(-1),0,1}}};
  for (const auto& range : malformed) {
    if (depth) {
      typename Fixture<Interface>::DepthArgs args{}; args.hDrvResource=cube.native->handle;args.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
      args.ResourceDimension=D3D10DDIRESOURCE_TEXTURECUBE;args.TexCube={range[0],range[1],range[2]};
      Storage failed(f.output.table.pfnCalcPrivateDepthStencilViewSize(f.device,&args));ok();failed.poison();
      rejection(f,true,[&]{f.output.table.pfnCreateDepthStencilView(f.device,&args,{failed.data()},{});},failed);
    } else {
      D3D10DDIARG_CREATERENDERTARGETVIEW args{};args.hDrvResource=cube.native->handle;args.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
      args.ResourceDimension=D3D10DDIRESOURCE_TEXTURECUBE;args.TexCube={range[0],range[1],range[2]};
      Storage failed(f.output.table.pfnCalcPrivateRenderTargetViewSize(f.device,&args));ok();failed.poison();
      rejection(f,false,[&]{f.output.table.pfnCreateRenderTargetView(f.device,&args,{failed.data()},{});},failed);
    }
  }
  if constexpr (Interface == 2) {
    if (depth) for (UINT flag : {4u,UINT(-1)}) {
      D3D11DDIARG_CREATEDEPTHSTENCILVIEW args{};args.hDrvResource=cube.native->handle;args.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
      args.ResourceDimension=D3D10DDIRESOURCE_TEXTURECUBE;args.TexCube={1,5,3};args.Flags=flag;
      Storage failed(f.output.table.pfnCalcPrivateDepthStencilViewSize(f.device,&args));ok();failed.poison();
      rejection(f,true,[&]{f.output.table.pfnCreateDepthStencilView(f.device,&args,{failed.data()},{});},failed);
    }
  }
  if constexpr (Interface == 0) {
    auto arrayArgs = cube.args; arrayArgs.ArraySize = 18; arrayArgs.pInitialDataUP = nullptr;
    Storage failed(f.output.table.pfnCalcPrivateResourceSize(f.device,&arrayArgs));ok();failed.poison();
    rejection(f,depth,[&]{f.output.table.pfnCreateResource(f.device,&arrayArgs,{failed.data()},{});},failed);
  }
  cube.read(expected,4); f.bind();
  if(depth) f.output.table.pfnDestroyDepthStencilView(f.device,dsHandle);else f.output.table.pfnDestroyRenderTargetView(f.device,rtHandle);
  ok();retained->guards();
}

static void descriptorControls() {
  D3D11_TEXTURE2D_DESC resource{};resource.Width=resource.Height=8;resource.MipLevels=4;resource.ArraySize=18;
  resource.SampleDesc.Count=1;resource.MiscFlags=D3D11_RESOURCE_MISC_TEXTURECUBE;resource.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_DEPTH_STENCIL;
  for(UINT first=0;first<=19;++first) for(UINT count=0;count<=19;++count) for(UINT mip=0;mip<=4;++mip) {
    const bool valid=mip<4 && count>0 && UINT64(first)+count<=18;
    D3D11_RENDER_TARGET_VIEW_DESC rt{};std::memset(&rt,0xa5,sizeof(rt));const auto oldRt=rt;
    const bool accepted=dxvk::umd::cubeArrayTargetViewDesc({mip,first,count},DXGI_FORMAT_R8G8B8A8_UNORM,resource,rt);CHECK(accepted==valid);
    if(!valid)CHECK(!std::memcmp(&rt,&oldRt,sizeof(rt)));
    else CHECK(rt.Texture2DArray.MipSlice==mip && rt.Texture2DArray.FirstArraySlice==first && rt.Texture2DArray.ArraySize==count);
    for(UINT flags=0;flags<=4;++flags) {
      D3D11_DEPTH_STENCIL_VIEW_DESC ds{};std::memset(&ds,0xa5,sizeof(ds));const auto oldDs=ds;
      CHECK(dxvk::umd::cubeArrayDepthViewDesc({mip,first,count},DXGI_FORMAT_D24_UNORM_S8_UINT,flags,resource,ds)==(valid && flags<4));
      if(!valid || flags==4)CHECK(!std::memcmp(&ds,&oldDs,sizeof(ds)));
      else CHECK(ds.Flags==flags && ds.Texture2DArray.MipSlice==mip && ds.Texture2DArray.FirstArraySlice==first && ds.Texture2DArray.ArraySize==count);
    }
  }
  D3D10DDIARG_CREATERENDERTARGETVIEW legacy{};legacy.ResourceDimension=D3D10DDIRESOURCE_TEXTURECUBE;legacy.Format=DXGI_FORMAT_R8G8B8A8_UNORM;legacy.TexCube={0,5,3};
  D3D11_RENDER_TARGET_VIEW_DESC rt{};CHECK(!dxvk::umd::textureTargetView(legacy,resource,rt));
  resource.ArraySize=6;legacy.TexCube={0,5,1};CHECK(dxvk::umd::textureTargetView(legacy,resource,rt));
  const auto validResource=resource;
  for(UINT malformed=0;malformed<11;++malformed) {
    resource=validResource;
    if(malformed==0)resource.Width=0;
    if(malformed==1)resource.Height=9;
    if(malformed==2)resource.ArraySize=7;
    if(malformed==3)resource.SampleDesc.Count=2;
    if(malformed==4)resource.SampleDesc.Quality=1;
    if(malformed==5)resource.MipLevels=0;
    if(malformed==6)resource.MipLevels=D3D11_REQ_MIP_LEVELS+1;
    if(malformed==7)resource.BindFlags=0;
    if(malformed==8)resource.MiscFlags=0;
    if(malformed==9)resource.ArraySize=UINT(-1);
    if(malformed==10)resource.Width=resource.Height=D3D11_REQ_TEXTURECUBE_DIMENSION+1;
    D3D11_RENDER_TARGET_VIEW_DESC rejectedRt{};std::memset(&rejectedRt,0xa5,sizeof(rejectedRt));const auto originalRt=rejectedRt;
    CHECK(!dxvk::umd::cubeArrayTargetViewDesc({0,0,1},DXGI_FORMAT_R8G8B8A8_UNORM,resource,rejectedRt));
    CHECK(!std::memcmp(&rejectedRt,&originalRt,sizeof(rejectedRt)));
    D3D11_DEPTH_STENCIL_VIEW_DESC rejectedDs{};std::memset(&rejectedDs,0xa5,sizeof(rejectedDs));const auto originalDs=rejectedDs;
    CHECK(!dxvk::umd::cubeArrayDepthViewDesc({0,0,1},DXGI_FORMAT_D24_UNORM_S8_UINT,0,resource,rejectedDs));
    CHECK(!std::memcmp(&rejectedDs,&originalDs,sizeof(rejectedDs)));
  }
}
int main() {
  callerThread=GetCurrentThreadId(); descriptorControls();
  exercise<0>(false);exercise<0>(true);exercise<1>(false);exercise<1>(true);exercise<2>(false);exercise<2>(true);
  CHECK(backendCalls==6 && views==24 && snapshots==30 && comparedWords==35700 && callbacks==46 && alternateCallbacks==46);
  std::printf("typed cube-array targets verified checks=%u views=24 snapshots=30 words=35700 callbacks=46 public_reference=1 hardware_admission=0\n",checks);
}
