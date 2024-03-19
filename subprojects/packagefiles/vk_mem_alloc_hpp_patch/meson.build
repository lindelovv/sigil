project('vma_hpp', ['c', 'cpp'], version: '3.0.1-3')

add_project_arguments('-w', language: ['c', 'cpp'])

vma_hpp_dep = declare_dependency(
    include_directories: 'include',
    dependencies: dependency('vulkan-memory-allocator'),
)

