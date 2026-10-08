// SPDX-License-Identifier: MIT
// Exact typed D3D11 callbacks, reconstructed compute and original FXC WARP.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_d3d11_desc.h"
#include "../src/umd/umd_result.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks, callbacks, alternateCallbacks, backendCalls, viewCount, readWords;
static HRESULT lastError = S_OK;
static DWORD callerThread;
static const LUID expectedLuid{0x5a492718, -119};
static ComPtr<ID3D11DeviceContext> createdContext;
#define CHECK(x) do { ++checks; if (!(x)) { \
  std::fprintf(stderr, "Texture UAV failure line=%d expression=%s checks=%u callbacks=%u HRESULT=%08lx\n", \
    __LINE__, #x, checks, callbacks, static_cast<unsigned long>(lastError)); std::exit(1); \
} } while (0)
static void ok() { CHECK(lastError == S_OK); }
static_assert(std::is_same_v<decltype(D3D11DDI_DEVICEFUNCS::pfnCreateUnorderedAccessView),
  PFND3D11DDI_CREATEUNORDEREDACCESSVIEW>, "original UAV create ABI");
static_assert(std::is_same_v<decltype(D3D11DDI_DEVICEFUNCS::pfnCsSetUnorderedAccessViews),
  PFND3D11DDI_SETUNORDEREDACCESSVIEWS>, "original compute UAV bind ABI");

struct Storage {
  static constexpr UINT64 canary = 0x2eac9845bc17063full;
  std::vector<UINT64> words;
  explicit Storage(SIZE_T size) : words((size + 7) / 8 + 2, 0) {
    CHECK(size); words.front() = words.back() = canary;
  }
  void* data() { return words.data() + 1; }
  void poison() { std::fill(words.begin() + 1, words.end() - 1, 0xcdcdcdcdcdcdcdcdull); }
  void guards() const { CHECK(words.front() == canary && words.back() == canary); }
};
static const Storage* failedStorage;
static const std::vector<UINT64>* failedBytes;
static ID3D11DeviceContext* failedContext;
static ID3D11UnorderedAccessView* retainedBinding;
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  CHECK(runtime.handle && GetCurrentThreadId() == callerThread && FAILED(result));
  ++callbacks; lastError = result;
  if (failedStorage) {
    CHECK(failedBytes && failedStorage->words == *failedBytes); failedStorage->guards();
    ComPtr<ID3D11UnorderedAccessView> current;
    failedContext->CSGetUnorderedAccessViews(0, 1, &current);
    CHECK(current.Get() == retainedBinding);
  }
}
static void APIENTRY alternateError(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  ++alternateCallbacks; error(runtime, result);
}
HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  CHECK(!runtime && !std::memcmp(&luid, &expectedLuid, sizeof(luid)));
  ++backendCalls; level = implementationFeatureLevel(level);
  const HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (result == S_OK) createdContext = *context;
  return result;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }

struct Fixture {
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{storage.data()};
  D3D11DDI_CORELAYER_DEVICECALLBACKS core{};
  struct { D3D11DDI_DEVICEFUNCS table{}; UINT64 canary = Storage::canary; } output;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Device> backend;
  Fixture() {
    core.pfnSetErrorCb = error;
    CHECK(VioGpuDxvkCreateDdiTestDevice11(&expectedLuid, device, {&core}, &core,
      &output.table, D3D_FEATURE_LEVEL_11_0) == S_OK);
    CHECK(output.canary == Storage::canary);
    context = createdContext; createdContext.Reset(); CHECK(context);
    context->GetDevice(&backend); CHECK(backend);
  }
  ~Fixture() {
    context->ClearState(); context.Reset(); backend.Reset();
    output.table.pfnDestroyDevice(device); ok(); storage.guards();
    CHECK(output.canary == Storage::canary);
  }
};

