#pragma once
// SPDX-License-Identifier: MIT
#include <dxgiformat.h>
#include "umd_copy_format.h"

namespace dxvk::umd {

// PRESENT backbuffers permit fully typed views within their bit layout. The
// private renderer has no swapchain/PRESENT bind, so store a typeless image
// and retain the DDI's original format separately for Blt and presentation.
constexpr DXGI_FORMAT presentCacheFormat(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_TYPELESS;
    default: return format;
  }
}
constexpr bool presentX8Format(DXGI_FORMAT format) {
  return format == DXGI_FORMAT_B8G8R8X8_UNORM
      || format == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
}

// A mutable private image must not expand the runtime's view family. Ordinary
// fully typed textures still allow only the original interpretation.
constexpr bool presentViewAllowed(DXGI_FORMAT resource, DXGI_FORMAT view, bool present) {
  if (presentCacheFormat(resource) == resource) return true;
  if (!present) return resource == view;
  const auto family = copyFormatFamily(resource);
  return copyFormatFamily(view) == family && view != family;
}

}
