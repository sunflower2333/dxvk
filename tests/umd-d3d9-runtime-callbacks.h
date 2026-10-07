#pragma once

// Probe-only forwarding diagnostics. No production UMD includes this header.
#include <windows.h>
#include <d3dumddi.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace dxvk::test {

class RuntimeCallbacks9 {
public:
  using Log = void (*)(const char*, ...);
  static constexpr size_t callbackBytes =
    offsetof(D3DDDI_DEVICECALLBACKS, pfnSetDisplayPrivateDriverFormatCb)
      + sizeof(PFND3DDDI_SETDISPLAYPRIVATEDRIVERFORMATCB);
  static_assert(callbackBytes == 22 * sizeof(void*));

  struct Owner {
    HANDLE device = nullptr, adapter = nullptr;
    const D3DDDI_DEVICECALLBACKS* original = nullptr;
    D3DDDI_DEVICECALLBACKS wrapped = {};
    Log log = nullptr;
    std::atomic<uint64_t> sequence{0};
  };
  using Pin = std::shared_ptr<Owner>;

  static Pin install(HANDLE device, HANDLE adapter,
                     const D3DDDI_DEVICECALLBACKS* original, Log log) {
    if (!device || !adapter || !original || !log) return {};
    auto owner = std::make_shared<Owner>();
    owner->device = device; owner->adapter = adapter;
    owner->original = original; owner->log = log;
    // Read only the actual Vista prefix, even with the current SDK definition.
    if (!read(&owner->wrapped, original, callbackBytes)) return {};
    if (owner->wrapped.pfnAllocateCb) owner->wrapped.pfnAllocateCb = allocate;
    if (owner->wrapped.pfnCreateContextCb) owner->wrapped.pfnCreateContextCb = createContext;
    if (owner->wrapped.pfnRenderCb) owner->wrapped.pfnRenderCb = render;
    if (owner->wrapped.pfnEscapeCb) owner->wrapped.pfnEscapeCb = escape;
    std::lock_guard<std::mutex> lock(mutex);
    return owners.emplace(device, owner).second ? owner : Pin{};
  }

  static void remove(const Pin& owner) {
    if (!owner) return;
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = owners.find(owner->device);
    if (entry != owners.end() && entry->second == owner) owners.erase(entry);
  }

private:
  static inline std::mutex mutex;
  static inline std::unordered_map<HANDLE, Pin> owners;

  // Logging must not add a fault for optional/unreadable diagnostic payloads.
  // This function has no objects requiring C++ unwinding inside its SEH scope.
  static bool read(void* output, const void* input, size_t bytes) noexcept {
    if (!input) return false;
    __try { std::memcpy(output, input, bytes); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
  }

  template<typename Type>
  static bool snapshot(Type& output, const Type* input, size_t bytes = sizeof(Type)) {
    if (read(&output, input, bytes)) return true;
    output = {};
    return false;
  }

  static Pin retain(HANDLE device) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = owners.find(device);
    return entry == owners.end() ? Pin{} : entry->second;
  }

  static Pin retainEscape(HANDLE adapter, HANDLE device) {
    std::lock_guard<std::mutex> lock(mutex);
    if (device) {
      const auto entry = owners.find(device);
      return entry != owners.end() && entry->second->adapter == adapter
        ? entry->second : Pin{};
    }
    // Escape permits hDevice==NULL. A single active device supplies its live
    // adapter callback. Never choose arbitrarily between different owners.
    Pin result;
    for (const auto& entry : owners) {
      if (entry.second->adapter != adapter) continue;
      if (result) return {};
      result = entry.second;
    }
    return result;
  }

  template<typename Function>
  static Function current(const Pin& owner,
                          Function D3DDDI_DEVICECALLBACKS::* member) {
    Function function = nullptr;
    if (!read(&function, &(owner->original->*member), sizeof(function))) function = nullptr;
    return function;
  }

  template<typename Function>
  static size_t address(Function function) {
    static_assert(sizeof(Function) == sizeof(size_t));
    size_t value = 0;
    std::memcpy(&value, &function, sizeof(value));
    return value;
  }

  struct PrivateWords { uint32_t values[16] = {}; bool readable = false; };
  static PrivateWords privateWords(const void* data, UINT bytes) {
    PrivateWords words;
    if (bytes >= 16) {
      const size_t count = bytes < sizeof(words.values) ? bytes : sizeof(words.values);
      words.readable = read(words.values, data, count);
    }
    if (!words.readable) std::memset(words.values, 0, sizeof(words.values));
    return words;
  }

