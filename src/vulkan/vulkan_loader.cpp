#include <cwchar>
#include <string>
#include <tuple>
#include <vector>

#include "vulkan_loader.h"
#include "vulkan_loader_config.h"

#include "../util/log/log.h"

#include "../util/util_string.h"
#include "../util/util_win32_compat.h"

namespace dxvk::vk {

#ifdef _WIN32
  // A package build names one private Khronos loader (umd_vulkan_loader). It
  // is resolved next to the module this code is linked into, never through
  // the application's DLL search path: a D3D UMD runs from the DriverStore,
  // which is never on that path, and an emulated x64 or x86 process has no
  // guaranteed matching-architecture public vulkan-1.dll. There is no
  // fallback to winevulkan.dll or vulkan-1.dll.
  static HMODULE loadPrivateVulkanLibrary(std::wstring& path) {
    static const wchar_t name[] = L"" DXVK_PRIVATE_VULKAN_LOADER;
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
          | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        name, &self))
      return nullptr;
    std::vector<wchar_t> buffer(32768);
    DWORD length = GetModuleFileNameW(self, buffer.data(), DWORD(buffer.size()));
    if (!length || length >= buffer.size())
      return nullptr;
    path.assign(buffer.data(), length);
    const size_t slash = path.find_last_of(L'\\');
    if (slash == std::wstring::npos)
      return nullptr;
    path.resize(slash + 1);
    path += name;
    // A same-name loader from another package or application directory must
    // not be adopted silently.
    auto sameModule = [&buffer, &path] (HMODULE module) {
      DWORD size = GetModuleFileNameW(module, buffer.data(), DWORD(buffer.size()));
      return size && size < buffer.size() && !_wcsicmp(buffer.data(), path.c_str());
    };
    HMODULE existing = GetModuleHandleW(name);
    if (existing && !sameModule(existing))
      return nullptr;
    HMODULE library = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (library && !sameModule(library)) {
      FreeLibrary(library);
      return nullptr;
    }
    return library;
  }
#endif

  static std::pair<HMODULE, PFN_vkGetInstanceProcAddr> loadVulkanLibrary() {
#ifdef _WIN32
    if (sizeof(DXVK_PRIVATE_VULKAN_LOADER) > 1) {
      std::wstring path;
      HMODULE library = loadPrivateVulkanLibrary(path);
      auto proc = library ? GetProcAddress(library, "vkGetInstanceProcAddr") : nullptr;

      if (!proc) {
        if (library)
          FreeLibrary(library);
        Logger::err(str::format("Vulkan: private loader " DXVK_PRIVATE_VULKAN_LOADER " not usable at ", path.c_str()));
        return { };
      }

      Logger::info(str::format("Vulkan: Found vkGetInstanceProcAddr in private loader ", path.c_str()));
      return std::make_pair(library, reinterpret_cast<PFN_vkGetInstanceProcAddr>(proc));
    }
#endif

    static const std::array<const char*, 2> dllNames = {{
#ifdef _WIN32
      "winevulkan.dll",
      "vulkan-1.dll",
#else
      "libvulkan.so",
      "libvulkan.so.1",
#endif
    }};

    for (auto dllName : dllNames) {
      HMODULE library = LoadLibraryA(dllName);

      if (!library)
        continue;

      auto proc = GetProcAddress(library, "vkGetInstanceProcAddr");

      if (!proc) {
        FreeLibrary(library);
        continue;
      }

      Logger::info(str::format("Vulkan: Found vkGetInstanceProcAddr in ", dllName, " @ 0x", std::hex, reinterpret_cast<uintptr_t>(proc)));
      return std::make_pair(library, reinterpret_cast<PFN_vkGetInstanceProcAddr>(proc));
    }

    Logger::err("Vulkan: vkGetInstanceProcAddr not found");
    return { };
  }

  LibraryLoader::LibraryLoader() {
    std::tie(m_library, m_getInstanceProcAddr) = loadVulkanLibrary();
  }

  LibraryLoader::LibraryLoader(PFN_vkGetInstanceProcAddr loaderProc) {
    m_getInstanceProcAddr = loaderProc;
  }

  LibraryLoader::~LibraryLoader() {
    if (m_library)
      FreeLibrary(m_library);
  }

  PFN_vkVoidFunction LibraryLoader::sym(VkInstance instance, const char* name) const {
    return m_getInstanceProcAddr(instance, name);
  }

  PFN_vkVoidFunction LibraryLoader::sym(const char* name) const {
    return sym(nullptr, name);
  }

  
  InstanceLoader::InstanceLoader(const Rc<LibraryLoader>& library, bool owned, VkInstance instance)
  : m_library(library), m_instance(instance), m_owned(owned) { }
  
  
  PFN_vkVoidFunction InstanceLoader::sym(const char* name) const {
    return m_library->sym(m_instance, name);
  }
  
  
  DeviceLoader::DeviceLoader(const Rc<InstanceLoader>& library, bool owned, VkDevice device)
  : m_library(library)
  , m_getDeviceProcAddr(reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      m_library->sym("vkGetDeviceProcAddr"))),
    m_device(device), m_owned(owned) { }
  
  
  PFN_vkVoidFunction DeviceLoader::sym(const char* name) const {
    return m_getDeviceProcAddr(m_device, name);
  }
  
  
  LibraryFn::LibraryFn() { }
  LibraryFn::LibraryFn(PFN_vkGetInstanceProcAddr loaderProc)
  : LibraryLoader(loaderProc) { }
  LibraryFn::~LibraryFn() { }
  
  
  InstanceFn::InstanceFn(const Rc<LibraryLoader>& library, bool owned, VkInstance instance)
  : InstanceLoader(library, owned, instance) { }
  InstanceFn::~InstanceFn() {
    if (m_owned)
      this->vkDestroyInstance(m_instance, nullptr);
  }
  
  
  DeviceFn::DeviceFn(const Rc<InstanceLoader>& library, bool owned, VkDevice device)
  : DeviceLoader(library, owned, device) { }
  DeviceFn::~DeviceFn() {
    if (m_owned)
      this->vkDestroyDevice(m_device, nullptr);
  }
  
}
