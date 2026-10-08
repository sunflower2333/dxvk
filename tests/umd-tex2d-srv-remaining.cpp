// SPDX-License-Identifier: MIT
// Source-linked typed DDI controls with Microsoft's public WARP reference.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_result.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <type_traits>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks, callbacks, alternateCallbacks, backendCalls, views, sampledWords;
static HRESULT lastError = S_OK;
static DWORD callerThread;
static const LUID expectedLuid = {0x73524618, -91};
static ComPtr<ID3D11DeviceContext> createdContext;
#define CHECK(x) do { ++checks; if (!(x)) { \
  std::fprintf(stderr, "Texture2D SRV failure line=%d expression=%s checks=%u callbacks=%u HRESULT=%08lx\n", \
    __LINE__, #x, checks, callbacks, static_cast<unsigned long>(lastError)); std::exit(1); \
} } while (0)
static void ok() { CHECK(lastError == S_OK); }

static_assert(std::is_same_v<decltype(D3D10DDI_DEVICEFUNCS::pfnCreateShaderResourceView),
  PFND3D10DDI_CREATESHADERRESOURCEVIEW>, "original10 view ABI");
static_assert(std::is_same_v<decltype(D3D10_1DDI_DEVICEFUNCS::pfnCreateShaderResourceView),
  PFND3D10_1DDI_CREATESHADERRESOURCEVIEW>, "original10.1 view ABI");
static_assert(std::is_same_v<decltype(D3D11DDI_DEVICEFUNCS::pfnCreateShaderResourceView),
  PFND3D11DDI_CREATESHADERRESOURCEVIEW>, "original11 view ABI");

struct Storage {
  static constexpr UINT64 canary = 0x4e17d238ac5960bfull;
  std::vector<UINT64> words;
  explicit Storage(SIZE_T size) : words((size + 7) / 8 + 2, 0) {
    CHECK(size); words.front() = words.back() = canary;
  }
  void* data() { return words.data() + 1; }
  void poison() { std::fill(words.begin() + 1, words.end() - 1, 0xcdcdcdcdcdcdcdcdull); }
  void guards() const { CHECK(words.front() == canary && words.back() == canary); }
};
static const Storage* failedStorage;
static const std::vector<UINT64>* failedOriginal;
static ID3D11DeviceContext* failedContext;
static ID3D11ShaderResourceView* retainedBinding;

