#pragma once
#include "vulkan.hh"

namespace sigil::renderer {

    //_____________________________________
    struct PipelineBuilder {
        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
        vk::PipelineInputAssemblyStateCreateInfo input_assembly;
        vk::PipelineRasterizationStateCreateInfo rasterizer;
        vk::PipelineColorBlendAttachmentState color_blend_attachment;
        vk::PipelineMultisampleStateCreateInfo multisampling;
        vk::PipelineDepthStencilStateCreateInfo depth_stencil;
        vk::PipelineRenderingCreateInfo render_info;
        vk::Format color_attachment_format;
        vk::PipelineLayout layout;

        PipelineBuilder& set_shaders(vk::ShaderModule vertex, vk::ShaderModule fragment) {
            shader_stages.clear();
            shader_stages = {
                vk::PipelineShaderStageCreateInfo {
                    .stage  = vk::ShaderStageFlagBits::eVertex,
                    .module = vertex,
                    .pName  = "main",
                },
                vk::PipelineShaderStageCreateInfo {
                    .stage  = vk::ShaderStageFlagBits::eFragment,
                    .module = fragment,
                    .pName  = "main",
                }
            };
            return *this;
        }

        PipelineBuilder& set_input_topology(vk::PrimitiveTopology topoloty) {
            input_assembly.topology               = topoloty;
            input_assembly.primitiveRestartEnable = false;
            return *this;
        }

        PipelineBuilder& set_polygon_mode(vk::PolygonMode mode) {
            rasterizer.polygonMode = mode;
            rasterizer.lineWidth   = 1;
            return *this;
        }

        PipelineBuilder& set_cull_mode(vk::CullModeFlagBits cull_mode, vk::FrontFace front_face) {
            rasterizer.cullMode  = cull_mode;
            rasterizer.frontFace = front_face;
            return *this;
        }

        PipelineBuilder& set_multisampling_none() {
            multisampling.sampleShadingEnable   = false;
            multisampling.rasterizationSamples  = vk::SampleCountFlagBits::e1;
            multisampling.minSampleShading      = 1.0f;
            multisampling.pSampleMask           = nullptr;
            multisampling.alphaToCoverageEnable = false;
            multisampling.alphaToOneEnable      = false;
            return *this;
        }

        PipelineBuilder& disable_blending() {
            color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                                  | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            color_blend_attachment.blendEnable    = false;
            return *this;
        }

        PipelineBuilder& enable_blending_addative() {
            color_blend_attachment.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                                       | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            color_blend_attachment.blendEnable         = true;
            color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eOne;
            color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
            color_blend_attachment.colorBlendOp        = vk::BlendOp::eAdd;
            color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
            color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
            color_blend_attachment.alphaBlendOp        = vk::BlendOp::eAdd;
            return *this;
        }

        PipelineBuilder& enable_blending_alphablend() {
            color_blend_attachment.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                                       | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            color_blend_attachment.blendEnable         = true;
            color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eOneMinusDstAlpha;
            color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
            color_blend_attachment.colorBlendOp        = vk::BlendOp::eAdd;
            color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
            color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
            color_blend_attachment.alphaBlendOp        = vk::BlendOp::eAdd;
            return *this;
        }

        PipelineBuilder& set_color_attachment_format(vk::Format format) {
            color_attachment_format             = format;
            render_info.colorAttachmentCount    = 1;
            render_info.pColorAttachmentFormats = &color_attachment_format;
            return *this;
        }

        PipelineBuilder& set_depth_format(vk::Format format) {
            render_info.depthAttachmentFormat = format;
            return *this;
        }

        PipelineBuilder& disable_depthtest() {
            depth_stencil.depthTestEnable       = false;
            depth_stencil.depthWriteEnable      = false;
            depth_stencil.depthCompareOp        = vk::CompareOp::eNever;
            depth_stencil.depthBoundsTestEnable = false;
            depth_stencil.stencilTestEnable     = false;
            return *this;
        }

        PipelineBuilder& enable_depthtest(bool write_enable, vk::CompareOp op) {
            depth_stencil.depthTestEnable       = true;
            depth_stencil.depthWriteEnable      = write_enable;
            depth_stencil.depthCompareOp        = op;
            depth_stencil.depthBoundsTestEnable = false;
            depth_stencil.stencilTestEnable     = false;
            return *this;
        }

        PipelineBuilder& set_layout(vk::PipelineLayout in_layout) {
            layout = in_layout;
            return *this;
        }

        vk::Pipeline build() {
            vk::PipelineViewportStateCreateInfo viewport_state {
                .viewportCount = 1,
                .scissorCount  = 1,
            };

            vk::PipelineColorBlendStateCreateInfo color_blending {
                .logicOpEnable   = false,
                .logicOp         = vk::LogicOp::eCopy,
                .attachmentCount = 1,
                .pAttachments    = &color_blend_attachment,
            };

            vk::PipelineVertexInputStateCreateInfo vertex_input {};

            vk::DynamicState states[] {
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor,
            };
            vk::PipelineDynamicStateCreateInfo dynamic_state {
                .dynamicStateCount = 2,
                .pDynamicStates    = &states[0],
            };

            vk::GraphicsPipelineCreateInfo pipeline_info {
                .pNext               = &render_info,
                .stageCount          = (u32)shader_stages.size(),
                .pStages             = shader_stages.data(),
                .pVertexInputState   = &vertex_input,
                .pInputAssemblyState = &input_assembly,
                .pViewportState      = &viewport_state,
                .pRasterizationState = &rasterizer,
                .pMultisampleState   = &multisampling,
                .pDepthStencilState  = &depth_stencil,
                .pColorBlendState    = &color_blending,
                .pDynamicState       = &dynamic_state,
                .layout              = layout,
            };

            _deletion_queue.push_function([=]{
                device.destroyPipelineLayout(layout);
            });

            return device
                .createGraphicsPipelines(nullptr, pipeline_info)
                .value
                .front();
        }
    };

} // sigil::renderer

