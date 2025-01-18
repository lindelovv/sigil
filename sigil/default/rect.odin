//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
package __sigil_default
import vk "vendor:vulkan"

rect_pipelne     : vk.Pipeline
rect_layout      : vk.PipelineLayout
rect_mesh        : Mesh

//_____________________________
rect := [dynamic]Vertex {
    { position = {  0.5, -0.5, 0 }, color = { 0, 0, 1, 1 } },
    { position = {  0.5,  0.5, 0 }, color = { 1, 0, 1, 1 } },
    { position = { -0.5, -0.5, 0 }, color = { 1, 0, 0, 1 } },
    { position = { -0.5,  0.5, 0 }, color = { 0, 1, 0, 1 } },
}
rect_indices := [dynamic]u32 { 3, 1, 2, 2, 1, 0 }

rect_declare :: proc() {
    // //triangle_pipeline_layout := vk.PipelineLayoutCreateInfo { sType = .PIPELINE_LAYOUT_CREATE_INFO }
    // //check_err(vk.CreatePipelineLayout(device, &triangle_pipeline_layout, nil, &triangle_Layout), "Failed to create triangle pipeline layout")

    // //vertex_shader := create_shader_module("res/shaders/triangle.vert.spv")
    // //defer vk.DestroyShaderModule(device, vertex_shader, nil)

    // //fragment_shader := create_shader_module("res/shaders/triangle.frag.spv")
    // //defer vk.DestroyShaderModule(device, fragment_shader, nil)

    //vertex_shader := create_shader_module("res/shaders/rect.vert.spv")
    //defer vk.DestroyShaderModule(device, vertex_shader, nil)

    //fragment_shader := create_shader_module("res/shaders/rect.frag.spv")
    //defer vk.DestroyShaderModule(device, fragment_shader, nil)

    //stages := []vk.PipelineShaderStageCreateInfo {
    //    vk.PipelineShaderStageCreateInfo {
    //        sType  = .PIPELINE_SHADER_STAGE_CREATE_INFO,
    //        stage  = { .VERTEX },
    //        module = vertex_shader,
    //        pName  = "main"
    //    },
    //    vk.PipelineShaderStageCreateInfo {
    //        sType  = .PIPELINE_SHADER_STAGE_CREATE_INFO,
    //        stage  = { .FRAGMENT },
    //        module = fragment_shader,
    //        pName  = "main"
    //    },
    //}

    //gpu_push_const := vk.PushConstantRange {
    //    stageFlags = { .VERTEX },
    //    offset     = 0,
    //    size       = size_of(GPU_PushConstants),
    //}
    //rect_pipeline_layout := vk.PipelineLayoutCreateInfo {
    //    sType                  = .PIPELINE_LAYOUT_CREATE_INFO,
    //    pushConstantRangeCount = 1,
    //    pPushConstantRanges    = &gpu_push_const,
    //}
    //check_err(vk.CreatePipelineLayout(device, &rect_pipeline_layout, nil, &rect_layout), "Failed to create rect pipeline layout")

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
    //    sType = .PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
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
    //    layout              = rect_layout
    //}
    //check_err(vk.CreateGraphicsPipelines(device, 0, 1, &graphics_pipe_info, nil, &rect_pipelne), "Failed to create PBR Pipeline")
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