static void save(UINT index, const char* suffix, const void* bytes, size_t count) {
  char name[128];
  CHECK(std::snprintf(name, sizeof(name), "texture-uav-%03u.%s", index, suffix) > 0);
  std::ofstream output(name, std::ios::binary); CHECK(output.is_open());
  output.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(count));
  output.close(); CHECK(!output.fail());
}
static UINT initialWord(bool floating, UINT slice, UINT mip, UINT x, UINT y) {
  return (floating ? 0x3f000000u : 0x11000000u) | (slice << 16) | (mip << 12) | (y << 6) | x;
}
static UINT floatWord(FLOAT value) { UINT word; std::memcpy(&word, &value, sizeof(word)); return word; }
struct Range { UINT mip, first, count; };

struct Texture {
  Fixture& f;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  const UINT dimension, slices;
  const bool floating;
  const DXGI_FORMAT format;
  ComPtr<ID3D11Resource> reference;
  Texture(Fixture& owner, UINT kind, UINT arraySize, bool fp, bool staging = false,
      UINT bindings = D3D11_DDI_BIND_UNORDERED_ACCESS)
  : f(owner), storage(f.output.table.pfnCalcPrivateResourceSize(f.device, nullptr)),
    handle{storage.data()}, dimension(kind), slices(arraySize), floating(fp),
    format(fp ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R32_UINT) {
    std::array<D3D10DDI_MIPINFO, 3> shapes{};
    std::vector<std::vector<UINT>> pixels(3 * slices);
    std::vector<D3D10_DDIARG_SUBRESOURCE_UP> initial(pixels.size());
    std::vector<D3D11_SUBRESOURCE_DATA> publicInitial(pixels.size());
    for (UINT mip = 0; mip < 3; ++mip) {
      const UINT width = 8u >> mip, height = kind == 1 ? 1u : 4u >> mip;
      shapes[mip] = {width,height,1,width,height,1};
      for (UINT slice = 0; slice < slices; ++slice) {
        const UINT sub = mip + 3 * slice;
        for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x)
          pixels[sub].push_back(initialWord(fp, slice, mip, x, y));
        initial[sub] = {pixels[sub].data(), width * 4, width * height * 4};
        publicInitial[sub] = {pixels[sub].data(), width * 4, width * height * 4};
      }
    }
    D3D11DDIARG_CREATERESOURCE args{};
    args.pMipInfoList = shapes.data(); args.pInitialDataUP = staging ? nullptr : initial.data();
    args.ResourceDimension = kind == 1 ? D3D10DDIRESOURCE_TEXTURE1D : D3D10DDIRESOURCE_TEXTURE2D;
    args.Usage = staging ? D3D10_DDI_USAGE_STAGING : D3D10_DDI_USAGE_DEFAULT;
    args.BindFlags = staging ? 0 : bindings; args.MapFlags = staging ? D3D10_DDI_CPU_ACCESS_READ : 0;
    args.Format = format; args.SampleDesc.Count = 1; args.MipLevels = 3; args.ArraySize = slices;
    f.output.table.pfnCreateResource(f.device, &args, handle, {}); ok(); storage.guards();
    const UINT publicBindings = staging ? 0 : ((bindings & D3D11_DDI_BIND_UNORDERED_ACCESS)
      ? D3D11_BIND_UNORDERED_ACCESS : D3D11_BIND_SHADER_RESOURCE);
    if (kind == 1) {
      D3D11_TEXTURE1D_DESC desc{8,3,slices,format,staging ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT,
        publicBindings,staging ? D3D11_CPU_ACCESS_READ : 0u,0};
      ComPtr<ID3D11Texture1D> texture;
      CHECK(f.backend->CreateTexture1D(&desc, staging ? nullptr : publicInitial.data(), &texture) == S_OK);
      reference = texture;
    } else {
      D3D11_TEXTURE2D_DESC desc{8,4,3,slices,format,{1,0},staging ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT,
        publicBindings,staging ? D3D11_CPU_ACCESS_READ : 0u,0};
      ComPtr<ID3D11Texture2D> texture;
      CHECK(f.backend->CreateTexture2D(&desc, staging ? nullptr : publicInitial.data(), &texture) == S_OK);
      reference = texture;
    }
  }
  ~Texture() { f.output.table.pfnDestroyResource(f.device, handle); ok(); storage.guards(); }
};

