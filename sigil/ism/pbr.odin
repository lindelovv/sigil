
package ism

import vk "vendor:vulkan"
import "core:math/linalg/glsl"

//_____________________________
pbr: struct {
    using _ : material,
    color             : AllocatedImage,
    metal_roughness   : AllocatedImage,
    normal            : AllocatedImage,
    emissive          : AllocatedImage,
    ambient_occlusion : AllocatedImage,
}

PBR_Uniform :: struct {
    color_factors           : glsl.vec4,
    metal_roughness_factors : glsl.vec4,
    padding                 : [14]glsl.vec4,
}
pbr_uniform := PBR_Uniform {
    color_factors           = { 1, 1, 1, 1 },
    metal_roughness_factors = { 1, 1, 1, 1 },
}

pbr_declare :: proc() {
    pbr_layout_bindings := []vk.DescriptorSetLayoutBinding {
        vk.DescriptorSetLayoutBinding {
            binding         = 0,
            descriptorType  = .UNIFORM_BUFFER,
            descriptorCount = 1,
            stageFlags      = { .VERTEX, .FRAGMENT },
        },
        vk.DescriptorSetLayoutBinding {
            binding         = 1,
            descriptorType  = .COMBINED_IMAGE_SAMPLER,
            descriptorCount = 1,
            stageFlags      = { .VERTEX, .FRAGMENT },
        },
        vk.DescriptorSetLayoutBinding {
            binding         = 2,
            descriptorType  = .COMBINED_IMAGE_SAMPLER,
            descriptorCount = 1,
            stageFlags      = { .VERTEX, .FRAGMENT },
        },
        vk.DescriptorSetLayoutBinding {
            binding         = 3,
            descriptorType  = .COMBINED_IMAGE_SAMPLER,
            descriptorCount = 1,
            stageFlags      = { .VERTEX, .FRAGMENT },
        },
        vk.DescriptorSetLayoutBinding {
            binding         = 4,
            descriptorType  = .COMBINED_IMAGE_SAMPLER,
            descriptorCount = 1,
            stageFlags      = { .VERTEX, .FRAGMENT },
        },
        vk.DescriptorSetLayoutBinding {
            binding         = 5,
            descriptorType  = .COMBINED_IMAGE_SAMPLER,
            descriptorCount = 1,
            stageFlags      = { .VERTEX, .FRAGMENT },
        },
    }
    pbr_layout_info := vk.DescriptorSetLayoutCreateInfo {
        sType        = .DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        flags        = {},
        bindingCount = u32(len(pbr_layout_bindings)),
        pBindings    = raw_data(pbr_layout_bindings),
    }
    __ensure(vk.CreateDescriptorSetLayout(device, &pbr_layout_info, nil, &pbr.set_layout), "Failed to create descriptor set")

    pbr_allocate_info := vk.DescriptorSetAllocateInfo {
        sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
        descriptorPool     = desc_pool,
        descriptorSetCount = 1,
        pSetLayouts        = &pbr.set_layout,
    }
    __ensure(vk.AllocateDescriptorSets(device, &pbr_allocate_info, &pbr.set), "Failed to allocate descriptor set")

    pbr_push_const := vk.PushConstantRange {
        stageFlags = { .VERTEX, .FRAGMENT },
        offset     = 0,
        size       = size_of(GPU_PushConstants),
    }
    layouts := []vk.DescriptorSetLayout {
        scene_data.set_layout,
        pbr.set_layout
    }
    pipeline_layout_info := vk.PipelineLayoutCreateInfo {
        sType                  = .PIPELINE_LAYOUT_CREATE_INFO,
        setLayoutCount         = u32(len(layouts)),
        pSetLayouts            = raw_data(layouts),
        pushConstantRangeCount = 1,
        pPushConstantRanges    = &pbr_push_const,
    }
    __ensure(vk.CreatePipelineLayout(device, &pipeline_layout_info, nil, &pbr.pipeline_layout), "Failed to create graphics pipeline layout")

    pbr_buffer := create_buffer(size_of(PBR_Uniform), { .UNIFORM_BUFFER }, .CPU_TO_GPU)
    {
        pbr_buffer_data := cast(^PBR_Uniform)pbr_buffer.info.pMappedData
        pbr_buffer_data.color_factors           = { 1, 1, 1, 1 }
        pbr_buffer_data.metal_roughness_factors = { 1, 1, 1, 1 }
    }
    color_img     := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_albedo.jpg")
    mtl_rough_img := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_metalRoughness.jpg")
    normal_img    := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_normal.jpg")
    emissive_img  := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_emissive.jpg")
    ao_img        := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_AO.jpg")
    
    desc := DescriptorData { set = pbr.set }
    pbr_writes := []vk.WriteDescriptorSet {
        write_descriptor(&desc, 0, .UNIFORM_BUFFER, DescriptorBufferInfo { pbr_buffer, size_of(PBR_Uniform), 0 }),
        write_descriptor(&desc, 1, .COMBINED_IMAGE_SAMPLER, DescriptorImageInfo { color_img.view,     sampler, .SHADER_READ_ONLY_OPTIMAL }),
        write_descriptor(&desc, 2, .COMBINED_IMAGE_SAMPLER, DescriptorImageInfo { mtl_rough_img.view, sampler, .SHADER_READ_ONLY_OPTIMAL }),
        write_descriptor(&desc, 3, .COMBINED_IMAGE_SAMPLER, DescriptorImageInfo { normal_img.view,    sampler, .SHADER_READ_ONLY_OPTIMAL }),
        write_descriptor(&desc, 4, .COMBINED_IMAGE_SAMPLER, DescriptorImageInfo { emissive_img.view,  sampler, .SHADER_READ_ONLY_OPTIMAL }),
        write_descriptor(&desc, 5, .COMBINED_IMAGE_SAMPLER, DescriptorImageInfo { ao_img.view,        sampler, .SHADER_READ_ONLY_OPTIMAL }),
    }
    vk.UpdateDescriptorSets(device, u32(len(pbr_writes)), raw_data(pbr_writes), 0, nil)

    vertex_shader := create_shader_module("res/shaders/pbr.vert.spv")
    defer vk.DestroyShaderModule(device, vertex_shader, nil)

    fragment_shader := create_shader_module("res/shaders/pbr.frag.spv")
    defer vk.DestroyShaderModule(device, fragment_shader, nil)

    stages := []vk.PipelineShaderStageCreateInfo {
        vk.PipelineShaderStageCreateInfo {
            sType  = .PIPELINE_SHADER_STAGE_CREATE_INFO,
            stage  = { .VERTEX },
            module = vertex_shader,
            pName  = "main"
        },
        vk.PipelineShaderStageCreateInfo {
            sType  = .PIPELINE_SHADER_STAGE_CREATE_INFO,
            stage  = { .FRAGMENT },
            module = fragment_shader,
            pName  = "main"
        },
    }

    input_asm := vk.PipelineInputAssemblyStateCreateInfo {
        sType    = .PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        topology = .TRIANGLE_LIST,
    }
    rasterizer := vk.PipelineRasterizationStateCreateInfo {
        sType       = .PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        polygonMode = .FILL,
        lineWidth   = 1,
        cullMode    = { .BACK },
        frontFace   = .COUNTER_CLOCKWISE,
    }
    multisampling := vk.PipelineMultisampleStateCreateInfo {
        sType                 = .PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        sampleShadingEnable   = false,
        rasterizationSamples  = { ._1 },
        minSampleShading      = 1.0,
        pSampleMask           = nil,
        alphaToCoverageEnable = false,
        alphaToOneEnable      = false,
    }
    color_attachment := vk.PipelineColorBlendAttachmentState {
        colorWriteMask      = { .R, .G, .B, .A },
        blendEnable         = false,
        //srcColorBlendFactor = .SRC_ALPHA,
        //dstColorBlendFactor = .ONE_MINUS_SRC_ALPHA,
        //colorBlendOp        = .ADD,
        //srcAlphaBlendFactor = .ONE,
        //dstAlphaBlendFactor = .ZERO,
        //alphaBlendOp        = .ADD,
    }
    color_blend := vk.PipelineColorBlendStateCreateInfo {
        sType           = .PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        logicOpEnable   = false,
        logicOp         = .COPY,
        attachmentCount = 1,
        pAttachments    = &color_attachment,
    }
    depth_stencil := vk.PipelineDepthStencilStateCreateInfo {
        sType                 = .PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        depthTestEnable       = true,
        depthWriteEnable      = true,
        depthCompareOp        = .GREATER_OR_EQUAL,
    }
    render_info := vk.PipelineRenderingCreateInfo {
        sType                   = .PIPELINE_RENDERING_CREATE_INFO,
        colorAttachmentCount    = 1,
        pColorAttachmentFormats = &draw_img.format,
        depthAttachmentFormat   = depth_img.format,
    }
    viewport_state := vk.PipelineViewportStateCreateInfo {
        sType         = .PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        viewportCount = 1,
        scissorCount  = 1,
    }
    vertex_input := vk.PipelineVertexInputStateCreateInfo {
        sType = .PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    }
    states := []vk.DynamicState { .VIEWPORT, .SCISSOR }
    dynamic_sate := vk.PipelineDynamicStateCreateInfo {
        sType             = .PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        dynamicStateCount = u32(len(states)),
        pDynamicStates    = raw_data(states),
    }
    graphics_pipe_info := vk.GraphicsPipelineCreateInfo {
        sType               = .GRAPHICS_PIPELINE_CREATE_INFO,
        pNext               = &render_info,
        stageCount          = u32(len(stages)),
        pStages             = raw_data(stages),
        pVertexInputState   = &vertex_input,
        pInputAssemblyState = &input_asm,
        pViewportState      = &viewport_state,
        pRasterizationState = &rasterizer,
        pMultisampleState   = &multisampling,
        pDepthStencilState  = &depth_stencil,
        pColorBlendState    = &color_blend,
        pDynamicState       = &dynamic_sate,
        layout              = pbr.pipeline_layout
    }
    __ensure(vk.CreateGraphicsPipelines(device, 0, 1, &graphics_pipe_info, nil, &pbr.pipeline), "Failed to create PBR Pipeline")
}

