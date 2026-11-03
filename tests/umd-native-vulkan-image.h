#pragma once
#define VK_NO_PROTOTYPES
#include "../include/vulkan/include/vulkan/vulkan.h"
#include "../src/umd/umd_runtime_gpu.h"
#include <vector>
#include <string>

// Uses the same private loader and exclusive ICD selection as the UMD. This
// diagnostic first queries layout; it never treats default pitch as AHB pitch.
class NativeVulkanImage final {
public:
  ~NativeVulkanImage() {
    if (m_device && m_gdpa) {
      if (m_readbackMapping) reinterpret_cast<PFN_vkUnmapMemory>(m_gdpa(m_device, "vkUnmapMemory"))(m_device, m_readbackMemory);
      if (m_fence) reinterpret_cast<PFN_vkDestroyFence>(m_gdpa(m_device, "vkDestroyFence"))(m_device, m_fence, nullptr);
      if (m_pool) reinterpret_cast<PFN_vkDestroyCommandPool>(m_gdpa(m_device, "vkDestroyCommandPool"))(m_device, m_pool, nullptr);
      if (m_readback) reinterpret_cast<PFN_vkDestroyBuffer>(m_gdpa(m_device, "vkDestroyBuffer"))(m_device, m_readback, nullptr);
      if (m_readbackMemory && m_freeMemory) m_freeMemory(m_device, m_readbackMemory, nullptr);
    }
    if (m_image && m_destroyImage) m_destroyImage(m_device, m_image, nullptr);
    if (m_memory && m_freeMemory) m_freeMemory(m_device, m_memory, nullptr);
    if (m_device && m_destroyDevice) m_destroyDevice(m_device, nullptr);
    if (m_instance && m_destroyInstance) m_destroyInstance(m_instance, nullptr);
    if (m_driver) FreeLibrary(m_driver);
    if (m_loader) FreeLibrary(m_loader);
  }
  HRESULT layout(dxvk::umd::RuntimeGpu& gpu, const LUID& luid,
      uint32_t width, uint32_t height, uint32_t stride, uint64_t size,
      const mwd_allocation& native, const char* candidate = nullptr) {
    const wchar_t* root = L"C:\\Users\\Public\\DxvkSystemIntegration-20261013-52\\arm64\\";
    const std::wstring loader = std::wstring(root) + L"viogpu_dxvk_loader_arm64.dll";
    const std::wstring driver = candidate ? std::wstring(candidate, candidate + std::strlen(candidate))
      : std::wstring(root) + L"viogpu_dxvk_vk_arm64.dll";
    m_loader = LoadLibraryExW(loader.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    m_driver = LoadLibraryExW(driver.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m_loader || !m_driver) return HRESULT_FROM_WIN32(GetLastError());
    auto gpa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(m_loader, "vkGetInstanceProcAddr"));
    auto driverGpa = reinterpret_cast<PFN_vkGetInstanceProcAddrLUNARG>(GetProcAddress(m_driver, "vk_icdGetInstanceProcAddr"));
    if (!gpa || !driverGpa) return E_FAIL;
    auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(gpa(nullptr, "vkCreateInstance"));
    if (!createInstance) return E_FAIL;
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "DXVK Native Surface Layout"; app.apiVersion = VK_API_VERSION_1_3;
    VkDirectDriverLoadingInfoLUNARG driverInfo = {VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_INFO_LUNARG};
    driverInfo.pfnGetInstanceProcAddr = driverGpa;
    VkDirectDriverLoadingListLUNARG drivers = {VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_LIST_LUNARG};
    drivers.mode = VK_DIRECT_DRIVER_LOADING_MODE_EXCLUSIVE_LUNARG;
    drivers.driverCount = 1; drivers.pDrivers = &driverInfo;
    const char* extensions[] = {VK_LUNARG_DIRECT_DRIVER_LOADING_EXTENSION_NAME};
    VkInstanceCreateInfo create = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pNext = &drivers; create.pApplicationInfo = &app;
    create.enabledExtensionCount = 1; create.ppEnabledExtensionNames = extensions;
    VkResult result = createInstance(&create, nullptr, &m_instance);
    std::printf("NATIVE_VK_INSTANCE result=%d\n", result);
    if (result != VK_SUCCESS) return E_FAIL;
    m_destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(gpa(m_instance, "vkDestroyInstance"));
    if (!m_destroyInstance) return E_FAIL;
#define INSTANCE(name) auto name = reinterpret_cast<PFN_##name>(gpa(m_instance, #name)); if (!name) return E_FAIL
    INSTANCE(vkEnumeratePhysicalDevices);
    INSTANCE(vkGetPhysicalDeviceProperties2);
    INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);
    INSTANCE(vkGetPhysicalDeviceImageFormatProperties);
    INSTANCE(vkGetPhysicalDeviceImageFormatProperties2);
    INSTANCE(vkGetPhysicalDeviceFormatProperties2);
    INSTANCE(vkGetPhysicalDeviceMemoryProperties);
    INSTANCE(vkCreateDevice);
    INSTANCE(vkGetDeviceProcAddr);
#undef INSTANCE
    uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(m_instance, &count, nullptr) != VK_SUCCESS || !count || count > 8) return E_FAIL;
    std::vector<VkPhysicalDevice> devices(count);
    if (vkEnumeratePhysicalDevices(m_instance, &count, devices.data()) != VK_SUCCESS || count > devices.size()) return E_FAIL;
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < count; ++i) {
      mwd_support support{MWD_STYPE_SUPPORT};
      VkPhysicalDeviceIDProperties id = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES}; id.pNext = &support;
      VkPhysicalDeviceProperties2 properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; properties.pNext = &id;
      vkGetPhysicalDeviceProperties2(devices[i], &properties);
      if (id.deviceLUIDValid && !std::memcmp(id.deviceLUID, &luid, sizeof(luid))
          && properties.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU
          && support.magic == MWD_RUNTIME_MAGIC && support.version == MWD_RUNTIME_ABI_VERSION
          && support.size == sizeof(mwd_callbacks) && (support.flags & 1)
          && (!candidate || (support.flags & MWD_SUPPORT_NATIVE_SURFACE))) {
        if (selected) return E_FAIL;
        selected = devices[i];
        std::printf("NATIVE_VK_SELECTED name=%s support=%u\n", properties.properties.deviceName, support.flags);
      }
    }
    if (!selected) return E_FAIL;
    count = 0; vkGetPhysicalDeviceQueueFamilyProperties(selected, &count, nullptr);
    if (!count || count > 32) return E_FAIL;
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(selected, &count, queues.data());
    uint32_t family = UINT32_MAX;
    for (uint32_t i = 0; i < count && i < queues.size(); ++i)
      if (queues[i].queueCount && (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { family = i; break; }
    if (family == UINT32_MAX) return E_FAIL;
    m_family = family;
    vkGetPhysicalDeviceMemoryProperties(selected, &m_memoryProperties);
    m_gdpa = vkGetDeviceProcAddr;
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
    auto runtime = gpu.backend();
    VkDeviceCreateInfo device = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device.pNext = &runtime.create; device.queueCreateInfoCount = 1; device.pQueueCreateInfos = &queue;
    const char* deviceExtensions[] = {VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME};
    if (candidate) { device.enabledExtensionCount = 1; device.ppEnabledExtensionNames = deviceExtensions; }
    result = vkCreateDevice(selected, &device, nullptr, &m_device);
    std::printf("NATIVE_VK_DEVICE result=%d\n", result);
    if (result != VK_SUCCESS) return E_FAIL;
    m_destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(vkGetDeviceProcAddr(m_device, "vkDestroyDevice"));
    m_destroyImage = reinterpret_cast<PFN_vkDestroyImage>(vkGetDeviceProcAddr(m_device, "vkDestroyImage"));
    if (!m_destroyDevice || !m_destroyImage) return E_FAIL;
#define DEVICE(name) auto name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(m_device, #name)); if (!name) return E_FAIL
    DEVICE(vkCreateImage);
    DEVICE(vkGetImageMemoryRequirements);
    DEVICE(vkGetImageSubresourceLayout);
    DEVICE(vkAllocateMemory);
    DEVICE(vkBindImageMemory);
#undef DEVICE
    m_freeMemory = reinterpret_cast<PFN_vkFreeMemory>(vkGetDeviceProcAddr(m_device, "vkFreeMemory"));
    if (!m_freeMemory) return E_FAIL;
    VkImageCreateInfo image = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image.imageType = VK_IMAGE_TYPE_2D; image.format = VK_FORMAT_B8G8R8A8_UNORM;
    image.extent = {width, height, 1}; image.mipLevels = 1; image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT; image.tiling = VK_IMAGE_TILING_LINEAR;
    image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE; image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageFormatProperties format = {};
    VkSubresourceLayout plane = {}; plane.rowPitch = stride;
    VkImageDrmFormatModifierExplicitCreateInfoEXT modifier = {VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT};
    modifier.drmFormatModifier = 0; modifier.drmFormatModifierPlaneCount = 1; modifier.pPlaneLayouts = &plane;
    if (candidate) {
      VkDrmFormatModifierPropertiesListEXT list = {VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT};
      VkFormatProperties2 properties = {VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2}; properties.pNext = &list;
      vkGetPhysicalDeviceFormatProperties2(selected, image.format, &properties);
      if (!list.drmFormatModifierCount || list.drmFormatModifierCount > 16) return E_FAIL;
      std::vector<VkDrmFormatModifierPropertiesEXT> modifiers(list.drmFormatModifierCount);
      list.pDrmFormatModifierProperties = modifiers.data();
      vkGetPhysicalDeviceFormatProperties2(selected, image.format, &properties);
      const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
        | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
      bool linear = false;
      for (uint32_t i = 0; i < list.drmFormatModifierCount && i < modifiers.size(); ++i)
        if (!modifiers[i].drmFormatModifier && modifiers[i].drmFormatModifierPlaneCount == 1
            && (modifiers[i].drmFormatModifierTilingFeatures & required) == required) linear = true;
      if (!linear) return E_FAIL;
      image.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT; image.pNext = &modifier;
      VkPhysicalDeviceImageDrmFormatModifierInfoEXT info = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT};
      info.drmFormatModifier = 0; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      VkPhysicalDeviceImageFormatInfo2 query = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
      query.pNext = &info; query.format = image.format; query.type = image.imageType;
      query.tiling = image.tiling; query.usage = image.usage; query.flags = image.flags;
      VkImageFormatProperties2 properties2 = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};
      result = vkGetPhysicalDeviceImageFormatProperties2(selected, &query, &properties2);
      format = properties2.imageFormatProperties;
    } else {
      result = vkGetPhysicalDeviceImageFormatProperties(selected, image.format, image.imageType,
        image.tiling, image.usage, image.flags, &format);
    }
    std::printf("NATIVE_VK_FORMAT result=%d\n", result);
    if (result != VK_SUCCESS) return E_FAIL;
    result = vkCreateImage(m_device, &image, nullptr, &m_image);
    std::printf("NATIVE_VK_IMAGE result=%d\n", result);
    if (result != VK_SUCCESS) return E_FAIL;
    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(m_device, m_image, &requirements);
    VkImageSubresource subresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    if (candidate) subresource.aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
    VkSubresourceLayout layout = {};
    vkGetImageSubresourceLayout(m_device, m_image, &subresource, &layout);
    const bool matches = layout.rowPitch == stride && !layout.offset
      && layout.size <= size && requirements.size <= size && requirements.alignment
      && !(native.address % requirements.alignment) && requirements.memoryTypeBits;
    std::printf("NATIVE_VK_LAYOUT width=%u height=%u row=%llu ahb_row=%u size=%llu ahb_size=%llu"
      " required=%llu alignment=%llu types=%u offset=%llu matches=%u\n", width, height,
      (unsigned long long)layout.rowPitch, stride, (unsigned long long)layout.size,
      (unsigned long long)size, (unsigned long long)requirements.size,
      (unsigned long long)requirements.alignment, requirements.memoryTypeBits,
      (unsigned long long)layout.offset, unsigned(matches));
    if (!matches) return E_FAIL;
    if (candidate) {
      VkPhysicalDeviceMemoryProperties memory = {};
      vkGetPhysicalDeviceMemoryProperties(selected, &memory);
      uint32_t type = UINT32_MAX;
      for (uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        std::printf("NATIVE_VK_MEMORY_TYPE index=%u flags=%u compatible=%u\n", i,
          memory.memoryTypes[i].propertyFlags, unsigned((requirements.memoryTypeBits & (1u << i)) != 0));
        if ((requirements.memoryTypeBits & (1u << i))
            && (memory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            && !(memory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) { type = i; break; }
      }
      if (type == UINT32_MAX) return E_FAIL;
      mwd_import_memory_info imported{MWD_STYPE_IMPORT, nullptr, runtime.create.owner, native.token};
      VkMemoryAllocateInfo allocate = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
      allocate.pNext = &imported; allocate.allocationSize = size; allocate.memoryTypeIndex = type;
      result = vkAllocateMemory(m_device, &allocate, nullptr, &m_memory);
      std::printf("NATIVE_VK_IMPORT result=%d type=%u bytes=%llu\n", result, type, (unsigned long long)size);
      if (result != VK_SUCCESS) return E_FAIL;
      result = vkBindImageMemory(m_device, m_image, m_memory, 0);
      std::printf("NATIVE_VK_BIND result=%d\n", result);
      if (result != VK_SUCCESS) return E_FAIL;
    }
    return S_OK;
  }
  HRESULT exercise(uint32_t width, uint32_t height) {
    if (!m_memory || !m_image || !m_gdpa || m_readback) return E_INVALIDARG;
#define DEVICE(name) auto name = reinterpret_cast<PFN_##name>(m_gdpa(m_device, #name)); if (!name) return E_FAIL
    DEVICE(vkGetDeviceQueue); DEVICE(vkCreateBuffer); DEVICE(vkGetBufferMemoryRequirements);
    DEVICE(vkAllocateMemory); DEVICE(vkBindBufferMemory); DEVICE(vkMapMemory);
    DEVICE(vkCreateCommandPool); DEVICE(vkAllocateCommandBuffers);
    DEVICE(vkBeginCommandBuffer); DEVICE(vkEndCommandBuffer); DEVICE(vkResetCommandBuffer);
    DEVICE(vkCmdPipelineBarrier); DEVICE(vkCmdClearColorImage); DEVICE(vkCmdCopyImageToBuffer);
    DEVICE(vkCreateFence); DEVICE(vkResetFences); DEVICE(vkQueueSubmit); DEVICE(vkWaitForFences);
    DEVICE(vkDestroyFence); DEVICE(vkDestroyCommandPool); DEVICE(vkDestroyBuffer); DEVICE(vkUnmapMemory);
    (void)vkDestroyFence; (void)vkDestroyCommandPool; (void)vkDestroyBuffer; (void)vkUnmapMemory;
#undef DEVICE
#define VK_CHECK(call) do { const VkResult r = (call); if (r != VK_SUCCESS) { \
    std::printf("NATIVE_VK_OPERATION_FAIL operation=%s result=%d\n", #call, r); return E_FAIL; } } while (0)
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(m_device, m_family, 0, &queue);
    if (!queue) return E_FAIL;
    const VkDeviceSize bytes = VkDeviceSize(width) * height * 4;
    VkBufferCreateInfo buffer = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size = bytes + 64; buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(m_device, &buffer, nullptr, &m_readback));
    VkMemoryRequirements requirement = {};
    vkGetBufferMemoryRequirements(m_device, m_readback, &requirement);
    uint32_t type = UINT32_MAX;
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
      if ((requirement.memoryTypeBits & (1u << i)) && (m_memoryProperties.memoryTypes[i].propertyFlags & host) == host) {
        type = i; break;
      }
    if (type == UINT32_MAX || requirement.size < buffer.size || requirement.size > UINT32_MAX) return E_FAIL;
    VkMemoryAllocateInfo allocate = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = requirement.size; allocate.memoryTypeIndex = type;
    VK_CHECK(vkAllocateMemory(m_device, &allocate, nullptr, &m_readbackMemory));
    VK_CHECK(vkBindBufferMemory(m_device, m_readback, m_readbackMemory, 0));
    VK_CHECK(vkMapMemory(m_device, m_readbackMemory, 0, VK_WHOLE_SIZE, 0, &m_readbackMapping));
    if (!m_readbackMapping) return E_FAIL;
    VkCommandPoolCreateInfo pool = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pool.queueFamilyIndex = m_family;
    VK_CHECK(vkCreateCommandPool(m_device, &pool, nullptr, &m_pool));
    VkCommandBufferAllocateInfo commands = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commands.commandPool = m_pool; commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; commands.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &commands, &command));
    VkFenceCreateInfo fence = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK_CHECK(vkCreateFence(m_device, &fence, nullptr, &m_fence));
    for (uint32_t frame = 0; frame < 2; ++frame) {
      if (frame) {
        VK_CHECK(vkResetFences(m_device, 1, &m_fence));
        VK_CHECK(vkResetCommandBuffer(command, 0));
      }
      std::memset(m_readbackMapping, 0x5a, size_t(requirement.size));
      VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
      begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VK_CHECK(vkBeginCommandBuffer(command, &begin));
      VkImageMemoryBarrier image = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
      image.srcAccessMask = frame ? VK_ACCESS_TRANSFER_READ_BIT : 0;
      image.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      image.oldLayout = frame ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
      image.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      image.srcQueueFamilyIndex = image.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      image.image = m_image; image.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      VkBufferMemoryBarrier hostWrite = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
      hostWrite.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT; hostWrite.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      hostWrite.srcQueueFamilyIndex = hostWrite.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      hostWrite.buffer = m_readback; hostWrite.size = VK_WHOLE_SIZE;
      vkCmdPipelineBarrier(command, (frame ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
        | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &hostWrite, 1, &image);
      VkClearColorValue color = {}; color.float32[frame] = 1.0f; color.float32[3] = 1.0f;
      vkCmdClearColorImage(command, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &image.subresourceRange);
      image.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; image.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      image.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; image.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &image);
      VkBufferImageCopy copy = {}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      copy.imageExtent = {width, height, 1};
      vkCmdCopyImageToBuffer(command, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_readback, 1, &copy);
      VkBufferMemoryBarrier read = hostWrite;
      read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; read.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
      vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        0, 0, nullptr, 1, &read, 0, nullptr);
      VK_CHECK(vkEndCommandBuffer(command));
      VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
      submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
      VK_CHECK(vkQueueSubmit(queue, 1, &submit, m_fence));
      const VkResult waited = vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, 5000000000ull);
      std::printf("NATIVE_VK_FENCE frame=%u result=%d\n", frame, waited);
      if (waited != VK_SUCCESS) {
        // Do not free in-flight Vulkan objects or forge completion. The owned
        // runner records failure; process teardown retires kernel ownership.
        std::printf("NATIVE_VK_QUIESCENCE_FAILED process_cleanup_required=1\n");
        std::fflush(stdout); TerminateProcess(GetCurrentProcess(), 3); return E_FAIL;
      }
      const auto* pixels = static_cast<const uint32_t*>(m_readbackMapping);
      const uint32_t expected = frame ? 0xff00ff00u : 0xffff0000u;
      uint32_t mismatches = 0;
      for (uint64_t i = 0; i < bytes / 4; ++i) {
        if (pixels[i] != expected) {
          if (mismatches < 4) std::printf("NATIVE_VK_PIXEL_MISMATCH frame=%u index=%llu actual=%08x expected=%08x\n",
            frame, (unsigned long long)i, pixels[i], expected);
          ++mismatches;
        }
      }
      const auto* raw = static_cast<const uint8_t*>(m_readbackMapping);
      uint32_t guard = 0;
      for (uint64_t i = bytes; i < requirement.size; ++i) guard += raw[i] != 0x5a;
      std::printf("NATIVE_VK_PIXELS frame=%u checked=%llu mismatches=%u guard_mismatches=%u native_cpu_map=0\n",
        frame, (unsigned long long)(bytes / 4), mismatches, guard);
      if (mismatches || guard) return E_FAIL;
    }
#undef VK_CHECK
    return S_OK;
  }
private:
  HMODULE m_loader = nullptr, m_driver = nullptr;
  VkInstance m_instance = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkImage m_image = VK_NULL_HANDLE;
  VkDeviceMemory m_memory = VK_NULL_HANDLE;
  PFN_vkDestroyInstance m_destroyInstance = nullptr;
  PFN_vkDestroyDevice m_destroyDevice = nullptr;
  PFN_vkDestroyImage m_destroyImage = nullptr;
  PFN_vkFreeMemory m_freeMemory = nullptr;
  PFN_vkGetDeviceProcAddr m_gdpa = nullptr;
  VkPhysicalDeviceMemoryProperties m_memoryProperties = {};
  uint32_t m_family = UINT32_MAX;
  VkBuffer m_readback = VK_NULL_HANDLE;
  VkDeviceMemory m_readbackMemory = VK_NULL_HANDLE;
  void* m_readbackMapping = nullptr;
  VkCommandPool m_pool = VK_NULL_HANDLE;
  VkFence m_fence = VK_NULL_HANDLE;
};
