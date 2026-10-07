#pragma once

#include "umd_ddi.h"
#include "umd_adapter_identity.h"
#include "umd_interface.h"

namespace dxvk::umd {

HRESULT queryRuntimeIdentity(D3D10DDI_HRTADAPTER runtime,
  PFND3DDDI_QUERYADAPTERINFOCB query, RuntimeIdentity& result);

HRESULT createAdapterDevice(const std::shared_ptr<const AdapterIdentity>& identity,
  D3D10DDIARG_CREATEDEVICE* args);

// Internal fixture entry, linked directly into CPU tests. The exported
// development helper retains the original legacy adapter-table ABI.
HRESULT openAdapterForTest(D3D10DDIARG_OPENADAPTER* args, bool modern);

}
