package ism
import vk "vendor:vulkan"

shader_data_t :: struct {
    pipeline       : shader_pipeline_t,
    buffers        : [dynamic]allocated_buffer_t,
    push_constants : rawptr,
    pc_size        : u32,
}

shader_pipeline_t :: struct {
    name            : string,
    sampler         : vk.Sampler,
    data            : vk.Buffer,
    offset          : u32,
    set_layout      : vk.DescriptorSetLayout,
    set             : vk.DescriptorSet,
    pool            : vk.DescriptorPool,
    pipeline        : vk.Pipeline,
    pipeline_layout : vk.PipelineLayout,
}

material_definition_t :: struct {
    albedo_texture    : u32,
    normal_texture    : u32,
    roughness_texture : u32,
    occlusion_texture : u32,
    emissive_texture  : u32,
}

