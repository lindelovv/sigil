
project( 'sigil', 'cpp',
  version: '0.0.1',
  default_options: [
    'cpp_std=c++20'
  ]
)
#add_project_link_arguments('-fuse-ld=mold', language: ['cpp'])
add_project_arguments('-w', language: ['c', 'cpp'])

cmake = import('cmake')
compiler = meson.get_compiler('cpp')

assimp_opt = cmake.subproject_options()
assimp_opt.add_cmake_defines({
    'BUILD_SHARED_LIBS': false,
    'CMAKE_BUILD_TYPE': get_option('buildtype') == 'debug' ? 'Debug' : 'Release',
    'ASSIMP_BUILD_ASSIMP_TOOLS': false,
    'ASSIMP_BUILD_TESTS': false,
    'ASSIMP_BUILD_ZLIB': true,

    'ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT': false,
    'ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT': false,

    'ASSIMP_BUILD_GLB_IMPORTER': true,
    'ASSIMP_BUILD_OBJ_IMPORTER': true,
    'ASSIMP_BUILD_GLTF_IMPORTER': true,
})
assimp_dep = cmake.subproject('assimp', options: assimp_opt)

sources = ['src/main.cc', 'src/systems/vk_debug.cc']
executable('sigil', sources,
  include_directories: [
    include_directories('src/core'),
    include_directories('src/systems'),
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

