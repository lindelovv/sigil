package ism

import vk "vendor:vulkan"
import glm "core:math/linalg/glsl"
import "lib:slang"
import "core:slice"
import "core:mem"
import "core:os"
import "core:fmt"
import sigil "sigil:core"

shadow_push_constant_t :: struct #align(16) {
    vertex_buffer : vk.DeviceAddress,
    model         : u32,
}

shadow_pipeline: vk.Pipeline
shadow_pipeline_layout: vk.PipelineLayout

shadow_declare :: proc(global_session: ^slang.IGlobalSession) {
    shadow_extent := vk.Extent3D { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1 }
    shadow_data.smap = create_image(.D32_SFLOAT, { .DEPTH_STENCIL_ATTACHMENT, .SAMPLED }, shadow_extent)
    fb_attachments := []vk.ImageView { shadow_data.smap.view }
    fb_info := vk.FramebufferCreateInfo {
        sType           = .FRAMEBUFFER_CREATE_INFO,
        attachmentCount = 1,
        pAttachments    = raw_data(fb_attachments),
        width           = SHADOW_MAP_SIZE,
        height          = SHADOW_MAP_SIZE,
        layers          = 1,
    }
    vk.CreateFramebuffer(device.handle, &fb_info, nil, &shadow_data.fbo)

    shadow_data.smap.index = 0//register_image(shadow_data.smap.view, 3)
    shadow_image_info := vk.DescriptorImageInfo {
        sampler     = sampler,
        imageView   = shadow_data.smap.view,
        imageLayout = .SHADER_READ_ONLY_OPTIMAL,
    }
    shadow_write := vk.WriteDescriptorSet {
        sType           = .WRITE_DESCRIPTOR_SET,
        dstSet          = bindless.set,
        dstBinding      = 3,
        dstArrayElement = 0,
        descriptorType  = .SAMPLED_IMAGE,
        pImageInfo      = &shadow_image_info,
        descriptorCount = 1,
    }
    vk.UpdateDescriptorSets(device.handle, 1, &shadow_write, 0, nil)

    shadow_push_const_ranges := []vk.PushConstantRange {
        { 
            stageFlags = { .VERTEX }, 
            offset = 0, 
            size = size_of(shadow_push_constant_t),
        },
    }
    
    shadow_layouts := []vk.DescriptorSetLayout { 
        scene_data.set_layout,
        bindless.set_layout,
    }
    pipeline_layout_info := vk.PipelineLayoutCreateInfo {
        sType                  = .PIPELINE_LAYOUT_CREATE_INFO,
        setLayoutCount         = u32(len(shadow_layouts)),
        pSetLayouts            = raw_data(shadow_layouts),
        pushConstantRangeCount = u32(len(shadow_push_const_ranges)),
        pPushConstantRanges    = raw_data(shadow_push_const_ranges),
    }
    __ensure(
        vk.CreatePipelineLayout(device.handle, &pipeline_layout_info, nil, &shadow_pipeline_layout),
        msg = "Failed to create shadow pipeline layout"
    )
    build_shadow_pipeline(global_session)
}

