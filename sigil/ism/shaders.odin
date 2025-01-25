
package ism

import vk "vendor:vulkan"
import slang "lib:odin-slang/slang"

ShaderPipeline :: struct {
    declare           : proc(),
    rebuild           : proc(),

    module            : ^slang.IModule,
    sampler           : vk.Sampler,
    data              : vk.Buffer,
    offset            : u32,
    set_layout        : vk.DescriptorSetLayout,
    set               : vk.DescriptorSet,
    pool              : vk.DescriptorPool,
    pipeline          : vk.Pipeline,
    pipeline_layout   : vk.PipelineLayout,
}

ShaderData :: struct {
    // just per shader data
}

