#include "../src/umd/umd_api.h"
#include "../src/umd/umd_adapter.h"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks, backendCalls;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "texture resource check %u line %d: %s\n", checks, __LINE__, #c); std::exit(1); } } while (0)

HRESULT dxvk::umd::createDevice(const LUID&, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  ++backendCalls;
  CHECK(level == D3D_FEATURE_LEVEL_10_0 && runtime && runtime->owner
    && runtime->create.owner == runtime->owner.get());
  // Only the backend factory is substituted. Original WDK CreateDevice,
  // CreateResource, Map/Copy/Update and destruction are production sources.
  return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &level, 1,
    D3D11_SDK_VERSION, device, nullptr, context);
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }

struct Device {
  std::vector<std::max_align_t> storage;
  D3D10DDI_HDEVICE handle{};
  D3D10DDI_DEVICEFUNCS table{};
  unsigned errors = 0;
  HRESULT lastError = S_OK;
  std::function<void()> onError;
  bool live = false;
  static void APIENTRY error(D3D10DDI_HRTCORELAYER core, HRESULT hr) {
    auto& self = *static_cast<Device*>(core.handle);
    CHECK(FAILED(hr)); ++self.errors; self.lastError = hr;
    auto hook = std::move(self.onError); self.onError = {};
    if (hook) hook();
  }
  Device() {
    storage.resize((VioGpuDxvkPrivateDeviceSize() + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    handle.pDrvPrivate = storage.data();
    auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
    identity->luid = {0x12348765, -31}; identity->generation = 13; identity->capabilities = 3;
    D3D10DDI_CORELAYER_DEVICECALLBACKS core{}; core.pfnSetErrorCb = error;
    D3DDDI_DEVICECALLBACKS kernel{};
    D3D10DDIARG_CREATEDEVICE args{};
    args.hDrvDevice = handle; args.hRTDevice.handle = this; args.hRTCoreLayer.handle = this;
    args.pUMCallbacks = &core; args.pKTCallbacks = &kernel; args.pDeviceFuncs = &table;
    CHECK(dxvk::umd::createAdapterDevice(identity, &args) == S_OK);
    live = true;
    // The runtime may overwrite its caller-owned input callback storage.
    core = {}; kernel = {};
  }
  ~Device() { if (live) table.pfnDestroyDevice(handle); }
};

struct Texture {
  Device& device;
  SIZE_T bytes;
  std::vector<std::max_align_t> storage;
  D3D10DDI_HRESOURCE handle{};
  bool live = false;
  Texture(Device& owner, const D3D10DDIARG_CREATERESOURCE& desc)
  : device(owner), bytes(owner.table.pfnCalcPrivateResourceSize(owner.handle, &desc)),
    storage((bytes + 64 + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t)) {
    CHECK(bytes >= sizeof(void*)); handle.pDrvPrivate = storage.data(); poison();
  }
  void poison() { std::memset(storage.data(), 0xcc, storage.size() * sizeof(std::max_align_t)); }
  void canary() const {
    const auto* data = reinterpret_cast<const unsigned char*>(storage.data());
    for (SIZE_T i = bytes; i < storage.size() * sizeof(std::max_align_t); ++i) CHECK(data[i] == 0xcc);
  }
  void untouched() const {
    const auto* data = reinterpret_cast<const unsigned char*>(storage.data());
    for (SIZE_T i = 0; i < storage.size() * sizeof(std::max_align_t); ++i) CHECK(data[i] == 0xcc);
  }
  void create(const D3D10DDIARG_CREATERESOURCE& desc, bool success = true) {
    const auto before = device.errors;
    device.table.pfnCreateResource(device.handle, &desc, handle, {this});
    CHECK(device.errors == before + unsigned(!success));
    if (success) live = true;
    canary();
  }
  void destroy() {
    device.table.pfnDestroyResource(device.handle, handle);
    live = false; poison();
    device.table.pfnDestroyResource(device.handle, handle);
    untouched();
  }
  ~Texture() { if (live) device.table.pfnDestroyResource(device.handle, handle); }
};

struct Data {
  std::array<D3D10DDI_MIPINFO, 4> mips{};
  D3D10DDIARG_CREATERESOURCE desc{};
  std::vector<std::vector<UINT>> pixels;
  std::vector<D3D10_DDIARG_SUBRESOURCE_UP> initial;
  explicit Data(bool volume) {
    desc.ResourceDimension = volume ? D3D10DDIRESOURCE_TEXTURE3D : D3D10DDIRESOURCE_TEXTURE1D;
    desc.Usage = D3D10_DDI_USAGE_DEFAULT; desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.MipLevels = 4; desc.ArraySize = volume ? 1 : 3;
    desc.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE;
    desc.pMipInfoList = mips.data();
    for (UINT mip = 0; mip < 4; ++mip) {
      const UINT w = std::max(1u, 9u >> mip);
      const UINT h = volume ? std::max(1u, 3u >> mip) : 1;
      const UINT d = volume ? std::max(1u, 5u >> mip) : 1;
      mips[mip] = {w, h, d, w, h, d};
    }
    const UINT count = desc.ArraySize * desc.MipLevels;
    pixels.resize(count); initial.resize(count);
    for (UINT index = 0; index < count; ++index) {
      const auto& size = mips[index % desc.MipLevels];
      // Deliberately padded rows and slices distinguish rowPitch/depthPitch
      // from tight packing and 3D depth from a 1D array subresource index.
      const UINT row = size.TexelWidth + 3, slice = row * (size.TexelHeight + 2);
      pixels[index].resize(slice * size.TexelDepth, 0xdecafbad);
      for (UINT z = 0; z < size.TexelDepth; ++z)
        for (UINT y = 0; y < size.TexelHeight; ++y)
          for (UINT x = 0; x < size.TexelWidth; ++x)
            pixels[index][z * slice + y * row + x] = value(index, x, y, z);
      initial[index].pSysMem = pixels[index].data();
      initial[index].SysMemPitch = row * 4; initial[index].SysMemSlicePitch = slice * 4;
    }
    desc.pInitialDataUP = initial.data();
  }
  static UINT value(UINT index, UINT x, UINT y, UINT z) {
    return 0xff000000 | (index << 16) | (z << 12) | (y << 8) | x;
  }
};

template<typename Expected>
static void read(Texture& texture, const Data& data, Expected&& expected) {
  auto& device = texture.device;
  const auto before = device.errors;
  for (UINT index = 0; index < data.desc.MipLevels * data.desc.ArraySize; ++index) {
    const auto& size = data.mips[index % data.desc.MipLevels];
    D3D10DDI_MAPPED_SUBRESOURCE mapped{};
    device.table.pfnStagingResourceMap(device.handle, texture.handle, index, D3D10_DDI_MAP_READ, 0, &mapped);
    CHECK(device.errors == before && mapped.pData);
    if (size.TexelHeight > 1) CHECK(mapped.RowPitch >= size.TexelWidth * 4);
    if (size.TexelDepth > 1) CHECK(mapped.DepthPitch >= mapped.RowPitch * size.TexelHeight);
    for (UINT z = 0; z < size.TexelDepth; ++z)
      for (UINT y = 0; y < size.TexelHeight; ++y) {
        const auto* row = reinterpret_cast<const UINT*>(static_cast<const char*>(mapped.pData)
          + size_t(z) * mapped.DepthPitch + size_t(y) * mapped.RowPitch);
        for (UINT x = 0; x < size.TexelWidth; ++x) CHECK(row[x] == expected(index, x, y, z));
      }
    device.table.pfnStagingResourceUnmap(device.handle, texture.handle, index);
    CHECK(device.errors == before);
  }
}

static void functional(bool volume) {
  Device device; Data data(volume);
  Texture source(device, data.desc); source.create(data.desc);
  auto staging = data.desc; staging.pInitialDataUP = nullptr;
  staging.Usage = D3D10_DDI_USAGE_STAGING; staging.BindFlags = 0;
  staging.MapFlags = D3D10_DDI_CPU_ACCESS_READ | D3D10_DDI_CPU_ACCESS_WRITE;
  Texture destination(device, staging); destination.create(staging);
  device.table.pfnResourceCopy(device.handle, destination.handle, source.handle);
  read(destination, data, Data::value);
  CHECK(!device.errors);

  // Duplicate CreateResource must leave the first live resource intact.
  source.create(data.desc, false);
  device.table.pfnResourceCopy(device.handle, destination.handle, source.handle);
  read(destination, data, Data::value);
  const unsigned previous = device.errors;
  const UINT index = volume ? 0 : 8;
  const D3D10_DDI_BOX box{2, 0, volume ? 1 : 0, 5, volume ? 2 : 1, volume ? 3 : 1};
  const UINT pitch = 7, slice = 28;
  std::array<UINT, 56> update{}; update.fill(0xaaff7711);
  device.table.pfnResourceUpdateSubresourceUP(device.handle, source.handle, index, &box,
    update.data(), pitch * 4, slice * 4);
  CHECK(device.errors == previous);
  device.table.pfnResourceCopy(device.handle, destination.handle, source.handle);
  auto changed = [index, volume](UINT sub, UINT x, UINT y, UINT z) {
    return sub == index && x >= 2 && x < 5 && y < (volume ? 2u : 1u)
      && z >= (volume ? 1u : 0u) && z < (volume ? 3u : 1u)
      ? 0xaaff7711 : Data::value(sub, x, y, z);
  };
  read(destination, data, changed);

  // Copy a bounded subvolume (including destination Z), or a 1D array mip,
  // preserving all unaffected texels and every other subresource.
  Data copyData(volume); Texture other(device, copyData.desc); other.create(copyData.desc);
  const D3D10_DDI_BOX copyBox{2, 0, volume ? 1 : 0, 5, volume ? 2 : 1, volume ? 3 : 1};
  device.table.pfnResourceCopyRegion(device.handle, other.handle, index, 4, volume ? 1 : 0,
    volume ? 2 : 0, source.handle, index, &copyBox);
  CHECK(device.errors == previous);
  device.table.pfnResourceCopy(device.handle, destination.handle, other.handle);
  read(destination, data, [index, volume](UINT sub, UINT x, UINT y, UINT z) {
    return sub == index && x >= 4 && x < 7 && y >= (volume ? 1u : 0u)
      && y < (volume ? 3u : 1u) && z >= (volume ? 2u : 0u) && z < (volume ? 4u : 1u)
      ? 0xaaff7711 : Data::value(sub, x, y, z);
  });

  // Bad pitches, invalid subresource, overflow-sized destinations and source
  // boxes must report an error before the backend can modify the resource.
  auto fail = [&](const std::function<void()>& operation) {
    const auto before = device.errors; operation(); CHECK(device.errors == before + 1);
  };
  fail([&] { device.table.pfnResourceCopyRegion(device.handle, other.handle, index,
    UINT_MAX, 0, 0, source.handle, index, &copyBox); });
  fail([&] { device.table.pfnResourceCopyRegion(device.handle, other.handle, index,
    0, 0, UINT_MAX, source.handle, index, &copyBox); });
  fail([&] { device.table.pfnResourceCopyRegion(device.handle, other.handle,
    data.desc.MipLevels * data.desc.ArraySize, 0, 0, 0, source.handle, index, &copyBox); });
  auto bad = copyBox; bad.back = 100;
  fail([&] { device.table.pfnResourceCopyRegion(device.handle, other.handle, index,
    0, 0, 0, source.handle, index, &bad); });
  if (volume) {
    fail([&] { device.table.pfnResourceUpdateSubresourceUP(device.handle, source.handle,
      index, &box, update.data(), 8, slice * 4); });
    fail([&] { device.table.pfnResourceUpdateSubresourceUP(device.handle, source.handle,
      index, &box, update.data(), pitch * 4, 1); });
    fail([&] { device.table.pfnResourceUpdateSubresourceUP(device.handle, source.handle,
      index, &box, update.data(), UINT_MAX, UINT_MAX); });
  }
  device.table.pfnResourceCopy(device.handle, destination.handle, source.handle);
  read(destination, data, changed);

  D3D10DDI_MAPPED_SUBRESOURCE mapping{};
  fail([&] { device.table.pfnStagingResourceMap(device.handle, destination.handle, 0,
    static_cast<D3D10_DDI_MAP>(0), 0, &mapping); });
  CHECK(!mapping.pData && !mapping.RowPitch && !mapping.DepthPitch);
  device.table.pfnStagingResourceMap(device.handle, destination.handle, 1,
    D3D10_DDI_MAP_WRITE, 0, &mapping);
  CHECK(mapping.pData); static_cast<UINT*>(mapping.pData)[0] = 0xfeed1234;
  device.table.pfnStagingResourceUnmap(device.handle, destination.handle, 1);
  read(destination, data, [&](UINT sub, UINT x, UINT y, UINT z) {
    return sub == 1 && !x && !y && !z ? 0xfeed1234 : changed(sub, x, y, z);
  });
  source.destroy(); source.create(data.desc);
  device.table.pfnResourceCopy(device.handle, destination.handle, source.handle);
  read(destination, data, Data::value);
}

static void failures(bool volume) {
  Device device; Data data(volume); Texture resource(device, data.desc);
  auto reject = [&](const D3D10DDIARG_CREATERESOURCE& invalid) {
    resource.create(invalid, false); resource.untouched();
    // No DestroyResource follows a failed CreateResource. Reusing its exact
    // private storage succeeds because the production reservation was removed.
    resource.create(data.desc); resource.destroy();
  };
  auto invalid = data.desc; invalid.SampleDesc.Count = 2; reject(invalid);
  invalid = data.desc; invalid.SampleDesc.Quality = 1; reject(invalid);
  invalid = data.desc; invalid.ArraySize = volume ? 2 : 0; reject(invalid);
  invalid = data.desc; invalid.MipLevels = 5; reject(invalid);
  invalid = data.desc; invalid.MiscFlags = D3D10_DDI_RESOURCE_MISC_SHARED; reject(invalid);
  DXGI_DDI_PRIMARY_DESC primary{};
  invalid = data.desc; invalid.pPrimaryDesc = &primary; reject(invalid);
  invalid = data.desc; invalid.BindFlags |= D3D10_DDI_BIND_PRESENT; reject(invalid);
  // Backend-rejected creation also leaves no placement-constructed object.
  invalid = data.desc; invalid.Format = DXGI_FORMAT_UNKNOWN; reject(invalid);
  invalid = data.desc; invalid.Usage = D3D10_DDI_USAGE_IMMUTABLE;
  invalid.pInitialDataUP = nullptr; reject(invalid);
  const auto old = data.mips;
  data.mips[0].TexelWidth = 0; resource.create(data.desc, false); resource.untouched(); data.mips = old;
  data.mips[2].TexelWidth = 99; resource.create(data.desc, false); resource.untouched(); data.mips = old;
  if (!volume) {
    data.mips[0].TexelDepth = 2; resource.create(data.desc, false); resource.untouched(); data.mips = old;
  }
  // Failed creation reports only after removing the reservation. SetError may
  // recursively destroy/reclaim and create a new object at the same address.
  bool reentered = false;
  device.onError = [&] {
    reentered = true; resource.destroy(); resource.create(data.desc);
  };
  invalid = data.desc; invalid.Format = DXGI_FORMAT_UNKNOWN;
  resource.create(invalid, false); CHECK(reentered && resource.live);
  auto staging = data.desc; staging.Usage = D3D10_DDI_USAGE_STAGING;
  staging.BindFlags = 0; staging.MapFlags = D3D10_DDI_CPU_ACCESS_READ; staging.pInitialDataUP = nullptr;
  Texture readback(device, staging); readback.create(staging);
  device.table.pfnResourceCopy(device.handle, readback.handle, resource.handle);
  read(readback, data, Data::value);

  Device foreign;
  const auto before = foreign.errors;
  D3D10DDI_MAPPED_SUBRESOURCE map{};
  foreign.table.pfnStagingResourceMap(foreign.handle, readback.handle, 0, D3D10_DDI_MAP_READ, 0, &map);
  CHECK(foreign.errors == before + 1 && !map.pData);
  foreign.table.pfnDestroyResource(foreign.handle, resource.handle);
  device.table.pfnResourceCopy(device.handle, readback.handle, resource.handle);
  read(readback, data, Data::value);
}

int main() {
  functional(false); functional(true); failures(false); failures(true);
  CHECK(backendCalls == 6);
  std::printf("PASS %u native texture resource checks; actual typed 1D/3D DDI lifecycle, padded readback, region/update isolation and failed-handle reuse; WARP fixture, not hardware/runtime admission\n", checks);
}