build_shadow_pipeline :: proc(global_session: ^slang.IGlobalSession) {
    source_code: []u8
    
    when ODIN_DEBUG {
        code, diagnostics: ^slang.IBlob
        target_desc := slang.TargetDesc {
            structureSize = size_of(slang.TargetDesc),
            format        = .SPIRV,
            flags         = { .GENERATE_SPIRV_DIRECTLY },
            profile       = global_session->findProfile("spirv_1_6"),
        }

        compiler_option_entries := [?]slang.CompilerOptionEntry{
            { name = .VulkanUseEntryPointName, value = { intValue0 = 1 } },
        }
        session_desc := slang.SessionDesc {
            structureSize            = size_of(slang.SessionDesc),
            targets                  = &target_desc,
            targetCount              = 1,
            compilerOptionEntries    = &compiler_option_entries[0],
            compilerOptionEntryCount = len(compiler_option_entries),
        }

        session: ^slang.ISession
        __ensure(
            global_session->createSession(session_desc, &session),
            msg = "failed to create slang session"
        )
        defer session->release()

        module := session->loadModule("sigil/ism/shaders/shadow.slang", &diagnostics)
        if module == nil {
            fmt.println("Shadow shader compile error!")
            if diagnostics != nil {
                buffer := slice.bytes_from_ptr(
                    diagnostics->getBufferPointer(),
                    int(diagnostics->getBufferSize()),
                )
                fmt.println(string(buffer))
            }
            return
        }
        defer module->release()

        vertex_entry: ^slang.IEntryPoint
        __ensure(
            module->findEntryPointByName("vs_main", &vertex_entry),
            msg = "failed to find vertex entry point"
        )

        if vertex_entry == nil {
            fmt.println("Expected 'vs_main' entry point")
            return;
        }

        components: [2]^slang.IComponentType = { module, vertex_entry }

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
            fmt.println(string(buffer))
        }

        target_code: ^slang.IBlob
        __ensure(
            linked_program->getTargetCode(0, &target_code, &diagnostics),
            msg = "failed getting shader target code"
        )
        
        if diagnostics != nil {
            buffer := slice.bytes_from_ptr(
                diagnostics->getBufferPointer(),
                int(diagnostics->getBufferSize()),
            )
            fmt.println(string(buffer))
        }

        code_size := target_code->getBufferSize()
        source_code = slice.bytes_from_ptr(target_code->getBufferPointer(), auto_cast code_size)
    } else {
        source_code = #load("shaders/shadow.spv")
    }

    info := vk.ShaderModuleCreateInfo {
        sType    = .SHADER_MODULE_CREATE_INFO,
        codeSize = len(source_code),
        pCode    = (^u32)(raw_data(slice.reinterpret([]u32, source_code))),
    }
    vk_module: vk.ShaderModule
    __ensure(
        vk.CreateShaderModule(device.handle, &info, nil, &vk_module),
        msg = "failed to create shadow shader module"
    )
    defer vk.DestroyShaderModule(device.handle, vk_module, nil)

    stages := []vk.PipelineShaderStageCreateInfo {
        {
            sType  = .PIPELINE_SHADER_STAGE_CREATE_INFO,
            stage  = { .VERTEX },
            module = vk_module,
            pName  = "vs_main"
        },
    }
    input_asm := vk.PipelineInputAssemblyStateCreateInfo {
        sType    = .PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        topology = .TRIANGLE_LIST,
    }
    rasterizer := vk.PipelineRasterizationStateCreateInfo {
        sType                   = .PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        polygonMode             = .FILL,
        lineWidth               = 1,
        cullMode                = { .BACK }, // Cull back faces for shadows
        frontFace               = .COUNTER_CLOCKWISE,
        depthBiasEnable         = true,      // Enable depth bias to avoid shadow acne
        depthBiasConstantFactor = 1.5,       // Adjust as needed
        depthBiasClamp          = 0.0,
        depthBiasSlopeFactor    = 1.5,       // Adjust as needed
    }
    multisampling := vk.PipelineMultisampleStateCreateInfo {
        sType                 = .PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        sampleShadingEnable   = false,
        rasterizationSamples  = { ._1 },
        minSampleShading      = 1.0,
    }
    color_blend := vk.PipelineColorBlendStateCreateInfo {
        sType           = .PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        logicOpEnable   = false,
        attachmentCount = 0, // No color attachments
        pAttachments    = nil,
    }
    depth_stencil := vk.PipelineDepthStencilStateCreateInfo {
        sType                 = .PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        depthTestEnable       = true,
        depthWriteEnable      = true,
        depthCompareOp        = .LESS_OR_EQUAL,
        depthBoundsTestEnable = false,
        stencilTestEnable     = false,
    }
    render_info := vk.PipelineRenderingCreateInfo {
        sType                 = .PIPELINE_RENDERING_CREATE_INFO,
        colorAttachmentCount  = 0, // No color attachments
        depthAttachmentFormat = .D32_SFLOAT, // Match your shadow map format
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
    dynamic_state := vk.PipelineDynamicStateCreateInfo {
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
        pDynamicState       = &dynamic_state,
        layout              = shadow_pipeline_layout,
    }
    
    __ensure(
        vk.CreateGraphicsPipelines(device.handle, 0, 1, &graphics_pipe_info, nil, &shadow_pipeline),
        msg = "Failed to create Shadow Pipeline"
    )
    fmt.println("Shadow pipeline created successfully")
}

