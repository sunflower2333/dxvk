vkcommon_src = files([
  'vulkan_loader.cpp',
  'vulkan_names.cpp',
])

thread_dep = dependency('threads')

# Empty: search winevulkan.dll/vulkan-1.dll as upstream does. A package build
# of the native UMD names its private loader, resolved beside the UMD itself.
vk_private_loader = get_option('umd_vulkan_loader')
if vk_private_loader != '' and (platform != 'windows'
    or not vk_private_loader.to_lower().endswith('.dll')
    or vk_private_loader.contains('/') or vk_private_loader.contains('\\')
    or vk_private_loader.contains('"'))
  error('umd_vulkan_loader must be a bare Windows DLL file name')
endif
vk_loader_conf = configuration_data()
vk_loader_conf.set_quoted('DXVK_PRIVATE_VULKAN_LOADER', vk_private_loader)
vk_loader_config = configure_file(
  configuration : vk_loader_conf,
  output        : 'vulkan_loader_config.h',
)

vkcommon_lib = static_library('vkcommon', vkcommon_src, vk_loader_config,
  dependencies        : [ thread_dep ],
  include_directories : [ dxvk_include_path ],
)

vkcommon_dep = declare_dependency(
  link_with           : [ vkcommon_lib ],
  include_directories : [ dxvk_include_path ],
)