static D3D11DDIARG_CREATEUNORDEREDACCESSVIEW nativeView(const Texture& texture, Range range) {
  D3D11DDIARG_CREATEUNORDEREDACCESSVIEW args{};
  args.hDrvResource = texture.handle; args.Format = texture.format;
  args.ResourceDimension = texture.dimension == 1 ? D3D10DDIRESOURCE_TEXTURE1D : D3D10DDIRESOURCE_TEXTURE2D;
  if (texture.dimension == 1) args.Tex1D = {range.mip,range.first,range.count};
  else args.Tex2D = {range.mip,range.first,range.count};
  return args;
}
static D3D11_UNORDERED_ACCESS_VIEW_DESC publicView(const Texture& texture, Range range) {
  // This reference conversion intentionally does not use the production helper.
  D3D11_UNORDERED_ACCESS_VIEW_DESC desc{}; desc.Format = texture.format;
  if (texture.dimension == 1) {
    if (texture.slices == 1) { desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D; desc.Texture1D.MipSlice = range.mip; }
    else { desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY; desc.Texture1DArray = {range.mip,range.first,range.count}; }
  } else {
    if (texture.slices == 1) { desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D; desc.Texture2D.MipSlice = range.mip; }
    else { desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY; desc.Texture2DArray = {range.mip,range.first,range.count}; }
  }
  return desc;
}
static void inspectDescriptor(const D3D11_UNORDERED_ACCESS_VIEW_DESC& actual,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC& expected) {
  CHECK(actual.Format == expected.Format && actual.ViewDimension == expected.ViewDimension);
  // GetDesc only gives meaning to the union member selected by ViewDimension.
  switch (expected.ViewDimension) {
    case D3D11_UAV_DIMENSION_TEXTURE1D:
      CHECK(actual.Texture1D.MipSlice == expected.Texture1D.MipSlice); break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
      CHECK(actual.Texture1DArray.MipSlice == expected.Texture1DArray.MipSlice);
      CHECK(actual.Texture1DArray.FirstArraySlice == expected.Texture1DArray.FirstArraySlice);
      CHECK(actual.Texture1DArray.ArraySize == expected.Texture1DArray.ArraySize); break;
    case D3D11_UAV_DIMENSION_TEXTURE2D:
      CHECK(actual.Texture2D.MipSlice == expected.Texture2D.MipSlice); break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      CHECK(actual.Texture2DArray.MipSlice == expected.Texture2DArray.MipSlice);
      CHECK(actual.Texture2DArray.FirstArraySlice == expected.Texture2DArray.FirstArraySlice);
      CHECK(actual.Texture2DArray.ArraySize == expected.Texture2DArray.ArraySize); break;
    default: CHECK(actual.ViewDimension == D3D11_UAV_DIMENSION_UNKNOWN);
  }
}
struct Uav {
  Fixture& f;
  Storage storage;
  D3D11DDI_HUNORDEREDACCESSVIEW handle;
  ComPtr<ID3D11UnorderedAccessView> native, reference;
  D3D11_UNORDERED_ACCESS_VIEW_DESC nativeDesc{}, publicDesc{};
  Uav(Fixture& owner, Texture& texture, Range range)
  : f(owner), storage(f.output.table.pfnCalcPrivateUnorderedAccessViewSize(f.device,nullptr)), handle{storage.data()} {
    const auto args = nativeView(texture,range);
    f.output.table.pfnCreateUnorderedAccessView(f.device,&args,handle,{}); ok(); storage.guards();
    f.output.table.pfnCsSetUnorderedAccessViews(f.device,0,1,&handle,nullptr); ok();
    f.context->CSGetUnorderedAccessViews(0,1,&native); CHECK(native);
    const auto expected = publicView(texture,range);
    CHECK(f.backend->CreateUnorderedAccessView(texture.reference.Get(),&expected,&reference) == S_OK);
    native->GetDesc(&nativeDesc); reference->GetDesc(&publicDesc);
    inspectDescriptor(nativeDesc,expected); inspectDescriptor(publicDesc,expected);
    ComPtr<ID3D11Resource> underlying; native->GetResource(&underlying); CHECK(underlying);
    if (texture.dimension == 1) {
      ComPtr<ID3D11Texture1D> typed; CHECK(underlying.As(&typed) == S_OK);
      D3D11_TEXTURE1D_DESC info{}; typed->GetDesc(&info);
      CHECK(info.ArraySize == texture.slices && info.MipLevels == 3 && info.Width == 8);
    } else {
      ComPtr<ID3D11Texture2D> typed; CHECK(underlying.As(&typed) == S_OK);
      D3D11_TEXTURE2D_DESC info{}; typed->GetDesc(&info);
      CHECK(info.ArraySize == texture.slices && info.MipLevels == 3 && info.Width == 8 && info.Height == 4);
      CHECK(info.SampleDesc.Count == 1 && !info.SampleDesc.Quality);
    }
  }
  ~Uav() {
    const D3D11DDI_HUNORDEREDACCESSVIEW empty{};
    f.output.table.pfnCsSetUnorderedAccessViews(f.device,0,1,&empty,nullptr); ok();
    native.Reset(); reference.Reset();
    f.output.table.pfnDestroyUnorderedAccessView(f.device,handle); ok(); storage.guards();
  }
};

