
package ism

import vk "vendor:vulkan"
//import "lib:odin-slang/slang"
import "core:slice"
import "core:fmt"
import "core:time"

rect := material {}
rect_mesh : Mesh

//_____________________________
rect_vertices := [dynamic]Vertex {
    { position = {  0.5, -0.5, 0 }, color = { 0, 0, 1, 1 } },
    { position = {  0.5,  0.5, 0 }, color = { 1, 0, 1, 1 } },
    { position = { -0.5, -0.5, 0 }, color = { 1, 0, 0, 1 } },
    { position = { -0.5,  0.5, 0 }, color = { 0, 1, 0, 1 } },
}
rect_indices := [dynamic]u32 { 3, 1, 2, 2, 1, 0 }

//rect_declare :: proc(global_session: ^slang.IGlobalSession) {
	//start_compile_time := time.tick_now()
    ////
    //using slang
	//code, diagnostics: ^IBlob
	//r: Result

	//target_desc := TargetDesc {
	//	structureSize = size_of(TargetDesc),
	//	format        = .SPIRV,
	//	flags         = { .GENERATE_SPIRV_DIRECTLY },
	//	profile       = global_session->findProfile("sm_6_0"),
	//}

	//compiler_option_entries := [?]CompilerOptionEntry{
	//	{name = .VulkanUseEntryPointName, value = {intValue0 = 1}},
	//}
	//session_desc := SessionDesc {
	//	structureSize            = size_of(SessionDesc),
	//	targets                  = &target_desc,
	//	targetCount              = 1,
	//	compilerOptionEntries    = &compiler_option_entries[0],
	//	compilerOptionEntryCount = 1,
	//}

	//session: ^ISession
	//__ensure(global_session->createSession(session_desc, &session))
	//defer session->release()

	//blob: ^IBlob

	//module: ^IModule = session->loadModule("sigil/ism/shaders/rect.slang", &diagnostics)
	//if module == nil {
	//	fmt.println("Shader compile error!")
	//	return
	//}
	//defer module->release()

	//if diagnostics != nil {
	//	buffer := slice.bytes_from_ptr(
	//		diagnostics->getBufferPointer(),
	//		int(diagnostics->getBufferSize()),
	//	)
	//	assert(false, string(buffer))
	//}

	//vertex_entry: ^IEntryPoint
	//module->findEntryPointByName("vsmain", &vertex_entry)

	//fragment_entry: ^IEntryPoint
	//module->findEntryPointByName("fsmain", &fragment_entry)

	//if vertex_entry == nil {
	//	fmt.println("Expected 'vertex' entry point")
	//	return;
	//}
	//if fragment_entry == nil {
	//	fmt.println("Expected 'fragment' entry point")
	//	return;
	//}

	//components: [3]^IComponentType = { module, vertex_entry, fragment_entry }

	//linked_program: ^IComponentType
	//r = session->createCompositeComponentType(
	//	&components[0],
	//	len(components),
	//	&linked_program,
	//	&diagnostics,
	//)

	//if diagnostics != nil {
	//	buffer := slice.bytes_from_ptr(
	//		diagnostics->getBufferPointer(),
	//		int(diagnostics->getBufferSize()),
	//	)
	//	assert(false, string(buffer))
	//}
	//__ensure(r)

	//target_code: ^IBlob
	//r = linked_program->getTargetCode(0, &target_code, &diagnostics)

	//if diagnostics != nil {
	//	buffer := slice.bytes_from_ptr(
	//		diagnostics->getBufferPointer(),
	//		int(diagnostics->getBufferSize()),
	//	)
	//	assert(false, string(buffer))
	//}
	//__ensure(r)

	//code_size := target_code->getBufferSize()
	//source_code := slice.bytes_from_ptr(target_code->getBufferPointer(), auto_cast code_size)

	//info := vk.ShaderModuleCreateInfo {
	//	sType    = .SHADER_MODULE_CREATE_INFO,
	//	codeSize = len(source_code), // codeSize needs to be in bytes
	//	pCode    = raw_data(slice.reinterpret([]u32, source_code)), // code needs to be in 32bit words
	//}

	//vk_module: vk.ShaderModule
	//__ensure(vk.CreateShaderModule(device, &info, nil, &vk_module))
    //defer vk.DestroyShaderModule(device, vk_module, nil)

    //stages := []vk.PipelineShaderStageCreateInfo {
    //    vk.PipelineShaderStageCreateInfo {
    //        sType  = .PIPELINE_SHADER_STAGE_CREATE_INFO,
    //        stage  = { .VERTEX },
    //        module = vk_module,
    //        pName  = "vsmain"
    //    },
    //    vk.PipelineShaderStageCreateInfo {
    //        sType  = .PIPELINE_SHADER_STAGE_CREATE_INFO,
    //        stage  = { .FRAGMENT },
    //        module = vk_module,
    //        pName  = "fsmain"
    //    },
    //}

    //gpu_push_const := vk.PushConstantRange {
    //    stageFlags = { .VERTEX, .FRAGMENT },
    //    offset     = 0,
    //    size       = size_of(GPU_PushConstants),
    //}
    //rect_pipeline_layout := vk.PipelineLayoutCreateInfo {
    //    sType                  = .PIPELINE_LAYOUT_CREATE_INFO,
    //    pushConstantRangeCount = 1,
    //    pPushConstantRanges    = &gpu_push_const,
    //}
    //__ensure(
    //    vk.CreatePipelineLayout(device, &rect_pipeline_layout, nil, &rect.pipeline_layout), 
    //    msg = "Failed to create rect pipeline layout"
    //)

    //input_asm := vk.PipelineInputAssemblyStateCreateInfo {
    //    sType    = .PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    //    topology = .TRIANGLE_LIST,
    //}
    //rasterizer := vk.PipelineRasterizationStateCreateInfo {
    //    sType       = .PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    //    polygonMode = .FILL,
    //    lineWidth   = 1,
    //    cullMode    = {},
    //    frontFace   = .CLOCKWISE,
    //}
    //multisampling := vk.PipelineMultisampleStateCreateInfo {
    //    sType                 = .PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    //    sampleShadingEnable   = false,
    //    rasterizationSamples  = { ._1 },
    //    minSampleShading      = 1.0,
    //    pSampleMask           = nil,
    //    alphaToCoverageEnable = false,
    //    alphaToOneEnable      = false,
    //}
    //color_attachment := vk.PipelineColorBlendAttachmentState {
    //    colorWriteMask = { .R, .G, .B, .A },
    //    blendEnable    = false,
    //}
    //color_blend := vk.PipelineColorBlendStateCreateInfo {
    //    sType           = .PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    //    logicOpEnable   = false,
    //    logicOp         = .COPY,
    //    attachmentCount = 1,
    //    pAttachments    = &color_attachment,
    //}
    //depth_stencil := vk.PipelineDepthStencilStateCreateInfo {
    //    sType                 = .PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    //    depthTestEnable       = false,
    //    depthWriteEnable      = false,
    //    depthCompareOp        = .NEVER,
    //    depthBoundsTestEnable = false,
    //    stencilTestEnable     = false,
    //}
    //render_info := vk.PipelineRenderingCreateInfo {
    //    sType                   = .PIPELINE_RENDERING_CREATE_INFO,
    //    colorAttachmentCount    = 1,
    //    pColorAttachmentFormats = &draw_img.format,
    //}
    //viewport_state := vk.PipelineViewportStateCreateInfo {
    //    sType         = .PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    //    viewportCount = 1,
    //    scissorCount  = 1,
    //}
    //vertex_input := vk.PipelineVertexInputStateCreateInfo {
    //    sType = .PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    //}
    //states := []vk.DynamicState { .VIEWPORT, .SCISSOR }
    //dynamic_sate := vk.PipelineDynamicStateCreateInfo {
    //    sType             = .PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    //    dynamicStateCount = u32(len(states)),
    //    pDynamicStates    = raw_data(states),
    //}
    //graphics_pipe_info := vk.GraphicsPipelineCreateInfo {
    //    sType               = .GRAPHICS_PIPELINE_CREATE_INFO,
    //    pNext               = &render_info,
    //    stageCount          = u32(len(stages)),
    //    pStages             = raw_data(stages),
    //    pVertexInputState   = &vertex_input,
    //    pInputAssemblyState = &input_asm,
    //    pViewportState      = &viewport_state,
    //    pRasterizationState = &rasterizer,
    //    pMultisampleState   = &multisampling,
    //    pDepthStencilState  = &depth_stencil,
    //    pColorBlendState    = &color_blend,
    //    pDynamicState       = &dynamic_sate,
    //    layout              = rect.pipeline_layout,
    //}
    //__ensure(
    //    vk.CreateGraphicsPipelines(device, 0, 1, &graphics_pipe_info, nil, &rect.pipeline),
    //    msg = "Failed to create PBR Pipeline"
    //)
//}