shadow_current_last_write_time: os.File_Time
@(disabled=!ODIN_DEBUG) __rebuild_shadow_pipeline :: proc(gs: ^slang.IGlobalSession) {
	last_write_time, file_err := os.last_write_time_by_name("sigil/ism/shaders/shadow.slang")
	if file_err == nil && shadow_current_last_write_time != last_write_time {
	    shadow_current_last_write_time = last_write_time
        build_shadow_pipeline(gs)
    }
}

render_shadow_pass :: proc(world: ^sigil.world_t, cmd: vk.CommandBuffer) {
    if shadow_pipeline == vk.Pipeline(0) {
        fmt.eprintln("Shadow pipeline not initialized!")
        return
    }

    light_pos := glm.vec3  { 10.0, 10.0, 10.0 }
    target_pos := glm.vec3 { 0.0, 0.0, 0.0 }
    up_vector := glm.vec3  { 0.0, 0.0, 1.0 }
    
    shadow_data.view = glm.mat4LookAt(light_pos, target_pos, up_vector)
    shadow_data.proj = glm.mat4Ortho3d(-10.0, 10.0, -10.0, 10.0, 1.0, 30.0)

    //gpu_scene_data.light_view = shadow_data.view
    //gpu_scene_data.light_proj = shadow_data.proj
    //mem.copy(scene_allocation.info.pMappedData, &gpu_scene_data, size_of(gpu_scene_data_t))

    depth_attachment := vk.RenderingAttachmentInfo {
        sType       = .RENDERING_ATTACHMENT_INFO,
        imageView   = shadow_data.smap.view,
        imageLayout = .DEPTH_ATTACHMENT_OPTIMAL,
        loadOp      = .CLEAR,
        storeOp     = .STORE,
        clearValue  = vk.ClearValue { depthStencil = { depth = 1.0 } },
    }
    
    render_info := vk.RenderingInfo {
        sType            = .RENDERING_INFO,
        renderArea       = { {0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE} },
        layerCount       = 1,
        pDepthAttachment = &depth_attachment,
    }
    
    transition_img(cmd, shadow_data.smap.handle, .UNDEFINED, .DEPTH_ATTACHMENT_OPTIMAL)
    
    vk.CmdBeginRendering(cmd, &render_info)
    {
        viewport := vk.Viewport { 
            width = f32(SHADOW_MAP_SIZE), 
            height = f32(SHADOW_MAP_SIZE), 
            minDepth = 0, 
            maxDepth = 1 
        }
        vk.CmdSetViewport(cmd, 0, 1, &viewport)
        
        scissor := vk.Rect2D { extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE} }
        vk.CmdSetScissor(cmd, 0, 1, &scissor)
        
        vk.CmdBindPipeline(cmd, .GRAPHICS, shadow_pipeline)
        
        shadow_sets := []vk.DescriptorSet { 
            frames[current_frame].descriptor.set,
            bindless.set,
        }
        vk.CmdBindDescriptorSets(cmd, .GRAPHICS, shadow_pipeline_layout, 0, u32(len(shadow_sets)), raw_data(shadow_sets), 0, nil)

        for &q, i in sigil.query(world, render_data_t, transform_t) {
            data, _ := q.x, glm.mat4(q.y)
            
            shadow_push_const := shadow_push_constant_t {
                vertex_buffer = data.address,
                model         = u32(i),
            }
            vk.CmdPushConstants(cmd, shadow_pipeline_layout, {.VERTEX}, 0, size_of(shadow_push_constant_t), &shadow_push_const)
            
            vk.CmdBindIndexBuffer(cmd, data.idx_buffer, 0, .UINT32)
            vk.CmdDrawIndexed(cmd, data.count, 1, data.first, 0, 0)
        }
    }
    vk.CmdEndRendering(cmd)
}
