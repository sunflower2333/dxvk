#pragma once

#include <windows.h>
#include <d3d9types.h>
#include <d3dumddi.h>
#include "umd_runtime_identity.h"

namespace dxvk::umd {

// Diagnostic labels only. They do not select a query or alter its contract.
enum class RuntimeQueryStage {
  Continuity, OpenAdapter, GetSupportedVersions,
  GetCapsThreading, GetCapsShader, GetCapsPipelines,
  CalcPrivateDeviceSize, CreateDeviceBeforeBackend, CreateDeviceAfterBackend
};

HRESULT queryRuntimeIdentity(HANDLE runtime,
  PFND3DDDI_QUERYADAPTERINFOCB query, RuntimeIdentity& result,
  RuntimeQueryStage stage = RuntimeQueryStage::Continuity);

}