struct Compute {
  Fixture& f;
  Storage storage;
  D3D10DDI_HSHADER handle;
  ComPtr<ID3D11ComputeShader> reference;
  Compute(Fixture& owner, UINT index, UINT dimension, bool floating, bool array)
  : f(owner), storage(f.output.table.pfnCalcPrivateShaderSize(f.device,nullptr,nullptr)), handle{storage.data()} {
    std::string source = "RWTexture" + std::string(dimension == 1 ? "1D" : "2D")
      + (array ? "Array" : "") + "<" + (floating ? "float" : "uint") + "> image:register(u0);\n"
      "[numthreads(1,1,1)]void main(uint3 id:SV_DispatchThreadID){image[";
    source += dimension == 1 ? (array ? "uint2(id.x,id.z)" : "id.x") : (array ? "id" : "id.xy");
    source += floating ? "]=float(1024+id.x+16*id.y+256*id.z);}\n"
      : "]=0x50000000u|id.x|(id.y<<8)|(id.z<<16);}\n";
    ComPtr<ID3DBlob> binary, diagnostic;
    const HRESULT result = D3DCompile(source.data(),source.size(),"typed-texture-uav",nullptr,nullptr,
      "main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&binary,&diagnostic);
    if (result != S_OK && diagnostic) std::fprintf(stderr,"%s\n",static_cast<const char*>(diagnostic->GetBufferPointer()));
    CHECK(result == S_OK); CHECK(binary);
    const auto* bytes = static_cast<const unsigned char*>(binary->GetBufferPointer());
    const size_t size = binary->GetBufferSize();
    save(index,"hlsl",source.data(),source.size()); save(index,"dxbc",bytes,size);
    auto word = [&](size_t offset) { CHECK(offset <= size && size - offset >= 4); UINT value; std::memcpy(&value,bytes+offset,4); return value; };
    CHECK(size >= 32 && !std::memcmp(bytes,"DXBC",4) && word(24) == size);
    const UINT chunks = word(28); CHECK(chunks && chunks <= (size-32)/4);
    std::vector<UINT> code;
    for (UINT i = 0; i < chunks; ++i) {
      const UINT offset = word(32+4*i); CHECK(offset <= size && size-offset >= 8);
      const UINT length = word(offset+4); CHECK(length <= size-offset-8);
      if (word(offset) == 0x58454853u) {
        CHECK(code.empty() && length >= 8 && !(length%4));
        code.resize(length/4); std::memcpy(code.data(),bytes+offset+8,length);
      }
    }
    CHECK(!code.empty() && code[0] == 0x50050u && code[1] == code.size());
    save(index,"shex",code.data(),code.size()*4);
    CHECK(f.backend->CreateComputeShader(bytes,size,nullptr,&reference) == S_OK);
    f.output.table.pfnCreateComputeShader(f.device,code.data(),handle,{}); ok(); storage.guards();
    std::fill(code.begin(),code.end(),0xcdcdcdcdu);
  }
  ~Compute() { f.output.table.pfnDestroyShader(f.device,handle); ok(); storage.guards(); }
};

static std::vector<UINT> readTexture(Texture& texture, bool native) {
  auto& f = texture.f;
  Texture staging(f,texture.dimension,texture.slices,texture.floating,true);
  if (native) { f.output.table.pfnResourceCopy(f.device,staging.handle,texture.handle); ok(); }
  else f.context->CopyResource(staging.reference.Get(),texture.reference.Get());
  std::vector<UINT> output;
  for (UINT slice = 0; slice < texture.slices; ++slice) for (UINT mip = 0; mip < 3; ++mip) {
    const UINT width = 8u >> mip, height = texture.dimension == 1 ? 1u : 4u >> mip;
    D3D10DDI_MAPPED_SUBRESOURCE mapped{};
    D3D11_MAPPED_SUBRESOURCE publicMapped{};
    const UINT sub = mip + 3*slice;
    if (native) { f.output.table.pfnStagingResourceMap(f.device,staging.handle,sub,D3D10_DDI_MAP_READ,0,&mapped); ok(); }
    else {
      CHECK(f.context->Map(staging.reference.Get(),sub,D3D11_MAP_READ,0,&publicMapped) == S_OK);
      mapped = {publicMapped.pData,publicMapped.RowPitch,publicMapped.DepthPitch};
    }
    CHECK(mapped.pData && mapped.RowPitch >= 4*width);
    for (UINT y = 0; y < height; ++y) {
      const auto* row = static_cast<const unsigned char*>(mapped.pData)+size_t(y)*mapped.RowPitch;
      for (UINT x = 0; x < width; ++x) { UINT value; std::memcpy(&value,row+4*x,4); output.push_back(value); }
    }
    if (native) { f.output.table.pfnStagingResourceUnmap(f.device,staging.handle,sub); ok(); }
    else f.context->Unmap(staging.reference.Get(),sub);
  }
  return output;
}
static std::vector<UINT> expectedWords(const Texture& texture, Range range, bool cleared) {
  std::vector<UINT> expected;
  for (UINT slice = 0; slice < texture.slices; ++slice) for (UINT mip = 0; mip < 3; ++mip) {
    const UINT width = 8u >> mip, height = texture.dimension == 1 ? 1u : 4u >> mip;
    for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
      UINT value = initialWord(texture.floating,slice,mip,x,y);
      if (mip == range.mip && slice >= range.first && slice-range.first < range.count) {
        if (cleared) value = texture.floating ? floatWord(0.375f) : 0x2468ace1u;
        else if (texture.floating) value = floatWord(FLOAT(1024+x+16*y+256*(slice-range.first)));
        else value = 0x50000000u | x | (y<<8) | ((slice-range.first)<<16);
      }
      expected.push_back(value);
    }
  }
  return expected;
}
static void checkReadback(UINT index, Texture& texture, Range range, bool cleared) {
  const auto expected = expectedWords(texture,range,cleared);
  const auto native = readTexture(texture,true), reference = readTexture(texture,false);
  CHECK(native.size() == expected.size() && reference.size() == expected.size());
  for (size_t i = 0; i < expected.size(); ++i) { CHECK(native[i] == expected[i]); CHECK(reference[i] == expected[i]); }
  readWords += static_cast<unsigned>(expected.size());
  save(index,cleared ? "clear.native.words" : "compute.native.words",native.data(),native.size()*4);
  save(index,cleared ? "clear.public.words" : "compute.public.words",reference.data(),reference.size()*4);
}

