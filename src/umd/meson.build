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

umd_development = shared_library('viogpudxvk', 'umd_ddi.cpp', 'umd_adapter.cpp', 'umd_contract.cpp', 'umd_shader.cpp', 'umd_runtime_query.cpp', 'umd_log.cpp', 'umd_allocation.cpp', 'umd_runtime_gpu.cpp',
  link_with : [umd_backend],
  dependencies : [dxvk_dep, dxbc_spirv_dep, lib_gdi32, lib_dxgi],
  include_directories : dxvk_include_path,
  vs_module_defs : 'viogpudxvk.def',
  name_prefix : '',
)

umd_ddi_probe = executable('dxvk-umd-ddi-probe', '../../tests/umd-ddi-probe.cpp',
  link_with : [umd_development],
  dependencies : [cpp.find_library('d3dcompiler'), lib_gdi32],
)

umd_predication_probe = executable('dxvk-umd-predication-probe', '../../tests/umd-predication.cpp',
  link_with : [umd_development],
  dependencies : [cpp.find_library('d3dcompiler')])

umd_stream_output_probe = executable('dxvk-umd-stream-output-probe', '../../tests/umd-stream-output.cpp',
  link_with : [umd_development],
  dependencies : [cpp.find_library('d3dcompiler')])

umd_shader_test = executable('dxvk-umd-shader-test',
  '../../tests/umd-shader-container.cpp', 'umd_shader.cpp',
  dependencies : [dxbc_spirv_dep, cpp.find_library('d3dcompiler'), cpp.find_library('d3d11')],
  cpp_args : ['-DVIOGPU_SHADER_SPIRV_TEST'],
)

umd_identity_query_test = executable('dxvk-umd-identity-query-test',
  '../../tests/umd-runtime-query.cpp', 'umd_runtime_query.cpp',
)

umd_adapter_test = executable('dxvk-umd-adapter-test',
  '../../tests/umd-adapter.cpp', 'umd_adapter.cpp', 'umd_runtime_query.cpp', 'umd_contract.cpp',
)

native_entry_sources = ['../../tests/umd-native-entry.cpp', 'umd_adapter.cpp',
  'umd_runtime_query.cpp', 'umd_ddi.cpp', 'umd_shader.cpp', 'umd_allocation.cpp', 'umd_runtime_gpu.cpp']
native_entry_dependencies = [dxbc_spirv_dep, cpp.find_library('d3d11')]
umd_predication_test = executable('dxvk-umd-predication-test',
  ['../../tests/umd-predication.cpp', 'umd_ddi.cpp', 'umd_shader.cpp',
   'umd_runtime_query.cpp', 'umd_allocation.cpp', 'umd_runtime_gpu.cpp'],
  dependencies : native_entry_dependencies + [cpp.find_library('d3dcompiler')],
  cpp_args : ['-DVIOGPU_PREDICATION_WARP'])
umd_stream_output_test = executable('dxvk-umd-stream-output-test',
  ['../../tests/umd-stream-output.cpp', 'umd_ddi.cpp', 'umd_shader.cpp',
   'umd_runtime_query.cpp', 'umd_allocation.cpp', 'umd_runtime_gpu.cpp'],
  dependencies : native_entry_dependencies + [cpp.find_library('d3dcompiler')],
  cpp_args : ['-DVIOGPU_STREAM_OUTPUT_WARP'])
umd_native_entry_test = executable('dxvk-umd-native-entry-test',
  native_entry_sources + ['umd_contract.cpp'], dependencies : native_entry_dependencies)
umd_native_lifetime_test = executable('dxvk-umd-native-lifetime-test',
  native_entry_sources, dependencies : native_entry_dependencies,
  cpp_args : ['-DVIOGPU_TEST_COMPLETE_CONTRACT'])

umd_mrt_test = executable('dxvk-umd-mrt-test',
  ['../../tests/umd-mrt.cpp', 'umd_ddi.cpp', 'umd_shader.cpp',
   'umd_runtime_query.cpp', 'umd_allocation.cpp', 'umd_runtime_gpu.cpp'],
  dependencies : native_entry_dependencies + [cpp.find_library('d3dcompiler')])
umd_mrt_signature_test = executable('dxvk-umd-mrt-signature-test',
  ['../../tests/umd-mrt-signature.cpp', 'umd_shader.cpp'], dependencies : [dxbc_spirv_dep])
umd_output_policy_test = executable('dxvk-umd-output-policy-test', '../../tests/umd-output-policy.cpp')

umd_query_test = executable('dxvk-umd-query-test', '../../tests/umd-query.cpp')

umd_allocation_test = executable('dxvk-umd-allocation-test',
  '../../tests/umd-allocation.cpp', 'umd_allocation.cpp', 'umd_runtime_query.cpp')

umd_runtime_gpu_test = executable('dxvk-umd-runtime-gpu-test',
  '../../tests/umd-runtime-gpu.cpp', 'umd_runtime_gpu.cpp', 'umd_runtime_query.cpp',
  'umd_ddi.cpp', 'umd_allocation.cpp', 'umd_shader.cpp',
  dependencies : native_entry_dependencies)

umd_view_test = executable('dxvk-umd-view-test', '../../tests/umd-view.cpp',
  dependencies : [cpp.find_library('d3d11'), cpp.find_library('d3dcompiler')])

umd_system_runtime_test = executable('dxvk-umd-system-runtime-test',
  '../../tests/umd-system-runtime.cpp',
  dependencies : [cpp.find_library('d3d11'), cpp.find_library('d3dcompiler'), cpp.find_library('user32')])
