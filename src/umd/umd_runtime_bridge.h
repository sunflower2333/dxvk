#pragma once

#include "mesa_wddm_runtime.h"
#include <memory>

namespace dxvk::umd {
struct RuntimeBackend {
  mwd_device_create_info create = {};
  std::shared_ptr<void> owner;
  // Caller-thread native execution policy, also required by the DDI's
  // DISABLE_EXTRA_THREAD_CREATION flag. Scoped to this device only.
  bool disableExtraThreads = false;
  // Set only after the matched ICD advertises exact native image references.
  bool nativeSurfaces = false;
};
}
