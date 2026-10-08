// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>

// Readonly diagnostics for this unregistered test DLL; never a production ABI.
enum class VioGpuD11Call : UINT { Open, Versions, Caps, PrivateSize, Create, Close };
constexpr UINT VioGpuD11EventCapacity = 128;
struct VioGpuD11Event {
  UINT sequence, completed;
  VioGpuD11Call call;
  HRESULT result;
  UINT interfaceVersion, version, flags, type, dataSize, capacity, count;
  UINT caps, capturedVersions;
  UINT64 argument, adapter, runtimeAdapter, kernelCallbacks, coreCallbacks;
  UINT64 returnSize;
  UINT64 versions[8];
};
struct VioGpuD11EntryInfo {
  UINT size, schema, eventCount, overflow, liveAdapters;
  WCHAR corePath[32768];
  VioGpuD11Event events[VioGpuD11EventCapacity];
};
using VioGpuD11ReadEntry = HRESULT(APIENTRY *)(VioGpuD11EntryInfo*);