static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  CHECK(runtime.handle && GetCurrentThreadId() == callerThread && FAILED(result));
  ++callbacks; lastError = result;
  if (failedStorage) {
    CHECK(failedOriginal && failedStorage->words == *failedOriginal); failedStorage->guards();
    ComPtr<ID3D11ShaderResourceView> bound;
    failedContext->PSGetShaderResources(0, 1, &bound);
    CHECK(bound.Get() == retainedBinding);
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

template<unsigned Profile> struct Fixture {
  using Table = std::conditional_t<Profile == 2, D3D11DDI_DEVICEFUNCS,
    std::conditional_t<Profile == 1, D3D10_1DDI_DEVICEFUNCS, D3D10DDI_DEVICEFUNCS>>;
  using Core = std::conditional_t<Profile == 2, D3D11DDI_CORELAYER_DEVICECALLBACKS,
    D3D10DDI_CORELAYER_DEVICECALLBACKS>;
  using ResourceArgs = std::conditional_t<Profile == 2, D3D11DDIARG_CREATERESOURCE, D3D10DDIARG_CREATERESOURCE>;
  using ViewArgs = std::conditional_t<Profile == 2, D3D11DDIARG_CREATESHADERRESOURCEVIEW,
    std::conditional_t<Profile == 1, D3D10_1DDIARG_CREATESHADERRESOURCEVIEW, D3D10DDIARG_CREATESHADERRESOURCEVIEW>>;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{storage.data()};
  Core core{};
  struct { Table table{}; UINT64 canary = Storage::canary; } output;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Device> backend;
  UINT index = 0;
  Fixture() {
    core.pfnSetErrorCb = error;
    if constexpr (Profile == 2) {
      CHECK(VioGpuDxvkCreateDdiTestDevice11(&expectedLuid, device, {&core}, &core,
        &output.table, D3D_FEATURE_LEVEL_11_0) == S_OK);
    } else if constexpr (Profile == 1) {
      CHECK(VioGpuDxvkCreateDdiTestDevice10_1(&expectedLuid, device, {&core}, &core, &output.table) == S_OK);
    } else {
      CHECK(VioGpuDxvkCreateDdiTestDevice(&expectedLuid, device, {&core}, &core, &output.table) == S_OK);
    }
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

static UINT texel(UINT absoluteSlice, UINT absoluteMip, UINT x, UINT y) {
  return 0x11000000u | (absoluteSlice << 16) | (absoluteMip << 12) | (y << 6) | x;
}
struct Range { UINT mip, mips, slice, slices; };

template<unsigned Profile> struct Texture {
  Fixture<Profile>& f;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  UINT levels, slices, samples;
  DXGI_FORMAT format;
  Texture(Fixture<Profile>& owner, UINT arraySize, bool multisample = false,
      UINT binding = D3D10_DDI_BIND_SHADER_RESOURCE)
      : f(owner), storage(f.output.table.pfnCalcPrivateResourceSize(f.device, nullptr)),
        handle{storage.data()}, levels(multisample ? 1u : 4u), slices(arraySize),
        samples(multisample ? 4u : 1u),
        format(multisample ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R32_UINT) {
    if (multisample) {
      UINT qualities = 0;
      CHECK(f.backend->CheckMultisampleQualityLevels(format, samples, &qualities) == S_OK && qualities);
    }
    std::vector<D3D10DDI_MIPINFO> shapes(levels);
    std::vector<std::vector<UINT>> pixels(size_t(levels) * slices);
    std::vector<D3D10_DDIARG_SUBRESOURCE_UP> initial(pixels.size());
    for (UINT mip = 0; mip < levels; ++mip) {
      const UINT width = std::max(1u, 8u >> mip), height = std::max(1u, 4u >> mip);
      shapes[mip] = {width, height, 1, (width + 15u) & ~15u, (height + 15u) & ~15u, 1};
      for (UINT slice = 0; slice < slices; ++slice) {
        const UINT subresource = mip + slice * levels;
        for (UINT y = 0; y < height; ++y)
          for (UINT x = 0; x < width; ++x) pixels[subresource].push_back(texel(slice, mip, x, y));
        initial[subresource] = {pixels[subresource].data(), width * UINT(sizeof(UINT)), width * height * UINT(sizeof(UINT))};
      }
    }
    typename Fixture<Profile>::ResourceArgs args{};
    args.pMipInfoList = shapes.data(); args.pInitialDataUP = multisample ? nullptr : initial.data();
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    args.Usage = D3D10_DDI_USAGE_DEFAULT; args.BindFlags = binding;
    args.Format = format; args.SampleDesc.Count = samples; args.MipLevels = levels; args.ArraySize = slices;
    f.output.table.pfnCreateResource(f.device, &args, handle, {}); ok(); storage.guards();
  }
  ~Texture() { f.output.table.pfnDestroyResource(f.device, handle); ok(); storage.guards(); }
};
template<unsigned Profile> static typename Fixture<Profile>::ViewArgs viewArgs(
    const Texture<Profile>& resource, const Range& range) {
  typename Fixture<Profile>::ViewArgs args{};
  args.hDrvResource = resource.handle; args.Format = resource.format;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  args.Tex2D.MostDetailedMip = range.mip; args.Tex2D.MipLevels = range.mips;
  args.Tex2D.FirstArraySlice = range.slice; args.Tex2D.ArraySize = range.slices;
  return args;
}
template<unsigned Profile> struct View {
  Fixture<Profile>& f;
  Storage storage;
  D3D10DDI_HSHADERRESOURCEVIEW handle;
  View(Fixture<Profile>& owner, const typename Fixture<Profile>::ViewArgs& args)
      : f(owner), storage(f.output.table.pfnCalcPrivateShaderResourceViewSize(f.device, &args)),
        handle{storage.data()} {
    f.output.table.pfnCreateShaderResourceView(f.device, &args, handle, {}); ok(); storage.guards();
  }
  ComPtr<ID3D11ShaderResourceView> bind() {
    f.output.table.pfnPsSetShaderResources(f.device, 0, 1, &handle); ok();
    ComPtr<ID3D11ShaderResourceView> bound;
    f.context->PSGetShaderResources(0, 1, &bound); CHECK(bound); return bound;
  }
  ~View() { f.output.table.pfnDestroyShaderResourceView(f.device, handle); ok(); storage.guards(); }
};
static void save(unsigned profile, UINT index, const char* suffix, const void* data, size_t bytes) {
  char name[128];
  CHECK(std::snprintf(name, sizeof(name), "tex2d-srv-%u-%03u.%s", profile, index, suffix) > 0);
  std::ofstream output(name, std::ios::binary); CHECK(output.is_open());
  output.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
  output.close(); CHECK(!output.fail());
}

struct Loader {
  ID3D11DeviceContext* context;
  std::array<ComPtr<ID3D11ComputeShader>, 2> shaders;
  ComPtr<ID3D11Buffer> constants, destination, staging;
  ComPtr<ID3D11UnorderedAccessView> output;
  static constexpr UINT capacity = 64;
  template<unsigned Profile> explicit Loader(Fixture<Profile>& f) : context(f.context.Get()) {
    const char* sources[] = {R"(
Texture2D<uint> source : register(t0);
RWStructuredBuffer<uint> result : register(u0);
cbuffer Shape : register(b0) { uint levels; uint slices; uint2 padding; }
[numthreads(1,1,1)] void main(uint3 id : SV_DispatchThreadID) {
  if (id.x >= 4 * levels * slices) return;
  uint mip = (id.x / 4) % levels, corner = id.x % 4;
  uint width, height, available; source.GetDimensions(mip, width, height, available);
  uint x = (corner & 1) ? width - 1 : 0, y = (corner & 2) ? height - 1 : 0;
  result[id.x] = source.Load(int3(x,y,mip));
})", R"(
Texture2DArray<uint> source : register(t0);
RWStructuredBuffer<uint> result : register(u0);
cbuffer Shape : register(b0) { uint levels; uint slices; uint2 padding; }
[numthreads(1,1,1)] void main(uint3 id : SV_DispatchThreadID) {
  if (id.x >= 4 * levels * slices) return;
  uint mip = (id.x / 4) % levels, slice = id.x / (4 * levels), corner = id.x % 4;
  uint width, height, availableSlices, availableMips;
  source.GetDimensions(mip, width, height, availableSlices, availableMips);
  uint x = (corner & 1) ? width - 1 : 0, y = (corner & 2) ? height - 1 : 0;
  result[id.x] = source.Load(int4(x,y,slice,mip));
})"};
    for (UINT i = 0; i < shaders.size(); ++i) {
      ComPtr<ID3DBlob> binary, diagnostics;
      const HRESULT result = D3DCompile(sources[i], std::strlen(sources[i]), "tex2d-srv-reference",
        nullptr, nullptr, "main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &binary, &diagnostics);
      if (FAILED(result) && diagnostics) std::fprintf(stderr, "%s\n", static_cast<const char*>(diagnostics->GetBufferPointer()));
      CHECK(result == S_OK);
      save(Profile, i, "hlsl", sources[i], std::strlen(sources[i]));
      save(Profile, i, "dxbc", binary->GetBufferPointer(), binary->GetBufferSize());
      CHECK(f.backend->CreateComputeShader(binary->GetBufferPointer(), binary->GetBufferSize(), nullptr, &shaders[i]) == S_OK);
    }
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = 16; buffer.Usage = D3D11_USAGE_DEFAULT; buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    CHECK(f.backend->CreateBuffer(&buffer, nullptr, &constants) == S_OK);
    buffer.ByteWidth = capacity * sizeof(UINT); buffer.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    buffer.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; buffer.StructureByteStride = sizeof(UINT);
    CHECK(f.backend->CreateBuffer(&buffer, nullptr, &destination) == S_OK);
    D3D11_UNORDERED_ACCESS_VIEW_DESC view{}; view.Format = DXGI_FORMAT_UNKNOWN;
    view.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; view.Buffer.NumElements = capacity;
    CHECK(f.backend->CreateUnorderedAccessView(destination.Get(), &view, &output) == S_OK);
    buffer.Usage = D3D11_USAGE_STAGING; buffer.BindFlags = buffer.MiscFlags = buffer.StructureByteStride = 0;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    CHECK(f.backend->CreateBuffer(&buffer, nullptr, &staging) == S_OK);
  }
  std::vector<UINT> load(ID3D11ShaderResourceView* view, bool array, UINT levels, UINT slices) {
    const UINT count = 4 * levels * slices; CHECK(count <= capacity);
    const UINT shape[4] = {levels, slices, 0, 0};
    const UINT poison[4] = {0xcdcdcdcdu, 0xcdcdcdcdu, 0xcdcdcdcdu, 0xcdcdcdcdu};
    context->ClearUnorderedAccessViewUint(output.Get(), poison);
    context->UpdateSubresource(constants.Get(), 0, nullptr, shape, 0, 0);
    context->CSSetShader(shaders[array ? 1 : 0].Get(), nullptr, 0);
    context->CSSetShaderResources(0, 1, &view);
    auto constant = constants.Get(); auto target = output.Get();
    context->CSSetConstantBuffers(0, 1, &constant); context->CSSetUnorderedAccessViews(0, 1, &target, nullptr);
    context->Dispatch(count, 1, 1);
    ID3D11UnorderedAccessView* emptyUav = nullptr; ID3D11ShaderResourceView* emptyView = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &emptyUav, nullptr); context->CSSetShaderResources(0, 1, &emptyView);
    context->CopyResource(staging.Get(), destination.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    CHECK(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped) == S_OK && mapped.pData);
    std::vector<UINT> data(capacity); std::memcpy(data.data(), mapped.pData, capacity * sizeof(UINT));
    context->Unmap(staging.Get(), 0);
    for (UINT i = count; i < capacity; ++i) CHECK(data[i] == poison[0]);
    data.resize(count); return data;
  }
};

// Construct the public descriptor from the original request, without calling
// the production resolver. The runtime separately interprets its sentinels.
template<unsigned Profile> static D3D11_SHADER_RESOURCE_VIEW_DESC publicDesc(
    const Texture<Profile>& resource, const Range& range) {
  D3D11_SHADER_RESOURCE_VIEW_DESC desc{}; desc.Format = resource.format;
  if (resource.samples > 1) {
    if (resource.slices == 1) desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
    else {
      desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
      desc.Texture2DMSArray = {range.slice, range.slices};
    }
  } else if (resource.slices == 1) {
    desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D = {range.mip, range.mips};
  } else {
    desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    desc.Texture2DArray = {range.mip, range.mips, range.slice, range.slices};
  }
  return desc;
}
template<unsigned Profile> static void checkDesc(const Texture<Profile>& texture,
    const Range& range, const D3D11_SHADER_RESOURCE_VIEW_DESC& desc, UINT mips, UINT slices) {
  CHECK(desc.Format == texture.format);
  if (texture.samples > 1) {
    CHECK(desc.ViewDimension == (texture.slices == 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS
      : D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY));
    if (texture.slices > 1)
      CHECK(desc.Texture2DMSArray.FirstArraySlice == range.slice && desc.Texture2DMSArray.ArraySize == slices);
  } else if (texture.slices == 1) {
    CHECK(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D);
    CHECK(desc.Texture2D.MostDetailedMip == range.mip && desc.Texture2D.MipLevels == mips);
  } else {
    CHECK(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY);
    CHECK(desc.Texture2DArray.MostDetailedMip == range.mip && desc.Texture2DArray.MipLevels == mips
      && desc.Texture2DArray.FirstArraySlice == range.slice && desc.Texture2DArray.ArraySize == slices);
  }
}
template<unsigned Profile> static void validRanges(Fixture<Profile>& f,
    Texture<Profile>& texture, Loader& loader) {
  for (UINT slice = 0; slice < texture.slices; ++slice)
    for (UINT mip = 0; mip < texture.levels; ++mip)
      for (UINT requestedMips : texture.samples == 1
          ? std::vector<UINT>{1, texture.levels - mip, UINT(-1)} : std::vector<UINT>{1, UINT(-1)})
        for (UINT requestedSlices : {1u, texture.slices - slice, UINT(-1)}) {
          const Range range{mip, requestedMips, slice, requestedSlices};
          const UINT mips = requestedMips == UINT(-1) ? texture.levels - mip : requestedMips;
          const UINT slices = requestedSlices == UINT(-1) ? texture.slices - slice : requestedSlices;
          View<Profile> native(f, viewArgs(texture, range)); auto bound = native.bind();
          D3D11_SHADER_RESOURCE_VIEW_DESC observed{}; bound->GetDesc(&observed);
          checkDesc(texture, range, observed, mips, slices);
          ComPtr<ID3D11Resource> resource; bound->GetResource(&resource); CHECK(resource);
          auto desc = publicDesc(texture, range);
          ComPtr<ID3D11ShaderResourceView> reference;
          CHECK(f.backend->CreateShaderResourceView(resource.Get(), &desc, &reference) == S_OK);
          D3D11_SHADER_RESOURCE_VIEW_DESC publicObserved{}; reference->GetDesc(&publicObserved);
          checkDesc(texture, range, publicObserved, mips, slices);
          const UINT metadata[] = {8, 4, texture.levels, texture.slices, texture.samples,
            range.mip, range.mips, range.slice, range.slices, mips, slices};
          save(Profile, f.index, "metadata", metadata, sizeof(metadata));
          if (texture.samples == 1) {
            const auto nativeData = loader.load(bound.Get(), texture.slices > 1, mips, slices);
            const auto referenceData = loader.load(reference.Get(), texture.slices > 1, mips, slices);
            std::vector<UINT> expected;
            for (UINT relativeSlice = 0; relativeSlice < slices; ++relativeSlice)
              for (UINT relativeMip = 0; relativeMip < mips; ++relativeMip)
                for (UINT corner = 0; corner < 4; ++corner) {
                  const UINT absoluteMip = mip + relativeMip;
                  const UINT width = std::max(1u, 8u >> absoluteMip), height = std::max(1u, 4u >> absoluteMip);
                  expected.push_back(texel(slice + relativeSlice, absoluteMip,
                    corner & 1 ? width - 1 : 0, corner & 2 ? height - 1 : 0));
                }
            CHECK(nativeData.size() == expected.size() && referenceData.size() == expected.size());
            for (size_t i = 0; i < expected.size(); ++i) {
              CHECK(nativeData[i] == expected[i]); CHECK(referenceData[i] == expected[i]); ++sampledWords;
            }
            save(Profile, f.index, "native.words", nativeData.data(), nativeData.size() * sizeof(UINT));
            save(Profile, f.index, "public.words", referenceData.data(), referenceData.size() * sizeof(UINT));
          }
          ++f.index; ++views;
        }
}

template<unsigned Profile> static void failedRanges(Fixture<Profile>& f,
    Texture<Profile>& texture, Texture<Profile>& multisample) {
  View<Profile> retained(f, viewArgs(texture, {1, UINT(-1), 1, UINT(-1)}));
  auto bound = retained.bind();
  auto args = viewArgs(texture, {0, UINT(-1), 0, UINT(-1)});
  Storage failed(f.output.table.pfnCalcPrivateShaderResourceViewSize(f.device, &args));
  failed.poison(); const auto original = failed.words; const D3D10DDI_HSHADERRESOURCEVIEW handle{failed.data()};
  f.core.pfnSetErrorCb = alternateError; const UINT before = alternateCallbacks;
  auto reject = [&](const typename Fixture<Profile>::ViewArgs* input) {
    failedStorage = &failed; failedOriginal = &original;
    failedContext = f.context.Get(); retainedBinding = bound.Get();
    const UINT errors = callbacks;
    f.output.table.pfnCreateShaderResourceView(f.device, input, handle, {});
    CHECK(callbacks == errors + 1 && lastError == dxvk::umd::ddiResult(E_INVALIDARG)); lastError = S_OK;
    failedStorage = nullptr; failedOriginal = nullptr; failedContext = nullptr; retainedBinding = nullptr;
    CHECK(failed.words == original); failed.guards();
    ComPtr<ID3D11ShaderResourceView> current;
    f.context->PSGetShaderResources(0, 1, &current); CHECK(current.Get() == bound.Get());
  };
  const Range invalid[] = {
    {texture.levels, UINT(-1), 0, UINT(-1)}, {UINT(-1), UINT(-1), 0, UINT(-1)},
    {0, UINT(-1), texture.slices, UINT(-1)}, {0, UINT(-1), UINT(-1), UINT(-1)},
    {0, 0, 0, UINT(-1)}, {0, UINT(-2), 0, UINT(-1)}, {0, texture.levels + 1, 0, 1}, {1, texture.levels, 0, 1},
    {0, UINT(-1), 0, 0}, {0, UINT(-1), 0, UINT(-2)}, {0, 1, 0, texture.slices + 1}, {0, 1, 1, texture.slices}
  };
  for (const auto& range : invalid) { args = viewArgs(texture, range); reject(&args); }
  args = viewArgs(texture, {0, 1, 0, 1}); args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE1D; reject(&args);
  args = viewArgs(texture, {0, 1, 0, 1}); args.Format = DXGI_FORMAT_R8G8B8A8_UNORM; reject(&args);
  reject(nullptr);
  Texture<Profile> noBinding(f, 1, false, D3D10_DDI_BIND_RENDER_TARGET);
  args = viewArgs(noBinding, {0, UINT(-1), 0, UINT(-1)}); reject(&args);
  const Range invalidMS[] = {{1, UINT(-1), 0, 1}, {0, 2, 0, 1}, {0, 0, 0, 1},
    {0, UINT(-1), multisample.slices, UINT(-1)}, {0, UINT(-1), 0, multisample.slices + 1}};
  for (const auto& range : invalidMS) { args = viewArgs(multisample, range); reject(&args); }
  CHECK(alternateCallbacks == before + 21);
  // The same untouched private output can later acquire a valid view.
  args = viewArgs(texture, {texture.levels - 1, UINT(-1), texture.slices - 1, UINT(-1)});
  f.output.table.pfnCreateShaderResourceView(f.device, &args, handle, {}); ok(); failed.guards();
  f.output.table.pfnDestroyShaderResourceView(f.device, handle); ok(); failed.guards();
}

template<unsigned Profile> static void exercise() {
  Fixture<Profile> f; Loader loader(f);
  Texture<Profile> single(f, 1), array(f, 3), singleMS(f, 1, true), arrayMS(f, 3, true);
  validRanges(f, single, loader); validRanges(f, array, loader);
  validRanges(f, singleMS, loader); validRanges(f, arrayMS, loader);
  failedRanges(f, array, arrayMS);
  CHECK(f.index == 168);
}
int main() {
  callerThread = GetCurrentThreadId();
  std::puts("BACKEND=WARP\nproduction_DDI=true\nhardware_admission=false\nregistration=false");
  exercise<0>(); exercise<1>(); exercise<2>();
  CHECK(backendCalls == 3 && views == 504 && sampledWords == 5184 && callbacks == 63 && alternateCallbacks == 63);
  std::printf("native D3D10/D3D10.1/D3D11 Texture2D SRV remaining ranges verified checks=%u views=%u words=%u callbacks=%u WARP controls\n",
    checks, views, sampledWords, callbacks);
}
