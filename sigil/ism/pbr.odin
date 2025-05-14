
package ism

import vk "vendor:vulkan"
import "core:math/linalg/glsl"
import "lib:slang"
import "core:slice"
import "core:fmt"

//_____________________________
pbr: struct {
    using _ : material_t,
    albedo            : u32,
    normal            : u32,
    metal_roughness   : u32,
    emissive          : u32,
    ambient_occlusion : u32,
}

pbr_uniform_t :: struct {
    color_factors           : glsl.vec4,
    metal_roughness_factors : glsl.vec4,
    padding                 : [14]glsl.vec4,
}
pbr_uniform := pbr_uniform_t {
    color_factors           = { 1, 1, 1, 1 },
    metal_roughness_factors = { 1, 1, 1, 1 },
}

pbr_data_t :: struct {
    albedo            : u32,
    normal            : u32,
    metal_roughness   : u32,
    emissive          : u32,
    ambient_occlusion : u32,
}

pbr_declare :: proc(global_session: ^slang.IGlobalSession) {

    code, diagnostics: ^slang.IBlob
    target_desc := slang.TargetDesc {
		structureSize = size_of(slang.TargetDesc),
		format        = .SPIRV,
		flags         = { .GENERATE_SPIRV_DIRECTLY },
		profile       = global_session->findProfile("sm_6_0"),
	}

	compiler_option_entries := [?]slang.CompilerOptionEntry{
		{ name = .VulkanUseEntryPointName, value = { intValue0 = 1 } },
	}
	session_desc := slang.SessionDesc {
		structureSize            = size_of(slang.SessionDesc),
		targets                  = &target_desc,
		targetCount              = 1,
		compilerOptionEntries    = &compiler_option_entries[0],
		compilerOptionEntryCount = 1,
	}

	session: ^slang.ISession
	__ensure(
        global_session->createSession(session_desc, &session),
        msg = "failed to create slang session"
    )
	defer session->release()

	module := session->loadModule("sigil/ism/shaders/pbr.slang", &diagnostics)
    __ensure(module, "failed to load module")

	defer module->release()

    if diagnostics != nil {
		buffer := slice.bytes_from_ptr(
			diagnostics->getBufferPointer(),
			int(diagnostics->getBufferSize()),
		)
		assert(false, string(buffer))
	}

	vertex_entry: ^slang.IEntryPoint
	__ensure(
        module->findEntryPointByName("vs_main", &vertex_entry),
        msg = "failed to find vertex entry point"
    )

	fragment_entry: ^slang.IEntryPoint
	__ensure(
        module->findEntryPointByName("fs_main", &fragment_entry),
        msg = "failed to find fragmnet entry point"
    )

	if vertex_entry == nil {
		fmt.println("Expected 'vs_main' entry point")
		return;
	}
	if fragment_entry == nil {
		fmt.println("Expected 'fs_main' entry point")
		return;
	}

	components: [3]^slang.IComponentType = { module, vertex_entry, fragment_entry }

	linked_program: ^slang.IComponentType
	__ensure(
        session->createCompositeComponentType(&components[0], len(components), &linked_program, &diagnostics),
        msg = "failed to create component types"
    )
    if diagnostics != nil {
		buffer := slice.bytes_from_ptr(
			diagnostics->getBufferPointer(),
			int(diagnostics->getBufferSize()),
		)
		assert(false, string(buffer))
	}

	target_code: ^slang.IBlob
	__ensure(linked_program->getTargetCode(0, &target_code, &diagnostics))
    //if diagnostics != nil {
	//	buffer := slice.bytes_from_ptr(
	//		diagnostics->getBufferPointer(),
	//		int(diagnostics->getBufferSize()),
	//	)
	//	assert(false, string(buffer))
	//}

	code_size := target_code->getBufferSize()
	source_code := slice.bytes_from_ptr(target_code->getBufferPointer(), auto_cast code_size)

	info := vk.ShaderModuleCreateInfo {
		sType    = .SHADER_MODULE_CREATE_INFO,
		codeSize = len(source_code),
        pCode    = raw_data(slice.reinterpret([]u32, source_code)),
    }

	vk_module: vk.ShaderModule
	__ensure(
        vk.CreateShaderModule(device, &info, nil, &vk_module),
        msg = "failed to create shader module"
    )

    fmt.printfln("-------------------")

    // ---------------
    
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
    __ensure(
        vk.CreateDescriptorSetLayout(device, &pbr_layout_info, nil, &pbr.set_layout),
        msg = "Failed to create descriptor set"
    )

    pbr_allocate_info := vk.DescriptorSetAllocateInfo {
        sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
        descriptorPool     = desc_pool,
        descriptorSetCount = 1,
        pSetLayouts        = &pbr.set_layout,
    }
    __ensure(
        vk.AllocateDescriptorSets(device, &pbr_allocate_info, &pbr.set),
        msg = "Failed to allocate descriptor set"
    )

    pbr_push_const := vk.PushConstantRange {
        stageFlags = { .VERTEX, .FRAGMENT },
        offset     = 0,
        size       = size_of(gpu_push_constants_t),
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
    __ensure(
        vk.CreatePipelineLayout(device, &pipeline_layout_info, nil, &pbr.pipeline_layout),
        msg = "Failed to create graphics pipeline layout"
    )

    pbr_buffer := create_buffer(size_of(pbr_uniform_t), { .UNIFORM_BUFFER }, .CPU_TO_GPU)
    {
        pbr_buffer_data := cast(^pbr_uniform_t)pbr_buffer.info.pMappedData
        pbr_buffer_data.color_factors           = { 1, 1, 1, 1 }
        pbr_buffer_data.metal_roughness_factors = { 1, 1, 1, 1 }
    }
    color_img     := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_albedo.jpg")
    mtl_rough_img := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_metalRoughness.jpg")
    normal_img    := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_normal.jpg")
    emissive_img  := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_emissive.jpg")
    ao_img        := load_image(.R8G8B8A8_UNORM, { .SAMPLED }, "res/textures/Default_AO.jpg")
    
    desc := descriptor_data_t { set = pbr.set }
    pbr_writes := []vk.WriteDescriptorSet {
        write_descriptor(&desc, 0, .UNIFORM_BUFFER, descriptor_buffer_info_t { pbr_buffer, size_of(pbr_uniform_t), 0 }),
        write_descriptor(&desc, 1, .COMBINED_IMAGE_SAMPLER, descriptor_image_info_t { color_img.view,     sampler, .SHADER_READ_ONLY_OPTIMAL }),
        write_descriptor(&desc, 2, .COMBINED_IMAGE_SAMPLER, descriptor_image_info_t { mtl_rough_img.view, sampler, .SHADER_READ_ONLY_OPTIMAL }),
        write_descriptor(&desc, 3, .COMBINED_IMAGE_SAMPLER, descriptor_image_info_t { normal_img.view,    sampler, .SHADER_READ_ONLY_OPTIMAL }),
        write_descriptor(&desc, 4, .COMBINED_IMAGE_SAMPLER, descriptor_image_info_t { emissive_img.view,  sampler, .SHADER_READ_ONLY_OPTIMAL }),
        write_descriptor(&desc, 5, .COMBINED_IMAGE_SAMPLER, descriptor_image_info_t { ao_img.view,        sampler, .SHADER_READ_ONLY_OPTIMAL }),
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
            module = vk_module,
            pName  = "vs_main"
        },
        vk.PipelineShaderStageCreateInfo {
            sType  = .PIPELINE_SHADER_STAGE_CREATE_INFO,
            stage  = { .FRAGMENT },
            module = vk_module,
            pName  = "fs_main"
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
        sType = .PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
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

