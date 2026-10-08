#pragma once
// SPDX-License-Identifier: MIT
// Diagnostics for the unregistered validation frontend, never a production ABI.
#include <windows.h>

struct VioGpuSystemValidationEntryInfo {
  UINT size;
  UINT legacyInterface;
  LONG calls;
  LONG successfulCalls;
  HRESULT lastResult;
  UINT lastInterface;
  UINT lastVersion;
  WCHAR corePath[32768];
};
using VioGpuSystemValidationReadEntry = HRESULT(APIENTRY *)(VioGpuSystemValidationEntryInfo*);
