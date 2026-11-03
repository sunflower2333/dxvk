#include "umd_backend.h"
#include "umd_api.h"
#include "umd_runtime_validation.h"
#include "../d3d11/d3d11_context_imm.h"
#include <cstring>
#include <new>

namespace dxvk::umd {
namespace {
class NativeImage final : public DxvkImageImport {
public:
  NativeImage(const RuntimeBackend& runtime, const mwd_allocation& allocation, uint32_t stride, bool preserve)
  : m_runtime(&runtime), m_allocation(allocation), m_stride(stride), m_preserve(preserve) { }
  ~NativeImage() override {
    if (m_image) m_vkd->vkDestroyImage(m_vkd->device(), m_image, nullptr);
    if (m_memory) m_vkd->vkFreeMemory(m_vkd->device(), m_memory, nullptr);
  }
  VkImage create(DxvkDevice* device, DxvkImageCreateInfo& info) override {
    if (m_image || !device || !device->nativeSurfaces()
        || !device->ownsRuntime(m_runtime.get()->create.owner)
        || info.type != VK_IMAGE_TYPE_2D || info.extent.depth != 1
        || info.mipLevels != 1 || info.numLayers != 1 || info.sampleCount != VK_SAMPLE_COUNT_1_BIT
        || !info.extent.width || !info.extent.height || info.viewFormatCount > 8
        || (info.viewFormatCount && !info.viewFormats)
        || info.sharing.mode != DxvkSharedHandleMode::None
        || m_stride < uint64_t(info.extent.width) * 4 || m_stride % 4
        || m_allocation.size < uint64_t(m_stride) * info.extent.height)
      throw DxvkError("Native image descriptor/owner mismatch");
    switch (info.format) {
      case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_SRGB:
      case VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_B8G8R8A8_SRGB: break;
      default: throw DxvkError("Native image format unsupported");
    }
    auto vki = device->instance()->vki();
    const auto physical = device->adapter()->handle();
    m_vkd = device->vkd();
    info.flags |= VK_IMAGE_CREATE_ALIAS_BIT;
    info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    info.layout = VK_IMAGE_LAYOUT_GENERAL;
    VkImageFormatListCreateInfo formats = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};
    formats.viewFormatCount = info.viewFormatCount; formats.pViewFormats = info.viewFormats;
    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierQuery = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT};
    modifierQuery.pNext = &formats; modifierQuery.drmFormatModifier = 0;
    modifierQuery.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkPhysicalDeviceImageFormatInfo2 query = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
    query.pNext = &modifierQuery; query.format = info.format; query.type = info.type;
    query.tiling = info.tiling; query.usage = info.usage; query.flags = info.flags;
    VkImageFormatProperties2 supported = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};
    check(vki->vkGetPhysicalDeviceImageFormatProperties2(physical, &query, &supported), "query-layout");
    if (info.extent.width > supported.imageFormatProperties.maxExtent.width
        || info.extent.height > supported.imageFormatProperties.maxExtent.height)
      throw DxvkError("Native image extent unsupported");
    VkSubresourceLayout plane = {}; plane.rowPitch = m_stride;
    VkImageDrmFormatModifierExplicitCreateInfoEXT modifier = {VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT};
    modifier.pNext = &formats; modifier.drmFormatModifier = 0;
    modifier.drmFormatModifierPlaneCount = 1; modifier.pPlaneLayouts = &plane;
    VkImageCreateInfo create = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    create.pNext = &modifier; create.flags = info.flags; create.imageType = info.type;
    create.format = info.format; create.extent = info.extent;
    create.mipLevels = info.mipLevels; create.arrayLayers = info.numLayers;
    create.samples = info.sampleCount; create.tiling = info.tiling; create.usage = info.usage;
    create.sharingMode = VK_SHARING_MODE_EXCLUSIVE; create.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    check(m_vkd->vkCreateImage(m_vkd->device(), &create, nullptr, &m_image), "create-image");
    VkMemoryRequirements requirements = {};
    m_vkd->vkGetImageMemoryRequirements(m_vkd->device(), m_image, &requirements);
    VkImageSubresource subresource = {VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT, 0, 0};
    VkSubresourceLayout layout = {};
    m_vkd->vkGetImageSubresourceLayout(m_vkd->device(), m_image, &subresource, &layout);
    if (layout.offset || layout.rowPitch != m_stride || !layout.size || layout.size > m_allocation.size
        || !requirements.size || requirements.size > m_allocation.size || !requirements.alignment
        || m_allocation.address % requirements.alignment)
      throw DxvkError("Native image layout differs from AHB backing");
    const auto& properties = device->adapter()->memoryProperties();
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
      if ((requirements.memoryTypeBits & (1u << i))
          && (properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
          && !(properties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT))) {
        type = i; break;
      }
    if (type == UINT32_MAX) throw DxvkError("Native image lacks device-only memory type");
    mwd_import_memory_info imported = {MWD_STYPE_IMPORT, nullptr, m_runtime.get()->create.owner, m_allocation.token};
    VkMemoryAllocateInfo allocate = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.pNext = &imported; allocate.allocationSize = m_allocation.size; allocate.memoryTypeIndex = type;
    check(m_vkd->vkAllocateMemory(m_vkd->device(), &allocate, nullptr, &m_memory), "import-memory");
    check(m_vkd->vkBindImageMemory(m_vkd->device(), m_image, m_memory, 0), "bind-memory");
    // The private MWD token imports existing linear AHB storage in GENERAL
    // layout. Keep the creator's contents when reopening it. This affects
    // DXVK's imported-resource state, never VkImageCreateInfo::initialLayout.
    if (m_preserve) info.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    return m_image;
  }