  static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* args) {
    const auto owner = retain(device);
    if (!owner) return E_INVALIDARG;
    const auto function = current(owner, &D3DDDI_DEVICECALLBACKS::pfnAllocateCb);
    const auto call = static_cast<unsigned long long>(++owner->sequence);
    D3DDDICB_ALLOCATE input = {};
    const bool readable = snapshot(input, args);
    owner->log("SYSTEM_D3D9_CALLBACK_BEGIN kind=Allocate call=%llu thread=%lu runtime=%p args=%p original_table=%p callback_address=%zx forwarded=%u readable=%u resource=%p allocations=%u private_data=%p private_bytes=%u allocation_info=%p\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args, owner->original,
      address(function), unsigned(function != nullptr), unsigned(readable), input.hResource, input.NumAllocations, input.pPrivateDriverData,
      input.PrivateDriverDataSize, input.pAllocationInfo);
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    D3DDDICB_ALLOCATE output = {};
    const bool outputReadable = snapshot(output, args);
    D3DDDI_ALLOCATIONINFO first = {};
    const bool allocationReadable = outputReadable && output.NumAllocations
      && snapshot(first, output.pAllocationInfo);
    owner->log("SYSTEM_D3D9_CALLBACK_END kind=Allocate call=%llu thread=%lu runtime=%p args=%p hr=%08lx forwarded=%u readable=%u kernel_resource=%u allocations=%u allocation_info=%p first_readable=%u first_allocation=%u\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args,
      static_cast<unsigned long>(hr), unsigned(function != nullptr), unsigned(outputReadable), output.hKMResource,
      output.NumAllocations, output.pAllocationInfo, unsigned(allocationReadable), first.hAllocation);
    return hr;
  }

  static HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* args) {
    const auto owner = retain(device);
    if (!owner) return E_INVALIDARG;
    const auto function = current(owner, &D3DDDI_DEVICECALLBACKS::pfnCreateContextCb);
    const auto call = static_cast<unsigned long long>(++owner->sequence);
    constexpr size_t bytes = offsetof(D3DDDICB_CREATECONTEXT, PatchLocationListSize) + sizeof(UINT);
    D3DDDICB_CREATECONTEXT input = {};
    const bool readable = snapshot(input, args, bytes);
    const auto words = privateWords(input.pPrivateDriverData, input.PrivateDriverDataSize);
    owner->log("SYSTEM_D3D9_CALLBACK_BEGIN kind=CreateContext call=%llu thread=%lu runtime=%p args=%p original_table=%p callback_address=%zx forwarded=%u readable=%u node=%u engine=%u flags=%08x private_data=%p private_bytes=%u private_readable=%u private_magic=%08x private_version=%u private_declared=%u private16=%08x private20=%08x private24=%08x private28=%08x\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args, owner->original,
      address(function), unsigned(function != nullptr), unsigned(readable), input.NodeOrdinal, input.EngineAffinity, input.Flags.Value,
      input.pPrivateDriverData, input.PrivateDriverDataSize, unsigned(words.readable),
      words.values[0], words.values[1], words.values[2], words.values[4], words.values[5],
      words.values[6], words.values[7]);
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    D3DDDICB_CREATECONTEXT output = {};
    const bool outputReadable = snapshot(output, args, bytes);
    owner->log("SYSTEM_D3D9_CALLBACK_END kind=CreateContext call=%llu thread=%lu runtime=%p args=%p hr=%08lx forwarded=%u readable=%u context=%p command_buffer=%p command_bytes=%u allocation_list=%p allocation_count=%u patch_list=%p patch_count=%u\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args,
      static_cast<unsigned long>(hr), unsigned(function != nullptr), unsigned(outputReadable), output.hContext,
      output.pCommandBuffer, output.CommandBufferSize, output.pAllocationList,
      output.AllocationListSize, output.pPatchLocationList, output.PatchLocationListSize);
    return hr;
  }

  static HRESULT APIENTRY render(HANDLE device, D3DDDICB_RENDER* args) {
    const auto owner = retain(device);
    if (!owner) return E_INVALIDARG;
    const auto function = current(owner, &D3DDDI_DEVICECALLBACKS::pfnRenderCb);
    const auto call = static_cast<unsigned long long>(++owner->sequence);
    constexpr size_t bytes = offsetof(D3DDDICB_RENDER, QueuedBufferCount) + sizeof(ULONG);
    D3DDDICB_RENDER input = {};
    const bool readable = snapshot(input, args, bytes);
    owner->log("SYSTEM_D3D9_CALLBACK_BEGIN kind=Render call=%llu thread=%lu runtime=%p args=%p original_table=%p callback_address=%zx forwarded=%u readable=%u context=%p command_offset=%u command_bytes=%u allocations=%u patches=%u flags=%08x broadcasts=%u command_buffer=%p next_command_bytes=%u allocation_list=%p next_allocation_count=%u patch_list=%p next_patch_count=%u\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args, owner->original,
      address(function), unsigned(function != nullptr), unsigned(readable), input.hContext, input.CommandOffset, input.CommandLength,
      input.NumAllocations, input.NumPatchLocations, input.Flags.Value, input.BroadcastContextCount,
      input.pNewCommandBuffer, input.NewCommandBufferSize, input.pNewAllocationList,
      input.NewAllocationListSize, input.pNewPatchLocationList, input.NewPatchLocationListSize);
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    D3DDDICB_RENDER output = {};
    const bool outputReadable = snapshot(output, args, bytes);
    owner->log("SYSTEM_D3D9_CALLBACK_END kind=Render call=%llu thread=%lu runtime=%p args=%p hr=%08lx forwarded=%u readable=%u context=%p command_buffer=%p command_bytes=%u allocation_list=%p allocation_count=%u patch_list=%p patch_count=%u queued=%lu\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args,
      static_cast<unsigned long>(hr), unsigned(function != nullptr), unsigned(outputReadable), output.hContext,
      output.pNewCommandBuffer, output.NewCommandBufferSize, output.pNewAllocationList,
      output.NewAllocationListSize, output.pNewPatchLocationList, output.NewPatchLocationListSize,
      static_cast<unsigned long>(output.QueuedBufferCount));
    return hr;
  }

  static HRESULT APIENTRY escape(HANDLE adapter, const D3DDDICB_ESCAPE* args) {
    D3DDDICB_ESCAPE input = {};
    const bool readable = snapshot(input, args);
    const auto owner = retainEscape(adapter, input.hDevice);
    if (!owner) return E_INVALIDARG;
    const auto function = current(owner, &D3DDDI_DEVICECALLBACKS::pfnEscapeCb);
    const auto call = static_cast<unsigned long long>(++owner->sequence);
    const auto before = privateWords(input.pPrivateDriverData, input.PrivateDriverDataSize);
    owner->log("SYSTEM_D3D9_CALLBACK_BEGIN kind=Escape call=%llu thread=%lu adapter=%p runtime=%p args=%p original_table=%p callback_address=%zx forwarded=%u readable=%u context=%p flags=%08x private_data=%p private_bytes=%u private_readable=%u private_magic=%08x private_version=%u private_declared=%u private16=%08x private20=%08x\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), adapter, input.hDevice, args,
      owner->original, address(function), unsigned(function != nullptr), unsigned(readable), input.hContext, input.Flags.Value,
      input.pPrivateDriverData, input.PrivateDriverDataSize, unsigned(before.readable),
      before.values[0], before.values[1], before.values[2], before.values[4], before.values[5]);
    const HRESULT hr = function ? function(adapter, args) : E_FAIL;
    const auto after = privateWords(input.pPrivateDriverData, input.PrivateDriverDataSize);
    owner->log("SYSTEM_D3D9_CALLBACK_END kind=Escape call=%llu thread=%lu adapter=%p runtime=%p args=%p hr=%08lx forwarded=%u private_readable=%u private_magic=%08x private_version=%u private_declared=%u private16=%08x private20=%08x private24=%08x private28=%08x private32=%08x private36=%08x private40=%08x private44=%08x private48=%08x private52=%08x private56=%08x private60=%08x\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), adapter, input.hDevice, args,
      static_cast<unsigned long>(hr), unsigned(function != nullptr), unsigned(after.readable), after.values[0], after.values[1],
      after.values[2], after.values[4], after.values[5], after.values[6], after.values[7],
      after.values[8], after.values[9], after.values[10], after.values[11], after.values[12],
      after.values[13], after.values[14], after.values[15]);
    return hr;
  }
};

}
