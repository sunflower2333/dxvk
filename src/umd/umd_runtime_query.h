#pragma once

#include <windows.h>
#include <d3dumddi.h>
#include "umd_runtime_identity.h"

namespace dxvk::umd {

HRESULT queryRuntimeIdentity(HANDLE runtime,
  PFND3DDDI_QUERYADAPTERINFOCB query, RuntimeIdentity& result);

}