private:
  static void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw DxvkError(str::format("Native image ", operation, ": ", result));
  }
  RuntimeBackendSnapshot m_runtime;
  mwd_allocation m_allocation;
  uint32_t m_stride;
  bool m_preserve;
  Rc<vk::DeviceFn> m_vkd;
  VkImage m_image = VK_NULL_HANDLE;
  VkDeviceMemory m_memory = VK_NULL_HANDLE;
};
}

bool nativeSurfacesSupported(ID3D11Device* device) noexcept {
  return device && static_cast<D3D11Device*>(device)->GetDXVKDevice()->nativeSurfaces();
}
HRESULT rotateNativeTextureStorage(ID3D11DeviceContext* context, UINT count,
    ID3D11Texture2D* const* textures, const TextureStorageCommit& commit) noexcept {
  if (!context || !textures || count < 2 || count > 16
      || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) return E_INVALIDARG;
  try {
    auto immediate = static_cast<D3D11ImmediateContext*>(context);
    auto lock = immediate->LockContext();
    Com<ID3D11Device> owner; context->GetDevice(&owner);
    if (!nativeSurfacesSupported(owner.ptr())) return DXGI_ERROR_UNSUPPORTED;
    std::vector<Rc<DxvkImage>> images; images.reserve(count);
    for (UINT i = 0; i < count; ++i) {
      if (!textures[i]) return E_INVALIDARG;
      Com<ID3D11Device> device; textures[i]->GetDevice(&device);
      if (device != owner) return E_INVALIDARG;
      auto image = static_cast<D3D11Texture2D*>(textures[i])->GetCommonTexture()->GetImage();
      if (!image) return E_INVALIDARG;
      for (const auto& previous : images)
        if (previous == image) return E_INVALIDARG;
      images.push_back(std::move(image));
    }
    // Only fixed texture owners may be read before this drain: an ordinary
    // image's storage can still be changing on the CS thread via relocation.
    HRESULT hr = immediate->FlushRuntimeSubmission();
    if (hr != S_OK) return hr;
    std::vector<DxvkImageStorageIdentity> identities; identities.reserve(count);
    HRESULT prepared = E_FAIL;
    immediate->InjectCs(DxvkCsQueue::Ordered, [&](DxvkContext* commands) {
      try {
        bool anyNative = false;
        for (UINT i = 0; i < count; ++i) {
          const auto& image = images[i];
          if (image->mapPtr(0)) { prepared = E_INVALIDARG; return; }
          const auto& b = image->info();
          const bool native = image->storage()->hasExternalOwner();
          anyNative |= native;
          if (b.type != VK_IMAGE_TYPE_2D || b.mipLevels != 1 || b.numLayers != 1
              || b.sampleCount != VK_SAMPLE_COUNT_1_BIT || b.extent.depth != 1 || b.transient
              || (b.format != VK_FORMAT_B8G8R8A8_UNORM && b.format != VK_FORMAT_R8G8B8A8_UNORM)
              || b.sharing.mode != DxvkSharedHandleMode::None
              || (b.flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT))
              || (native ? b.tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT
                  || b.layout != VK_IMAGE_LAYOUT_GENERAL || !b.shared
                : b.tiling != VK_IMAGE_TILING_OPTIMAL || b.shared)) { prepared = E_INVALIDARG; return; }
          for (UINT previous = 0; previous < i; ++previous)
            if (images[previous]->storage() == image->storage()) { prepared = E_INVALIDARG; return; }
          if (i) {
            const auto& a = images[0]->info();
            if (a.format != b.format || ((a.flags ^ b.flags) & ~VK_IMAGE_CREATE_ALIAS_BIT)
                || a.usage != b.usage || a.stages != b.stages || a.access != b.access
                || a.extent.width != b.extent.width || a.extent.height != b.extent.height
                || a.viewFormatCount != b.viewFormatCount) { prepared = E_INVALIDARG; return; }
            for (uint32_t f = 0; f < a.viewFormatCount; ++f)
              if (a.viewFormats[f] != b.viewFormats[f]) { prepared = E_INVALIDARG; return; }
          }
        }
        if (!anyNative) { prepared = E_INVALIDARG; return; }
        // Materialize all deferred clears (including unbound images), restore
        // layouts, pin addresses, and reserve trackers before the commit.
        for (const auto& image : images) commands->prepareImageIdentityRotation(image);
        for (const auto& image : images) identities.push_back(image->storageIdentity());
        prepared = S_OK;
      } catch (const std::bad_alloc&) { prepared = E_OUTOFMEMORY; }
        catch (...) { prepared = DXGI_ERROR_DEVICE_REMOVED; }
    }, true);
    if (prepared != S_OK) return prepared;
    // The exact injected chunk is joined already. A failed Flush allocation
    // cannot leave a queued closure referring to this function's stack.
    hr = immediate->FlushRuntimeSubmission();
    if (hr != S_OK) return hr;
    DxvkContext* recording = nullptr;
    const std::function<void()> apply = [&] {
      for (size_t i = 0; i < images.size(); ++i)
        recording->commitImageIdentityRotation(images[i], identities[(i + 1) % images.size()]);
    };
    HRESULT result = E_FAIL;
    immediate->InjectCs(DxvkCsQueue::Ordered, [&](DxvkContext* commands) {
      recording = commands;
      try {
        if (commit) result = commit(apply);
        else { apply(); result = S_OK; }
      } catch (const std::bad_alloc&) { result = E_OUTOFMEMORY; }
        catch (...) { result = DXGI_ERROR_DEVICE_REMOVED; }
    }, true);
    // Commit changes only prepared CPU state. No RenderCb/Flush after either
    // storage or its runtime owner has moved; later draws see dirty views.
    // InjectCs(..., true) joins this exact commit; SynchronizeAll alone does
    // not cover an injected chunk that has no sequence number.
    return result;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return DXGI_ERROR_DEVICE_REMOVED; }
}
HRESULT prepareNativeTextureForExternalWrite(ID3D11DeviceContext* context, ID3D11Resource* resource) noexcept {
  if (!context || !resource || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) return E_INVALIDARG;
  try {
    Com<ID3D11Device> owner, actual;
    context->GetDevice(&owner); resource->GetDevice(&actual);
    if (owner != actual || !nativeSurfacesSupported(owner.ptr())) return E_INVALIDARG;
    Com<ID3D11Texture2D> texture;
    if (resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture)) != S_OK)
      return E_INVALIDARG;
    auto image = static_cast<D3D11Texture2D*>(texture.ptr())->GetCommonTexture()->GetImage();
    if (!image || !image->storage()->hasExternalOwner() || image->info().layout != VK_IMAGE_LAYOUT_GENERAL)
      return E_INVALIDARG;
    auto immediate = static_cast<D3D11ImmediateContext*>(context);
    auto lock = immediate->LockContext();
    immediate->InjectCs(DxvkCsQueue::Ordered, [image](DxvkContext* commands) {
      commands->prepareImageForExternalWrite(image);
    });
    return immediate->FlushRuntimeSubmission();
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return DXGI_ERROR_DEVICE_REMOVED; }
}
HRESULT waitNativeTextureIdle(ID3D11DeviceContext* context, ID3D11Resource* resource) noexcept {
  if (!context || !resource || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) return E_INVALIDARG;
  try {
    Com<ID3D11Device> owner, actual;
    context->GetDevice(&owner); resource->GetDevice(&actual);
    if (owner != actual || !nativeSurfacesSupported(owner.ptr())) return E_INVALIDARG;
    Com<ID3D11Texture2D> texture;
    if (resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture)) != S_OK)
      return E_INVALIDARG;
    auto image = static_cast<D3D11Texture2D*>(texture.ptr())->GetCommonTexture()->GetImage();
    if (!image || !image->storage()->hasExternalOwner() || image->info().layout != VK_IMAGE_LAYOUT_GENERAL)
      return E_INVALIDARG;
    auto immediate = static_cast<D3D11ImmediateContext*>(context);
    auto lock = immediate->LockContext();
    const HRESULT submitted = immediate->FlushRuntimeSubmission();
    if (submitted != S_OK) return FAILED(submitted) ? submitted : E_FAIL;
    auto device = static_cast<D3D11Device*>(owner.ptr())->GetDXVKDevice();
    // Read waits for both GPU reads and writes. A host FOREIGN acquire is
    // exclusive even for a Blt source, so submission completion is insufficient.
    device->waitForResource(*image, DxvkAccess::Read);
    // Failed submissions also retire use references; idle alone is not success.
    return device->getDeviceStatus() == VK_SUCCESS ? S_OK : DXGI_ERROR_DEVICE_REMOVED;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return DXGI_ERROR_DEVICE_REMOVED; }
}
HRESULT createNativeTexture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc,
    const RuntimeBackend& runtime, const mwd_allocation& allocation, uint32_t stride,
    ID3D11Texture2D** texture, bool preserveContents) noexcept {
  if (!texture) return E_POINTER;
  *texture = nullptr;
  if (!device || !validRuntimeBackend(runtime) || !allocation.token || !allocation.handle
      || !allocation.address || allocation.address % 4096 || !allocation.size || allocation.size % 4096
      || !allocation.generation || allocation.flags != (4u | MWD_ALLOCATION_NATIVE_SURFACE)
      || !desc.Width || !desc.Height || desc.Width > 16384 || desc.Height > 16384
      || desc.MipLevels != 1 || desc.ArraySize != 1 || desc.SampleDesc.Count != 1 || desc.SampleDesc.Quality
      || desc.Usage != D3D11_USAGE_DEFAULT || desc.CPUAccessFlags || desc.MiscFlags
      || (desc.BindFlags & ~(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE))) return E_INVALIDARG;
  if (!nativeSurfacesSupported(device)) return DXGI_ERROR_UNSUPPORTED;
  try {
    auto renderer = static_cast<D3D11Device*>(device);
    if (!renderer->GetDXVKDevice()->ownsRuntime(runtime.create.owner)) return E_INVALIDARG;
    mwd_context_info context = {};
    const HRESULT status = runtime.create.callbacks->context(runtime.create.owner, &context);
    if (status != S_OK) return FAILED(status) ? status : E_FAIL;
    if (context.generation != allocation.generation) return DXGI_ERROR_DEVICE_REMOVED;
    D3D11_COMMON_TEXTURE_DESC common = {};
    common.Width = desc.Width; common.Height = desc.Height; common.Depth = 1;
    common.MipLevels = desc.MipLevels; common.ArraySize = desc.ArraySize;
    common.Format = desc.Format; common.SampleDesc = desc.SampleDesc;
    common.Usage = desc.Usage; common.BindFlags = desc.BindFlags;
    const HRESULT normalized = D3D11CommonTexture::NormalizeTextureProperties(&common);
    if (normalized != S_OK) return FAILED(normalized) ? normalized : E_FAIL;
    auto imported = std::make_shared<NativeImage>(runtime, allocation, stride, preserveContents);
    Com<D3D11Texture2D> result = new D3D11Texture2D(renderer, &common, DXGI_USAGE(0), VK_NULL_HANDLE, imported);
    *texture = result.ref();
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (const DxvkError& error) { Logger::err(error.message()); return E_FAIL; }
    catch (...) { return E_FAIL; }
}
}
