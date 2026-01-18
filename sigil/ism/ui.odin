package ism

import vk "vendor:vulkan"

//_____________________________
ui : struct { using _ : material_t }

ui_declare :: proc() {
    ui_layout_bindings := []vk.DescriptorSetLayoutBinding {}
    ui_layout_info := vk.DescriptorSetLayoutCreateInfo {
        sType        = .DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        flags        = {},
        bindingCount = u32(len(ui_layout_bindings)),
        pBindings    = raw_data(ui_layout_bindings),
    }
    __ensure(vk.CreateDescriptorSetLayout(device.handle, &ui_layout_info, nil, &ui.set_layout), "Failed to create descriptor set")

    ui_allocate_info := vk.DescriptorSetAllocateInfo {
        sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
        descriptorPool     = desc_pool,
        descriptorSetCount = 1,
        pSetLayouts        = &ui.set_layout,
    }
    __ensure(vk.AllocateDescriptorSets(device.handle, &ui_allocate_info, &ui.set), "Failed to allocate descriptor set")

    ui_push_const := vk.PushConstantRange {
        stageFlags = { .VERTEX, .FRAGMENT },
        offset     = 0,
        size       = size_of(push_constant_t),
    }
    layouts := []vk.DescriptorSetLayout {
        scene_data.set_layout,
        ui.set_layout
    }
    pipeline_layout_info := vk.PipelineLayoutCreateInfo {
        sType                  = .PIPELINE_LAYOUT_CREATE_INFO,
        setLayoutCount         = u32(len(layouts)),
        pSetLayouts            = raw_data(layouts),
        pushConstantRangeCount = 1,
        pPushConstantRanges    = &ui_push_const,
    }
    __ensure(vk.CreatePipelineLayout(device.handle, &pipeline_layout_info, nil, &ui.pipeline_layout), "Failed to create graphics pipeline layout")
    
    //desc := descriptor_data_t { set = ui.set }
    ui_writes := []vk.WriteDescriptorSet {}
    vk.UpdateDescriptorSets(device.handle, u32(len(ui_writes)), raw_data(ui_writes), 0, nil)

    vertex_shader := create_shader_module("res/shaders/ui.vert.spv")
    defer vk.DestroyShaderModule(device.handle, vertex_shader, nil)

    fragment_shader := create_shader_module("res/shaders/ui.frag.spv")
    defer vk.DestroyShaderModule(device.handle, fragment_shader, nil)

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
        blendEnable         = true,
        srcColorBlendFactor = .SRC_ALPHA,
        dstColorBlendFactor = .ONE_MINUS_SRC_ALPHA,
        colorBlendOp        = .ADD,
        srcAlphaBlendFactor = .ONE,
        dstAlphaBlendFactor = .ZERO,
        alphaBlendOp        = .ADD,
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
        depthTestEnable       = false,
        depthWriteEnable      = false,
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
        layout              = ui.pipeline_layout
    }
    __ensure(vk.CreateGraphicsPipelines(device.handle, 0, 1, &graphics_pipe_info, nil, &ui.pipeline), "Failed to create PBR Pipeline")
}

