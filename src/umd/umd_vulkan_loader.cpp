#include "../vulkan/vulkan_loader.h"

#include <windows.h>

// Package load check without an adapter or device: resolves the Vulkan loader
// exactly as native device creation does (vk::LibraryLoader), reports the
// module it loaded and releases it again. No Vulkan instance is created, so it
// runs on a CI runner with no GPU. With umd_vulkan_loader set, the loader must
// be that private DLL beside this UMD; otherwise it is the public search.
extern "C" HRESULT APIENTRY VioGpuDxvkQueryVulkanLoader(WCHAR* path, UINT32 capacity) {
  if (!path || !capacity) return E_INVALIDARG;
  path[0] = 0;
  try {
    dxvk::Rc<dxvk::vk::LibraryLoader> loader = new dxvk::vk::LibraryLoader();
    if (!loader->library() || !loader->getLoaderProc())
      return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    DWORD length = GetModuleFileNameW(loader->library(), path, capacity);
    if (!length || length >= capacity) {
      path[0] = 0;
      return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }
    return S_OK;
  } catch (...) {
    path[0] = 0;
    return E_FAIL;
  }
}