static void rejectView(Fixture& f, const D3D11DDIARG_CREATEUNORDEREDACCESSVIEW* args) {
  Storage untouched(f.output.table.pfnCalcPrivateUnorderedAccessViewSize(f.device,nullptr));
  untouched.poison(); const auto before = untouched.words;
  ComPtr<ID3D11UnorderedAccessView> bound; f.context->CSGetUnorderedAccessViews(0,1,&bound); CHECK(bound);
  failedStorage = &untouched; failedBytes = &before; failedContext = f.context.Get(); retainedBinding = bound.Get();
  const unsigned previous = callbacks;
  f.output.table.pfnCreateUnorderedAccessView(f.device,args,{untouched.data()},{});
  CHECK(callbacks == previous+1 && lastError == E_INVALIDARG);
  CHECK(untouched.words == before); untouched.guards();
  ComPtr<ID3D11UnorderedAccessView> after; f.context->CSGetUnorderedAccessViews(0,1,&after);
  CHECK(after.Get() == bound.Get());
  failedStorage = nullptr; failedBytes = nullptr; failedContext = nullptr; retainedBinding = nullptr;
  lastError = S_OK;
}
static void invalidViews(Fixture& f, Texture& texture) {
  auto rejectRange = [&](Range range) { const auto args = nativeView(texture,range); rejectView(f,&args); };
  rejectRange({0,0,0}); rejectRange({0,texture.slices,1}); rejectRange({0,0,texture.slices+1});
  rejectRange({3,0,1}); rejectRange({UINT(-1),0,1}); rejectRange({0,UINT(-1),1}); rejectRange({0,0,UINT(-1)});
  auto args = nativeView(texture,{0,0,1}); args.ResourceDimension = D3D10DDIRESOURCE_BUFFER; rejectView(f,&args);
  args = nativeView(texture,{0,0,1}); args.Format = DXGI_FORMAT_R32_TYPELESS;
  const auto publicArgs = publicView(texture,{0,0,1});
  auto badPublic = publicArgs; badPublic.Format = args.Format;
  ComPtr<ID3D11UnorderedAccessView> badReference;
  CHECK(f.backend->CreateUnorderedAccessView(texture.reference.Get(),&badPublic,&badReference) == E_INVALIDARG);
  CHECK(!badReference); rejectView(f,&args); rejectView(f,nullptr);
  Texture unbound(f,texture.dimension,texture.slices,texture.floating,false,D3D10_DDI_BIND_SHADER_RESOURCE);
  args = nativeView(unbound,{0,0,1}); rejectView(f,&args);
}
static void descriptorFailures() {
  const D3D11DDIARG_TEX1D_UNORDEREDACCESSVIEW view1{0,0,1};
  const D3D11DDIARG_TEX2D_UNORDEREDACCESSVIEW view2{0,0,1};
  D3D11_TEXTURE1D_DESC resource1{8,3,1,DXGI_FORMAT_R32_UINT,D3D11_USAGE_DEFAULT,D3D11_BIND_UNORDERED_ACCESS,0,0};
  D3D11_TEXTURE2D_DESC resource2{8,4,3,1,DXGI_FORMAT_R32_UINT,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_UNORDERED_ACCESS,0,0};
  D3D11_UNORDERED_ACCESS_VIEW_DESC output{};
  std::memset(&output,0xcd,sizeof(output)); const auto preserved = output;
  auto reject1 = [&](const auto& view, const auto& resource) {
    CHECK(!dxvk::umd::textureUnorderedView11Desc(view,DXGI_FORMAT_R32_UINT,resource,output));
    CHECK(!std::memcmp(&output,&preserved,sizeof(output)));
  };
  auto reject2 = reject1;
  auto noBind1 = resource1; noBind1.BindFlags = 0; reject1(view1,noBind1);
  auto noBind2 = resource2; noBind2.BindFlags = 0; reject2(view2,noBind2);
  auto multisample = resource2; multisample.SampleDesc.Count = 4; reject2(view2,multisample);
  multisample = resource2; multisample.SampleDesc.Count = 0; reject2(view2,multisample);
  auto missingMip1 = resource1; missingMip1.MipLevels = 0; reject1(view1,missingMip1);
  auto missingMip2 = resource2; missingMip2.MipLevels = 0; reject2(view2,missingMip2);
  auto noSlices1 = resource1; noSlices1.ArraySize = 0; reject1(view1,noSlices1);
  auto noSlices2 = resource2; noSlices2.ArraySize = 0; reject2(view2,noSlices2);
}
int main() {
  callerThread = GetCurrentThreadId(); descriptorFailures();
  {
    Fixture f;
    std::array<std::unique_ptr<Compute>,8> shaders;
    for (UINT kind = 1; kind <= 2; ++kind) for (UINT fp = 0; fp < 2; ++fp) for (UINT array = 0; array < 2; ++array) {
      const UINT index = (kind-1)*4+fp*2+array;
      shaders[index] = std::make_unique<Compute>(f,index,kind,fp != 0,array != 0);
    }
    for (UINT kind = 1; kind <= 2; ++kind) for (UINT fp = 0; fp < 2; ++fp) for (UINT slices : {1u,3u}) {
      const UINT shaderIndex = (kind-1)*4+fp*2+(slices > 1 ? 1 : 0);
      for (UINT mip = 0; mip < 3; ++mip) for (Range range : std::vector<Range>(slices == 1
          ? std::vector<Range>{{mip,0,1}} : std::vector<Range>{{mip,0,1},{mip,0,3},{mip,1,1},{mip,1,2},{mip,2,1}})) {
        Texture texture(f,kind,slices,fp != 0); Uav view(f,texture,range);
        const UINT index = viewCount++;
        const std::array<UINT,11> metadata{kind,UINT(texture.format),8,kind == 1 ? 1u : 4u,3,slices,
          range.mip,range.first,range.count,UINT(view.nativeDesc.ViewDimension),UINT(view.publicDesc.ViewDimension)};
        save(index,"metadata",metadata.data(),sizeof(metadata));
        if (mip == 0 && !range.first && range.count == 1) {
          invalidViews(f,texture); f.core.pfnSetErrorCb = alternateError;
        }
        auto& shader = *shaders[shaderIndex];
        f.output.table.pfnCsSetShader(f.device,shader.handle);
        f.output.table.pfnCsSetUnorderedAccessViews(f.device,0,1,&view.handle,nullptr); ok();
        const UINT width = 8u >> mip, height = kind == 1 ? 1u : 4u >> mip;
        f.output.table.pfnDispatch(f.device,width,height,range.count); ok();
        const D3D11DDI_HUNORDEREDACCESSVIEW empty{};
        f.output.table.pfnCsSetUnorderedAccessViews(f.device,0,1,&empty,nullptr); ok();
        f.context->CSSetShader(shader.reference.Get(),nullptr,0);
        ID3D11UnorderedAccessView* publicUav = view.reference.Get();
        f.context->CSSetUnorderedAccessViews(0,1,&publicUav,nullptr); f.context->Dispatch(width,height,range.count);
        publicUav = nullptr; f.context->CSSetUnorderedAccessViews(0,1,&publicUav,nullptr);
        checkReadback(index,texture,range,false);
        if (fp) {
          const FLOAT values[4]{0.375f,0.375f,0.375f,0.375f};
          f.output.table.pfnClearUnorderedAccessViewFloat(f.device,view.handle,values); ok();
          f.context->ClearUnorderedAccessViewFloat(view.reference.Get(),values);
        } else {
          const UINT values[4]{0x2468ace1u,0x2468ace1u,0x2468ace1u,0x2468ace1u};
          f.output.table.pfnClearUnorderedAccessViewUint(f.device,view.handle,values); ok();
          f.context->ClearUnorderedAccessViewUint(view.reference.Get(),values);
        }
        checkReadback(index,texture,range,true);
      }
    }
    f.output.table.pfnCsSetShader(f.device,{}); ok();
  }
  CHECK(viewCount == 72 && readWords == 10752 && callbacks == 88 && alternateCallbacks == 77 && backendCalls == 1);
  std::printf("D3D11 texture UAVs verified checks=%u views=72 words=10752 callbacks=88 original_files=384 WARP controls hardware_admission=0\n",checks);
}
