#pragma once
#include "umd_adapter_identity.h"
#include "umd_native_surface_wire.h"

namespace dxvk::umd {
// Adapter-scoped, read-only discovery. Never extend the private adapter-info
// capability mask consumed by the independently versioned Vulkan ICD.
HRESULT queryNativeSurfaceCapabilities(const AdapterIdentity&, uint32_t& capabilities);
struct NativeSurfaceRetirement;
// Owns the host allocation lease independently of every imported GPU image
// and VidMm wrapper. KMD retires the AHB only after all three kinds are gone.
class NativeSurfaceLease final {
public:
  static HRESULT create(const RuntimeIdentity& initial,
    std::shared_ptr<const AdapterIdentitySource> source, uint32_t width, uint32_t height,
    uint32_t fourcc, std::shared_ptr<NativeSurfaceLease>& output);
  ~NativeSurfaceLease();
  NativeSurfaceLease(const NativeSurfaceLease&) = delete;
  NativeSurfaceLease& operator=(const NativeSurfaceLease&) = delete;
  const NativeSurfaceInfo& info() const { return m_surface; }
  NativeResourceShare share() const {
    NativeResourceShare result; result.key = m_surface.key; result.stride = m_surface.stride;
    return result;
  }
  HRESULT resolve(D3DKMT_HANDLE allocation, D3DKMT_HANDLE& resource) const;
  HRESULT close();
  static HRESULT retire(std::shared_ptr<NativeSurfaceLease>& owner);
  static HRESULT drainRetired(const RuntimeIdentity* identity = nullptr);
private:
  NativeSurfaceLease();
  void deferRetirement();
  HRESULT identity() const;
  RuntimeIdentity m_initial;
  std::shared_ptr<const AdapterIdentitySource> m_source;
  NativeSurfaceInfo m_surface;
  std::unique_ptr<NativeSurfaceRetirement> m_retirement;
  bool m_owned = false, m_closing = false;
};
}
