#pragma once

#include "umd_identity.h"
#include "umd_runtime_bridge.h"
#include <d3d9.h>

namespace dxvk::umd {

class D3D9Backend {
public:
  D3D9Backend();
  ~D3D9Backend();
  D3D9Backend(const D3D9Backend&) = delete;
  D3D9Backend& operator=(const D3D9Backend&) = delete;

  IDirect3DDevice9Ex* device() const noexcept;
  HRESULT flush() noexcept;

  // The caller pumps the original runtime's callbacks during construction,
  // rendering and synchronous destruction. Runtime ownership is mandatory.
  static HRESULT create(const AdapterLuid& luid, const RuntimeBackend* runtime,
                        std::unique_ptr<D3D9Backend>& result) noexcept;

private:
  struct State;
  std::unique_ptr<State> m_state;
};

}
