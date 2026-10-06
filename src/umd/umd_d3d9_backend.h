#pragma once

#include "umd_identity.h"
#include "umd_runtime_bridge.h"
#include <d3d9.h>

namespace dxvk::umd {

struct D3D9SurfaceDesc {
  UINT width = 0, height = 0;
  D3DFORMAT format = D3DFMT_UNKNOWN;
  bool renderTarget = false, systemMemory = false, lockable = true;
  void* systemData = nullptr;
  UINT systemPitch = 0;
};

// Private renderer storage, never a runtime handle. Release on a pumped
// backend worker before the runtime resource and its system backing expire.
class D3D9SurfaceResource {
public:
  D3D9SurfaceResource();
  ~D3D9SurfaceResource();
  D3D9SurfaceResource(const D3D9SurfaceResource&) = delete;
  D3D9SurfaceResource& operator=(const D3D9SurfaceResource&) = delete;
private:
  friend class D3D9Backend;
  struct State;
  std::unique_ptr<State> m_state;
};

class D3D9VertexDeclaration {
public:
  D3D9VertexDeclaration();
  ~D3D9VertexDeclaration();
  D3D9VertexDeclaration(const D3D9VertexDeclaration&) = delete;
  D3D9VertexDeclaration& operator=(const D3D9VertexDeclaration&) = delete;
private:
  friend class D3D9Backend;
  struct State;
  std::unique_ptr<State> m_state;
};

class D3D9Backend {
public:
  D3D9Backend();
  ~D3D9Backend();
  D3D9Backend(const D3D9Backend&) = delete;
  D3D9Backend& operator=(const D3D9Backend&) = delete;

  IDirect3DDevice9Ex* device() const noexcept;
  HRESULT flush() noexcept;
  HRESULT createSurface(const D3D9SurfaceDesc& desc,
                        std::unique_ptr<D3D9SurfaceResource>& result);
  HRESULT setRenderTarget(D3D9SurfaceResource* target);
  HRESULT clear(D3DCOLOR color, UINT count, const RECT* rects, bool computeRects);
  HRESULT copySurface(D3D9SurfaceResource& destination, const RECT& destinationRect,
                      D3D9SurfaceResource& source, const RECT& sourceRect);
  HRESULT lockSurface(D3D9SurfaceResource& surface, const RECT* area,
                      DWORD flags, D3DLOCKED_RECT& result);
  HRESULT unlockSurface(D3D9SurfaceResource& surface, bool upload = true);
  HRESULT createVertexDeclaration(const D3DVERTEXELEMENT9* elements,
                                 std::unique_ptr<D3D9VertexDeclaration>& result);
  HRESULT setVertexDeclaration(D3D9VertexDeclaration* declaration);
  HRESULT setRenderState(D3DRENDERSTATETYPE state, DWORD value);
  HRESULT setScene(bool capture);
  HRESULT setSoftwareVertexProcessing(bool enable);
  HRESULT setViewport(UINT x, UINT y, UINT width, UINT height);
  HRESULT setZRange(float minimum, float maximum);
  HRESULT setScissorRect(const RECT& area);
  HRESULT drawPrimitive(D3DPRIMITIVETYPE type, UINT count, const void* vertices, UINT stride);

  // The caller pumps the original runtime's callbacks during construction,
  // rendering and synchronous destruction. Runtime ownership is mandatory.
  static HRESULT create(const AdapterLuid& luid, const RuntimeBackend* runtime,
                        std::unique_ptr<D3D9Backend>& result) noexcept;

private:
  struct State;
  std::unique_ptr<State> m_state;
};

}
