
project( 'sigil', 'cpp',
  version: '0.0.1',
  default_options: [
    'cpp_std=c++20'
  ]
)
#add_project_link_arguments('-fuse-ld=mold', language: ['cpp'])
add_project_arguments('-w', language: ['c', 'cpp'])

sources = ['src/main.cc', 'src/systems/vulkan/vk_debug.cc']
executable('sigil', sources,
  include_directories: [
    include_directories('src/core'),
    include_directories('src/systems'),
    include_directories('src/systems/vulkan'),
  ],
  dependencies: [
    dependency('vulkan', version: ['>=1.3.0']),
    dependency('vma-hpp'),
    dependency('glfw3', default_options: ['default_library=static']),
    dependency('glm'),
    dependency('imgui', version: ['>=1.90.4'], default_options: ['default_library=static']),
    dependency('fmt', default_options: ['default_library=static']),
    dependency('assimp'),
  ]
)

