
package ism
import vk "vendor:vulkan"

ShaderData :: struct {
    pipeline       : ShaderPipeline,
    buffers        : [dynamic]AllocatedBuffer,
    push_constants : rawptr,
    pc_size        : u32,
}

ShaderPipeline :: struct {
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

