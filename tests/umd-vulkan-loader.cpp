#include "../src/vulkan/vulkan_loader.h"
#include "../src/util/log/log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// The production loader executes unchanged; only log output is discarded.
namespace dxvk {
  void Logger::info(const std::string&) { }
  void Logger::err(const std::string&) { }
  void Logger::warn(const std::string&) { }
}

static unsigned checks, calls;
static VkInstance requestedInstance;
static const char* requestedName;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "Vulkan loader line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)

static void VKAPI_PTR marker() { }
static PFN_vkVoidFunction VKAPI_PTR loaderProc(VkInstance instance, const char* name) {
  ++calls;
  requestedInstance = instance;
  requestedName = name;
  return std::strcmp(name, "missing") ? marker : nullptr;
}

int main() {
  // A failed library load gives this same null resolver to LibraryFn's
  // constructor. Its member initializers must finish before admission rejects.
  dxvk::vk::LibraryFn missing(nullptr);
  CHECK(missing.getLoaderProc() == nullptr && missing.library() == nullptr);
  CHECK(missing.vkCreateInstance == nullptr);
  CHECK(missing.vkEnumerateInstanceLayerProperties == nullptr);
  CHECK(missing.vkEnumerateInstanceExtensionProperties == nullptr);
  CHECK(missing.sym("vkCreateInstance") == nullptr);
  CHECK(missing.sym(VK_NULL_HANDLE, "missing") == nullptr && calls == 0);

  dxvk::vk::LibraryFn available(loaderProc);
  CHECK(available.getLoaderProc() == loaderProc && available.library() == nullptr);
  CHECK(calls == 3 && requestedInstance == VK_NULL_HANDLE);
  CHECK(reinterpret_cast<PFN_vkVoidFunction>(available.vkCreateInstance) == marker);
  CHECK(reinterpret_cast<PFN_vkVoidFunction>(available.vkEnumerateInstanceLayerProperties) == marker);
  CHECK(reinterpret_cast<PFN_vkVoidFunction>(available.vkEnumerateInstanceExtensionProperties) == marker);
  CHECK(available.sym("vkCreateInstance") == marker);
  CHECK(calls == 4 && requestedInstance == VK_NULL_HANDLE && !std::strcmp(requestedName, "vkCreateInstance"));
  CHECK(available.sym("missing") == nullptr && calls == 5);
  const auto instance = reinterpret_cast<VkInstance>(uintptr_t(0x1234));
  CHECK(available.sym(instance, "vkCreateInstance") == marker);
  CHECK(calls == 6 && requestedInstance == instance && !std::strcmp(requestedName, "vkCreateInstance"));
  std::printf("Vulkan loader null resolver PASS checks=%u; no instance or device created\n", checks);
}
