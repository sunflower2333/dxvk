#pragma once

#include "umd_ddi.h"
#include "umd_adapter_identity.h"

namespace dxvk::umd {

HRESULT queryRuntimeIdentity(D3D10DDI_HRTADAPTER runtime,
  PFND3DDDI_QUERYADAPTERINFOCB query, RuntimeIdentity& result);

HRESULT createAdapterDevice(const std::shared_ptr<const AdapterIdentity>& identity,
  D3D10DDIARG_CREATEDEVICE* args);

}
