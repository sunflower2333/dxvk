#pragma once

#include "mesa_wddm_runtime.h"
#include <memory>

namespace dxvk::umd {
struct RuntimeBackend {
  mwd_device_create_info create = {};
  std::shared_ptr<void> owner;
};
}
