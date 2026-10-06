#pragma once

#include "umd_identity.h"
#include "umd_runtime_bridge.h"
#include <d3d9.h>
#include <vector>

namespace dxvk::umd {

struct D3D9SurfaceDesc {
  UINT width = 0, height = 0;
  D3DFORMAT format = D3DFMT_UNKNOWN;
  bool renderTarget = false, depthStencil = false, systemMemory = false, lockable = true;
  void* systemData = nullptr;
  UINT systemPitch = 0;
};

struct D3D9BufferDesc {
  UINT bytes = 0, fvf = 0;
  D3DFORMAT format = D3DFMT_VERTEXDATA;
  bool index = false, dynamic = false, writeOnly = false, lockable = true;
  bool systemMemory = false;
  void* systemData = nullptr;
};

class D3D9BufferResource {
public:
  D3D9BufferResource();
  ~D3D9BufferResource();
  D3D9BufferResource(const D3D9BufferResource&) = delete;
  D3D9BufferResource& operator=(const D3D9BufferResource&) = delete;
private:
  friend class D3D9Backend;
  struct State;
  std::unique_ptr<State> m_state;
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

class D3D9TextureResource {
public:
  D3D9TextureResource();
  ~D3D9TextureResource();
  D3D9TextureResource(const D3D9TextureResource&) = delete;
  D3D9TextureResource& operator=(const D3D9TextureResource&) = delete;
private:
  friend class D3D9Backend;
  struct State;
  std::unique_ptr<State> m_state;
};

// Cropped, tightly bounded caller snapshot, owned by the DDI until copy ends.
struct D3D9SurfaceUpload {
  const void* data = nullptr;
  UINT pitch = 0;
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

enum class D3D9ShaderStage { Vertex, Pixel };

class D3D9Shader {
public:
  D3D9Shader();
  ~D3D9Shader();
  D3D9Shader(const D3D9Shader&) = delete;
  D3D9Shader& operator=(const D3D9Shader&) = delete;
private:
  friend class D3D9Backend;
  struct State;
  std::unique_ptr<State> m_state;
};

class D3D9QueryResource {
public:
  D3D9QueryResource();
  ~D3D9QueryResource();
  D3D9QueryResource(const D3D9QueryResource&) = delete;
  D3D9QueryResource& operator=(const D3D9QueryResource&) = delete;
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
  HRESULT createBuffer(const D3D9BufferDesc& desc, std::unique_ptr<D3D9BufferResource>& result,
                       const void* initialData = nullptr);
  HRESULT lockBuffer(D3D9BufferResource& buffer, UINT offset, UINT bytes, DWORD flags, void*& data);
  HRESULT unlockBuffer(D3D9BufferResource& buffer, const void* upload = nullptr);
  HRESULT copyBuffer(D3D9BufferResource& destination, UINT destinationOffset,
                     D3D9BufferResource& source, UINT sourceOffset, UINT bytes,
                     const void* upload = nullptr);
  HRESULT setStreamSource(UINT stream, D3D9BufferResource* buffer, UINT offset, UINT stride);
  HRESULT setIndices(D3D9BufferResource* buffer);
  HRESULT drawPrimitiveBuffers(D3DPRIMITIVETYPE type, UINT start, UINT count);
  HRESULT drawIndexedPrimitive(D3DPRIMITIVETYPE type, INT base, UINT minimum,
                               UINT vertices, UINT start, UINT count);
  HRESULT createSurface(const D3D9SurfaceDesc& desc,
                        std::unique_ptr<D3D9SurfaceResource>& result);
  HRESULT createTexture(const D3D9SurfaceDesc* levels, UINT count,
                        std::unique_ptr<D3D9TextureResource>& texture,
                        std::vector<std::unique_ptr<D3D9SurfaceResource>>& surfaces);
  HRESULT setTexture(UINT stage, D3D9TextureResource* texture);
  HRESULT setTextureStageState(UINT stage, D3DTEXTURESTAGESTATETYPE state, DWORD value);
  HRESULT setSamplerState(UINT stage, D3DSAMPLERSTATETYPE state, DWORD value);
  HRESULT setRenderTarget(D3D9SurfaceResource* target);
  HRESULT setDepthStencil(D3D9SurfaceResource* depth);
  HRESULT clear(DWORD flags, D3DCOLOR color, float depth, DWORD stencil,
                UINT count, const RECT* rects, bool computeRects);
  HRESULT copySurface(D3D9SurfaceResource& destination, const RECT& destinationRect,
                      D3D9SurfaceResource& source, const RECT& sourceRect,
                      const D3D9SurfaceUpload* upload = nullptr);
  HRESULT lockSurface(D3D9SurfaceResource& surface, const RECT* area,
                      DWORD flags, D3DLOCKED_RECT& result);
  HRESULT unlockSurface(D3D9SurfaceResource& surface, bool upload = true);
  HRESULT createVertexDeclaration(const D3DVERTEXELEMENT9* elements,
                                 std::unique_ptr<D3D9VertexDeclaration>& result);
  HRESULT setVertexDeclaration(D3D9VertexDeclaration* declaration);
  HRESULT createShader(D3D9ShaderStage stage, const DWORD* code, UINT bytes,
                       std::unique_ptr<D3D9Shader>& result);
  HRESULT setShader(D3D9ShaderStage stage, D3D9Shader* shader);
  HRESULT setShaderConstantF(D3D9ShaderStage stage, UINT first, UINT count, const float* values);
  HRESULT setShaderConstantI(D3D9ShaderStage stage, UINT first, UINT count, const INT* values);
  HRESULT setShaderConstantB(D3D9ShaderStage stage, UINT first, UINT count, const BOOL* values);
  HRESULT createQuery(D3DQUERYTYPE type, std::unique_ptr<D3D9QueryResource>& result);
  HRESULT issueQuery(D3D9QueryResource& query, DWORD flags);
  HRESULT getQueryData(D3D9QueryResource& query, void* data, UINT bytes);
  HRESULT setTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX& matrix, bool multiply);
  HRESULT setMaterial(const D3DMATERIAL9& material);
  HRESULT setClipPlane(UINT index, const float* plane);
  HRESULT setLight(UINT index, const D3DLIGHT9& light);
  HRESULT setLightEnabled(UINT index, bool enable);
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
