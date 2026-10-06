#pragma once

#include "umd_runtime_query.h"
#include <memory>

namespace dxvk::umd {

// Shared by D3D9 and D3D10 devices; the runtime handle is opaque and is
// returned only to the query and escape callbacks which supplied it.
struct AdapterIdentity {
  LUID luid = {};
  HANDLE runtime = nullptr;
  PFND3DDDI_QUERYADAPTERINFOCB query = nullptr;
  uint64_t generation = 0;
  uint64_t capabilities = 0;
};

}
