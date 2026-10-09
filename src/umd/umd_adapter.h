#pragma once

#include "umd_ddi.h"
#include "umd_adapter_identity.h"
#include "umd_interface.h"

// Dedicated forwarding target for ordinary SYSTEM D3D11 validation at FL10.0.
// It retains strict runtime callbacks and does not change production admission.
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapter11Fl10_0ForValidation(D3D10DDIARG_OPENADAPTER* args);

namespace dxvk::umd {

HRESULT queryRuntimeIdentity(D3D10DDI_HRTADAPTER runtime,
  PFND3DDDI_QUERYADAPTERINFOCB query, RuntimeIdentity& result,
  RuntimeQueryStage stage = RuntimeQueryStage::Continuity);

HRESULT createAdapterDevice(const std::shared_ptr<const AdapterIdentity>& identity,
  D3D10DDIARG_CREATEDEVICE* args);

// Internal fixture entry, linked directly into CPU tests. The exported
// development helper retains the original legacy adapter-table ABI.
HRESULT openAdapterForTest(D3D10DDIARG_OPENADAPTER* args, bool modern);

}
