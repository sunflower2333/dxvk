#pragma once

// Probe-only forwarding diagnostics. No production UMD includes this header.
#include <windows.h>
#include <d3d9.h> // Official types required by the legacy DDI; no API calls.
#include <d3dumddi.h>
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace dxvk::test {

// The four detailed Allocate/CreateContext/Render/Escape forwarders below are
// derived from the already native-tested API9 diagnostics, with API8 log tags.
// No argument or HRESULT is substituted. Additional bookkeeping observes the
// actual returned handles, including locks surviving submission, and rejects
// incomplete lifetime evidence offline rather than changing runtime behavior.
class RuntimeCallbacks8 {
public:
  using Log = void (*)(const char*, ...);
  static constexpr size_t callbackBytes =
    offsetof(D3DDDI_DEVICECALLBACKS, pfnSetDisplayPrivateDriverFormatCb)
      + sizeof(PFND3DDDI_SETDISPLAYPRIVATEDRIVERFORMATCB);
  static_assert(callbackBytes == 22 * sizeof(void*));

  struct Owner {
    HANDLE device = nullptr, adapter = nullptr;
    const D3DDDI_DEVICECALLBACKS* originalTable = nullptr; // Address provenance only.
    const D3DDDI_DEVICECALLBACKS functions;
    D3DDDI_DEVICECALLBACKS wrapped = {};
    Log log = nullptr;
    std::atomic<uint64_t> sequence{0};
    std::atomic<unsigned> failures{0};
    std::mutex trackingMutex;
    struct Allocation { D3DKMT_HANDLE handle = 0; HANDLE resource = nullptr; bool locked = false; };
    std::array<Allocation, 512> allocations{};
    std::array<HANDLE, 16> contexts{};
    unsigned allocateOk = 0, deallocateOk = 0, lockOk = 0, unlockOk = 0;
    unsigned contextOk = 0, destroyContextOk = 0, renderOk = 0, presentOk = 0, residencyOk = 0;
    unsigned trackingErrors = 0;
    explicit Owner(const D3DDDI_DEVICECALLBACKS& inputFunctions)
      : functions(inputFunctions), wrapped(inputFunctions) { }
  };
  using Pin = std::shared_ptr<Owner>;

  static Pin install(HANDLE device, HANDLE adapter,
                     const D3DDDI_DEVICECALLBACKS* original, Log log) {
    if (!device || !adapter || !original || !log) return {};
    // Snapshot the actual Vista prefix once during CreateDevice, matching the
    // production D3D9 driver. Borrowed runtime table storage may be ephemeral.
    D3DDDI_DEVICECALLBACKS functions = {};
    if (!read(&functions, original, callbackBytes)) return {};
    auto owner = std::make_shared<Owner>(functions);
    owner->device = device; owner->adapter = adapter;
    owner->originalTable = original; owner->log = log;
    if (owner->wrapped.pfnAllocateCb) owner->wrapped.pfnAllocateCb = allocate;
    if (owner->wrapped.pfnCreateContextCb) owner->wrapped.pfnCreateContextCb = createContext;
    if (owner->wrapped.pfnRenderCb) owner->wrapped.pfnRenderCb = render;
    if (owner->wrapped.pfnEscapeCb) owner->wrapped.pfnEscapeCb = escape;
    if (owner->wrapped.pfnDeallocateCb) owner->wrapped.pfnDeallocateCb = deallocate;
    if (owner->wrapped.pfnDestroyContextCb) owner->wrapped.pfnDestroyContextCb = destroyContext;
    if (owner->wrapped.pfnLockCb) owner->wrapped.pfnLockCb = lockAllocation;
    if (owner->wrapped.pfnUnlockCb) owner->wrapped.pfnUnlockCb = unlockAllocation;
    if (owner->wrapped.pfnPresentCb) owner->wrapped.pfnPresentCb = present;
    if (owner->wrapped.pfnQueryResidencyCb) owner->wrapped.pfnQueryResidencyCb = residency;
    std::lock_guard<std::mutex> lock(mutex);
    return owners.emplace(device, owner).second ? owner : Pin{};
  }

  static void remove(const Pin& owner) {
    if (!owner) return;
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = owners.find(owner->device);
    if (entry != owners.end() && entry->second == owner) owners.erase(entry);
  }

  static void summary(const Pin& owner, const char* phase) {
    if (!owner) return;
    std::lock_guard<std::mutex> lock(owner->trackingMutex);
    unsigned live = 0, locked = 0, contexts = 0;
    for (const auto& item : owner->allocations) {
      live += unsigned(item.handle != 0); locked += unsigned(item.handle != 0 && item.locked);
    }
    for (const auto item : owner->contexts) contexts += unsigned(item != nullptr);
    owner->log("SYSTEM_D3D8_LIFETIME phase=%s runtime=%p allocate=%u deallocate=%u lock=%u unlock=%u create_context=%u destroy_context=%u render=%u present=%u residency=%u live_allocations=%u live_locks=%u live_contexts=%u tracking_errors=%u callback_failures=%u\n",
      phase, owner->device, owner->allocateOk, owner->deallocateOk, owner->lockOk, owner->unlockOk,
      owner->contextOk, owner->destroyContextOk, owner->renderOk, owner->presentOk, owner->residencyOk,
      live, locked, contexts, owner->trackingErrors, owner->failures.load());
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
    // Escape permits hDevice==NULL. A single active device supplies its owned
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
  static Function captured(const Pin& owner,
                          Function D3DDDI_DEVICECALLBACKS::* member) {
    return owner->functions.*member;
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
    const auto function = captured(owner, &D3DDDI_DEVICECALLBACKS::pfnAllocateCb);
    const auto call = static_cast<unsigned long long>(++owner->sequence);
    D3DDDICB_ALLOCATE input = {};
    const bool readable = snapshot(input, args);
    owner->log("SYSTEM_D3D8_CALLBACK_BEGIN kind=Allocate call=%llu thread=%lu runtime=%p args=%p original_table=%p callback_address=%zx forwarded=%u readable=%u resource=%p allocations=%u private_data=%p private_bytes=%u allocation_info=%p\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args, owner->originalTable,
      address(function), unsigned(function != nullptr), unsigned(readable), input.hResource, input.NumAllocations, input.pPrivateDriverData,
      input.PrivateDriverDataSize, input.pAllocationInfo);
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    if (hr != S_OK) ++owner->failures;
    D3DDDICB_ALLOCATE output = {};
    const bool outputReadable = snapshot(output, args);
    D3DDDI_ALLOCATIONINFO first = {};
    const bool allocationReadable = outputReadable && output.NumAllocations
      && snapshot(first, output.pAllocationInfo);
    owner->log("SYSTEM_D3D8_CALLBACK_END kind=Allocate call=%llu thread=%lu runtime=%p args=%p hr=%08lx forwarded=%u readable=%u kernel_resource=%u allocations=%u allocation_info=%p first_readable=%u first_allocation=%u\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args,
      static_cast<unsigned long>(hr), unsigned(function != nullptr), unsigned(outputReadable), output.hKMResource,
      output.NumAllocations, output.pAllocationInfo, unsigned(allocationReadable), first.hAllocation);
    if (hr == S_OK) {
      std::lock_guard<std::mutex> lock(owner->trackingMutex);
      if (!outputReadable || !output.NumAllocations || !output.pAllocationInfo
          || output.NumAllocations > owner->allocations.size())
        ++owner->trackingErrors;
      else for (UINT i = 0; i < output.NumAllocations; ++i) {
        D3DDDI_ALLOCATIONINFO info{};
        if (!snapshot(info, output.pAllocationInfo + i) || !info.hAllocation) { ++owner->trackingErrors; continue; }
        auto slot = owner->allocations.end(); bool duplicate = false;
        for (auto entry = owner->allocations.begin(); entry != owner->allocations.end(); ++entry) {
          duplicate = duplicate || entry->handle == info.hAllocation;
          if (!entry->handle && slot == owner->allocations.end()) slot = entry;
        }
        if (duplicate || slot == owner->allocations.end()) ++owner->trackingErrors;
        else { *slot = {info.hAllocation, output.hResource, false}; ++owner->allocateOk; }
        owner->log("SYSTEM_D3D8_ALLOCATION runtime=%p index=%u handle=%u resource=%p tracked=%u\n",
          device, i, info.hAllocation, output.hResource, unsigned(!duplicate && slot != owner->allocations.end()));
      }
    }
    return hr;
  }

  static HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* args) {
    const auto owner = retain(device);
    if (!owner) return E_INVALIDARG;
    const auto function = captured(owner, &D3DDDI_DEVICECALLBACKS::pfnCreateContextCb);
    const auto call = static_cast<unsigned long long>(++owner->sequence);
    constexpr size_t bytes = offsetof(D3DDDICB_CREATECONTEXT, PatchLocationListSize) + sizeof(UINT);
    D3DDDICB_CREATECONTEXT input = {};
    const bool readable = snapshot(input, args, bytes);
    const auto words = privateWords(input.pPrivateDriverData, input.PrivateDriverDataSize);
    owner->log("SYSTEM_D3D8_CALLBACK_BEGIN kind=CreateContext call=%llu thread=%lu runtime=%p args=%p original_table=%p callback_address=%zx forwarded=%u readable=%u node=%u engine=%u flags=%08x private_data=%p private_bytes=%u private_readable=%u private_magic=%08x private_version=%u private_declared=%u private16=%08x private20=%08x private24=%08x private28=%08x\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args, owner->originalTable,
      address(function), unsigned(function != nullptr), unsigned(readable), input.NodeOrdinal, input.EngineAffinity, input.Flags.Value,
      input.pPrivateDriverData, input.PrivateDriverDataSize, unsigned(words.readable),
      words.values[0], words.values[1], words.values[2], words.values[4], words.values[5],
      words.values[6], words.values[7]);
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    if (hr != S_OK) ++owner->failures;
    D3DDDICB_CREATECONTEXT output = {};
    const bool outputReadable = snapshot(output, args, bytes);
    owner->log("SYSTEM_D3D8_CALLBACK_END kind=CreateContext call=%llu thread=%lu runtime=%p args=%p hr=%08lx forwarded=%u readable=%u context=%p command_buffer=%p command_bytes=%u allocation_list=%p allocation_count=%u patch_list=%p patch_count=%u\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args,
      static_cast<unsigned long>(hr), unsigned(function != nullptr), unsigned(outputReadable), output.hContext,
      output.pCommandBuffer, output.CommandBufferSize, output.pAllocationList,
      output.AllocationListSize, output.pPatchLocationList, output.PatchLocationListSize);
    if (hr == S_OK) {
      std::lock_guard<std::mutex> lock(owner->trackingMutex);
      auto slot = owner->contexts.end(); bool duplicate = false;
      for (auto entry = owner->contexts.begin(); entry != owner->contexts.end(); ++entry) {
        duplicate = duplicate || *entry == output.hContext;
        if (!*entry && slot == owner->contexts.end()) slot = entry;
      }
      if (!outputReadable || !output.hContext || duplicate || slot == owner->contexts.end()) ++owner->trackingErrors;
      else { *slot = output.hContext; ++owner->contextOk; }
    }
    return hr;
  }

  static HRESULT APIENTRY render(HANDLE device, D3DDDICB_RENDER* args) {
    const auto owner = retain(device);
    if (!owner) return E_INVALIDARG;
    const auto function = captured(owner, &D3DDDI_DEVICECALLBACKS::pfnRenderCb);
    const auto call = static_cast<unsigned long long>(++owner->sequence);
    constexpr size_t bytes = offsetof(D3DDDICB_RENDER, QueuedBufferCount) + sizeof(ULONG);
    D3DDDICB_RENDER input = {};
    const bool readable = snapshot(input, args, bytes);
    owner->log("SYSTEM_D3D8_CALLBACK_BEGIN kind=Render call=%llu thread=%lu runtime=%p args=%p original_table=%p callback_address=%zx forwarded=%u readable=%u context=%p command_offset=%u command_bytes=%u allocations=%u patches=%u flags=%08x broadcasts=%u command_buffer=%p next_command_bytes=%u allocation_list=%p next_allocation_count=%u patch_list=%p next_patch_count=%u\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args, owner->originalTable,
      address(function), unsigned(function != nullptr), unsigned(readable), input.hContext, input.CommandOffset, input.CommandLength,
      input.NumAllocations, input.NumPatchLocations, input.Flags.Value, input.BroadcastContextCount,
      input.pNewCommandBuffer, input.NewCommandBufferSize, input.pNewAllocationList,
      input.NewAllocationListSize, input.pNewPatchLocationList, input.NewPatchLocationListSize);
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    if (hr != S_OK) ++owner->failures;
    else { std::lock_guard<std::mutex> lock(owner->trackingMutex); ++owner->renderOk; }
    D3DDDICB_RENDER output = {};
    const bool outputReadable = snapshot(output, args, bytes);
    owner->log("SYSTEM_D3D8_CALLBACK_END kind=Render call=%llu thread=%lu runtime=%p args=%p hr=%08lx forwarded=%u readable=%u context=%p command_buffer=%p command_bytes=%u allocation_list=%p allocation_count=%u patch_list=%p patch_count=%u queued=%lu\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), device, args,
      static_cast<unsigned long>(hr), unsigned(function != nullptr), unsigned(outputReadable), output.hContext,
      output.pNewCommandBuffer, output.NewCommandBufferSize, output.pNewAllocationList,
      output.NewAllocationListSize, output.pNewPatchLocationList, output.NewPatchLocationListSize,
      static_cast<unsigned long>(output.QueuedBufferCount));
    return hr;
  }

  static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
    const auto owner = retain(device); if (!owner) return E_INVALIDARG;
    D3DDDICB_DEALLOCATE input{}; const bool readable = snapshot(input, args);
    std::array<D3DKMT_HANDLE, 512> handles{};
    const bool list = readable && !input.hResource && input.NumAllocations <= handles.size()
      && read(handles.data(), input.HandleList, size_t(input.NumAllocations) * sizeof(handles[0]));
    const auto function = owner->functions.pfnDeallocateCb;
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    if (hr != S_OK) ++owner->failures;
    else {
      std::lock_guard<std::mutex> lock(owner->trackingMutex);
      if (!readable || (!input.hResource && !list)) ++owner->trackingErrors;
      else if (input.hResource) {
        bool found = false;
        for (auto& entry : owner->allocations) if (entry.handle && entry.resource == input.hResource) {
          found = true; if (entry.locked) ++owner->trackingErrors;
          entry = {}; ++owner->deallocateOk;
        }
        if (!found) ++owner->trackingErrors;
      } else for (UINT i = 0; i < input.NumAllocations; ++i) {
        bool found = false;
        for (auto& entry : owner->allocations) if (entry.handle && entry.handle == handles[i]) {
          found = true; if (entry.locked) ++owner->trackingErrors;
          entry = {}; ++owner->deallocateOk;
        }
        if (!found) ++owner->trackingErrors;
      }
    }
    owner->log("SYSTEM_D3D8_CALLBACK_RESULT kind=Deallocate runtime=%p hr=%08lx resource=%p count=%u readable=%u\n",
      device, static_cast<unsigned long>(hr), input.hResource, input.NumAllocations, unsigned(readable));
    return hr;
  }

  static HRESULT APIENTRY destroyContext(HANDLE device, const D3DDDICB_DESTROYCONTEXT* args) {
    const auto owner = retain(device); if (!owner) return E_INVALIDARG;
    D3DDDICB_DESTROYCONTEXT input{}; const bool readable = snapshot(input, args);
    const auto function = owner->functions.pfnDestroyContextCb;
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    if (hr != S_OK) ++owner->failures;
    else {
      std::lock_guard<std::mutex> lock(owner->trackingMutex); bool found = false;
      for (auto& entry : owner->contexts) if (entry && entry == input.hContext) {
        entry = nullptr; ++owner->destroyContextOk; found = true;
      }
      if (!readable || !found) ++owner->trackingErrors;
    }
    owner->log("SYSTEM_D3D8_CALLBACK_RESULT kind=DestroyContext runtime=%p hr=%08lx context=%p readable=%u\n",
      device, static_cast<unsigned long>(hr), input.hContext, unsigned(readable));
    return hr;
  }

  static HRESULT APIENTRY lockAllocation(HANDLE device, D3DDDICB_LOCK* args) {
    const auto owner = retain(device); if (!owner) return E_INVALIDARG;
    constexpr size_t bytes = offsetof(D3DDDICB_LOCK, Flags) + sizeof(D3DDDICB_LOCKFLAGS);
    D3DDDICB_LOCK input{}, output{}; const bool readable = snapshot(input, args, bytes);
    const auto function = owner->functions.pfnLockCb;
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    const bool outputReadable = snapshot(output, args, bytes);
    if (hr != S_OK) ++owner->failures;
    else {
      std::lock_guard<std::mutex> lock(owner->trackingMutex); bool found = false;
      for (auto& entry : owner->allocations) if (entry.handle && entry.handle == input.hAllocation) {
        found = true;
        // A runtime rename cannot be silently relabeled as the original owned BO.
        if (!readable || !outputReadable || entry.locked || output.hAllocation != input.hAllocation || !output.pData)
          ++owner->trackingErrors;
        else { entry.locked = true; ++owner->lockOk; }
      }
      if (!found) ++owner->trackingErrors;
    }
    owner->log("SYSTEM_D3D8_CALLBACK_RESULT kind=Lock runtime=%p hr=%08lx input=%u output=%u flags=%08x mapping=%p readable=%u\n",
      device, static_cast<unsigned long>(hr), input.hAllocation, output.hAllocation, input.Flags.Value,
      output.pData, unsigned(readable && outputReadable));
    return hr;
  }

  static HRESULT APIENTRY unlockAllocation(HANDLE device, const D3DDDICB_UNLOCK* args) {
    const auto owner = retain(device); if (!owner) return E_INVALIDARG;
    D3DDDICB_UNLOCK input{}; const bool readable = snapshot(input, args);
    std::array<D3DKMT_HANDLE, 512> handles{};
    const bool list = readable && input.NumAllocations <= handles.size()
      && read(handles.data(), input.phAllocations, size_t(input.NumAllocations) * sizeof(handles[0]));
    const auto function = owner->functions.pfnUnlockCb;
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    if (hr != S_OK) ++owner->failures;
    else {
      std::lock_guard<std::mutex> lock(owner->trackingMutex);
      if (!list) ++owner->trackingErrors;
      else for (UINT i = 0; i < input.NumAllocations; ++i) {
        bool found = false;
        for (auto& entry : owner->allocations) if (entry.handle && entry.handle == handles[i]) {
          found = true;
          if (!entry.locked) ++owner->trackingErrors;
          else { entry.locked = false; ++owner->unlockOk; }
        }
        if (!found) ++owner->trackingErrors;
      }
    }
    owner->log("SYSTEM_D3D8_CALLBACK_RESULT kind=Unlock runtime=%p hr=%08lx count=%u readable=%u\n",
      device, static_cast<unsigned long>(hr), input.NumAllocations, unsigned(list));
    return hr;
  }

  static HRESULT APIENTRY present(HANDLE device, D3DDDICB_PRESENT* args) {
    const auto owner = retain(device); if (!owner) return E_INVALIDARG;
    constexpr size_t bytes = offsetof(D3DDDICB_PRESENT, BroadcastContext) + sizeof(D3DDDICB_PRESENT::BroadcastContext);
    D3DDDICB_PRESENT input{}; const bool readable = snapshot(input, args, bytes);
    const auto function = owner->functions.pfnPresentCb;
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    if (hr != S_OK) ++owner->failures;
    else { std::lock_guard<std::mutex> lock(owner->trackingMutex); ++owner->presentOk; }
    owner->log("SYSTEM_D3D8_CALLBACK_RESULT kind=Present runtime=%p hr=%08lx source=%u destination=%u context=%p readable=%u\n",
      device, static_cast<unsigned long>(hr), input.hSrcAllocation, input.hDstAllocation, input.hContext, unsigned(readable));
    return hr;
  }

  static HRESULT APIENTRY residency(HANDLE device, const D3DDDICB_QUERYRESIDENCY* args) {
    const auto owner = retain(device); if (!owner) return E_INVALIDARG;
    D3DDDICB_QUERYRESIDENCY input{}; const bool readable = snapshot(input, args);
    const auto function = owner->functions.pfnQueryResidencyCb;
    const HRESULT hr = function ? function(device, args) : E_FAIL;
    if (hr != S_OK) ++owner->failures;
    else { std::lock_guard<std::mutex> lock(owner->trackingMutex); ++owner->residencyOk; }
    owner->log("SYSTEM_D3D8_CALLBACK_RESULT kind=Residency runtime=%p hr=%08lx resource=%p count=%u readable=%u\n",
      device, static_cast<unsigned long>(hr), input.hResource, input.NumAllocations, unsigned(readable));
    return hr;
  }

  static HRESULT APIENTRY escape(HANDLE adapter, const D3DDDICB_ESCAPE* args) {
    D3DDDICB_ESCAPE input = {};
    const bool readable = snapshot(input, args);
    const auto owner = retainEscape(adapter, input.hDevice);
    if (!owner) return E_INVALIDARG;
    const auto function = captured(owner, &D3DDDI_DEVICECALLBACKS::pfnEscapeCb);
    const auto call = static_cast<unsigned long long>(++owner->sequence);
    const auto before = privateWords(input.pPrivateDriverData, input.PrivateDriverDataSize);
    owner->log("SYSTEM_D3D8_CALLBACK_BEGIN kind=Escape call=%llu thread=%lu adapter=%p runtime=%p args=%p original_table=%p callback_address=%zx forwarded=%u readable=%u context=%p flags=%08x private_data=%p private_bytes=%u private_readable=%u private_magic=%08x private_version=%u private_declared=%u private16=%08x private20=%08x\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), adapter, input.hDevice, args,
      owner->originalTable, address(function), unsigned(function != nullptr), unsigned(readable), input.hContext, input.Flags.Value,
      input.pPrivateDriverData, input.PrivateDriverDataSize, unsigned(before.readable),
      before.values[0], before.values[1], before.values[2], before.values[4], before.values[5]);
    const HRESULT hr = function ? function(adapter, args) : E_FAIL;
    if (hr != S_OK) ++owner->failures;
    const auto after = privateWords(input.pPrivateDriverData, input.PrivateDriverDataSize);
    owner->log("SYSTEM_D3D8_CALLBACK_END kind=Escape call=%llu thread=%lu adapter=%p runtime=%p args=%p hr=%08lx forwarded=%u private_readable=%u private_magic=%08x private_version=%u private_declared=%u private16=%08x private20=%08x private24=%08x private28=%08x private32=%08x private36=%08x private40=%08x private44=%08x private48=%08x private52=%08x private56=%08x private60=%08x\n",
      call, static_cast<unsigned long>(GetCurrentThreadId()), adapter, input.hDevice, args,
      static_cast<unsigned long>(hr), unsigned(function != nullptr), unsigned(after.readable), after.values[0], after.values[1],
      after.values[2], after.values[4], after.values[5], after.values[6], after.values[7],
      after.values[8], after.values[9], after.values[10], after.values[11], after.values[12],
      after.values[13], after.values[14], after.values[15]);
    return hr;
  }
};

}
