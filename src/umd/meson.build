umd_backend = static_library('dxvk-umd-backend', 'umd_backend.cpp',
  link_with : [d3d11_umd_core],
  dependencies : [dxvk_dep, dxbc_spirv_dep, lib_gdi32, lib_dxgi],
  include_directories : dxvk_include_path,
)

umd_backend_probe = executable('dxvk-umd-backend-probe',
  '../../tests/umd-backend-probe.cpp',
  link_with : [umd_backend],
  dependencies : [dxvk_dep, dxbc_spirv_dep, lib_gdi32, lib_dxgi],
  include_directories : dxvk_include_path,
)

umd_development = shared_library('viogpudxvk', 'umd_ddi.cpp', 'umd_log.cpp',
  link_with : [umd_backend],
  dependencies : [dxvk_dep, dxbc_spirv_dep, lib_gdi32, lib_dxgi],
  include_directories : dxvk_include_path,
  vs_module_defs : 'viogpudxvk.def',
  name_prefix : '',
)

umd_ddi_probe = executable('dxvk-umd-ddi-probe', '../../tests/umd-ddi-probe.cpp',
  link_with : [umd_development],
)
