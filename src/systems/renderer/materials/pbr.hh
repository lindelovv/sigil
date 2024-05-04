#pragma once
#include "vulkan.hh"
#include "image.hh"
#include "pipelines.hh"

namespace sigil::renderer {

    struct PbrMetallicRoughness {
        struct MaterialConstants {
            glm::vec4 color_factors;
            glm::vec4 metal_roughness_factors;
            glm::vec4 padding[14];
        };
        struct MaterialResources {
            AllocatedImage color_img;
            vk::Sampler color_sampler;
            AllocatedImage metal_roughness_img;
            vk::Sampler metal_roughness_sampler;
            vk::Buffer data;
            u32 offset;
        };
        MaterialPipeline pipeline;
        vk::DescriptorSetLayout layout;
        DescriptorWriter writer;

        void build_pipelines() {
            vk::ShaderModule vert = load_shader_module("res/shaders/mesh.vert.spv").value;
            vk::ShaderModule frag = load_shader_module("res/shaders/texture.frag.spv").value;

            vk::PushConstantRange matrix_range {
                .stageFlags = vk::ShaderStageFlagBits::eVertex,
                .offset = 0,
                .size = sizeof(GPUDrawPushConstants),
            };

            layout = DescriptorLayoutBuilder {}
                .add_binding(0, vk::DescriptorType::eUniformBuffer)
                .add_binding(1, vk::DescriptorType::eCombinedImageSampler)
                .add_binding(2, vk::DescriptorType::eCombinedImageSampler)
                .build(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

            vk::DescriptorSetLayout layouts[] {
                _scene_data_layout,
                layout,
            };

            vk::PipelineLayoutCreateInfo mesh_layout_info {
                .setLayoutCount = 2,
                .pSetLayouts = layouts,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &matrix_range,
            };

            VK_CHECK(device.createPipelineLayout(&mesh_layout_info, nullptr, &pipeline.layout));

            pipeline.handle = PipelineBuilder {}
                .set_shaders(vert, frag)
                .set_input_topology(vk::PrimitiveTopology::eTriangleList)
                .set_polygon_mode(vk::PolygonMode::eFill)
                .set_cull_mode(vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise)
                .set_multisampling_none()
                .disable_blending()
                .enable_depthtest(true, vk::CompareOp::eGreaterOrEqual)
                .set_color_attachment_format(_draw.img.format)
                .set_depth_format(_depth.img.format)
                .set_layout(pipeline.layout)
                .build();

            device.destroyShaderModule(vert);
            device.destroyShaderModule(frag);

            _deletion_queue.push_function([=]{
                device.destroyPipeline(pipeline.handle);
                device.destroyPipelineLayout(pipeline.layout);
                device.destroyDescriptorSetLayout(layout);
            });
        }

        MaterialInstance write_material(const MaterialResources& resources, DescriptorAllocator& descriptor_allocator) {
            MaterialInstance instance {
                .pipeline = &pipeline,
                .material_set = descriptor_allocator.allocate(layout),
            };
            writer.clear()
                .write_buffer(0, resources.data, sizeof(MaterialConstants), resources.offset, vk::DescriptorType::eUniformBuffer)
                .write_img(1, resources.color_img.img.view, resources.color_sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
                .write_img(2, resources.metal_roughness_img.img.view, resources.metal_roughness_sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
                .update_set(instance.material_set);
            return instance;
        }
    } inline _pbr_metallic_roughness;

    //_____________________________________
    struct PbrMaterial {
        MaterialInstance data;
    };

} // sigil::renderer

