#pragma once

#include "sigil.hh"
#include "glfw.hh"
#include "deletion_queue.hh"

#include <GLFW/glfw3.h>
#include <assimp/mesh.h>
#include <deque>
#include <fstream>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vulkan/vulkan_core.h>

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "vk_mem_alloc.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "assimp/Importer.hpp"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace sigil::renderer {

// TODO: VkResult checker that can return value and result or just value
#define VK_CHECK(x) { vk::Result e = (vk::Result)x; if((VkResult)e) {       \
    std::cout << "__VK_CHECK failed__ "                                      \
    << vk::to_string(e) << " @ " << __FILE_NAME__ << "_" << __LINE__ << "\n"; \
} }
#define SIGIL_V VK_MAKE_VERSION(version::major, version::minor, version::patch)

    const vk::ApplicationInfo engine_info = {
        .pApplicationName                       = "sigil",
        .applicationVersion                     = SIGIL_V,
        .pEngineName                            = "sigil",
        .engineVersion                          = SIGIL_V,
        .apiVersion                             = VK_API_VERSION_1_3,
    };

    const std::vector<const char*> _layers      = { "VK_LAYER_KHRONOS_validation", };
    const std::vector<const char*> _device_exts = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    };

    //_____________________________________
    // DEBUG
    struct {
        vk::DebugUtilsMessengerEXT messenger;

        static VKAPI_ATTR VkBool32 VKAPI_CALL callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
            VkDebugUtilsMessageTypeFlagsEXT msg_type,
            const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
            void* p_user_data
        ) {
            std::cerr << ">> " << p_callback_data->pMessage << "\n"
                      << "---------------------------------------\n";
            return false;
        }
    } inline debug;

    //_____________________________________
    inline vk::Instance         instance;
    inline vk::SurfaceKHR       surface;
    inline vk::PhysicalDevice   phys_device;
    inline vk::Device           device;

    //_____________________________________
    struct {
        vk::SwapchainKHR           handle;
        vk::Format                 img_format;
        std::vector<vk::Image>     images;
        std::vector<vk::ImageView> img_views;
        vk::Extent2D               extent;
    } inline swapchain;

    inline bool request_resize = false;

    //_____________________________________
    struct {
        vk::Queue handle;
        u32       family;
    } inline graphics_queue;

    //_____________________________________
    struct DescriptorAllocator {

        void init(u32 nr_sets, std::span<std::pair<vk::DescriptorType, f32>> pool_ratios) {
            ratios.clear();
            for( auto ratio : pool_ratios ) {
                ratios.push_back(ratio);
            }
            vk::DescriptorPool new_pool = create_pool(nr_sets, pool_ratios);
            sets_per_pool = nr_sets * 1.5f;
            ready_pools.push_back(new_pool);
        }

        vk::DescriptorSet allocate(vk::DescriptorSetLayout layout) {
            vk::DescriptorPool pool = get_pool();
            vk::DescriptorSet set;
            vk::DescriptorSetAllocateInfo allocate_info {
                .descriptorPool = pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &layout,
            };
            vk::Result result = device.allocateDescriptorSets(&allocate_info, &set);
            if( result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool ) {
                full_pools.push_back(pool);
                pool = get_pool();
                allocate_info.descriptorPool = pool;
                VK_CHECK(device.allocateDescriptorSets(&allocate_info, &set));
            }
            ready_pools.push_back(pool);
            return set;
        }

        void clear() {
            for( auto pool : ready_pools ) {
                device.resetDescriptorPool(pool);
            }
            for( auto pool : full_pools ) {
                device.resetDescriptorPool(pool);
                ready_pools.push_back(pool);
            }
            full_pools.clear();
        }

        void destroy() {
            for( auto pool : ready_pools ) {
                device.destroyDescriptorPool(pool);
            }
            ready_pools.clear();
            for( auto pool : full_pools ) {
                device.destroyDescriptorPool(pool);
            }
            full_pools.clear();
        }

    private:

        vk::DescriptorPool get_pool() {
            vk::DescriptorPool new_pool;
            if( ready_pools.size() != 0 ) {
                new_pool = ready_pools.back();
                ready_pools.pop_back();
            }
            else {
                new_pool = create_pool(sets_per_pool, ratios);
                if( sets_per_pool > 4092 ) {
                    sets_per_pool = 4092;
                }
            }
            return new_pool;
        }

        vk::DescriptorPool create_pool(u32 set_count, std::span<std::pair<vk::DescriptorType, f32>> ratios) {
            std::vector<vk::DescriptorPoolSize> pool_sizes;
            for( auto [type, ratio] : ratios ) {
                pool_sizes.push_back(
                    vk::DescriptorPoolSize {
                        .type = type,
                        .descriptorCount = (u32)ratio * set_count,
                    }
                );
            }
            return device.createDescriptorPool(
                    vk::DescriptorPoolCreateInfo {
                        .flags = vk::DescriptorPoolCreateFlags(),
                        .maxSets = set_count,
                        .poolSizeCount = (u32)pool_sizes.size(),
                        .pPoolSizes = pool_sizes.data(),
                    },
                    nullptr
                ).value;
        }

        std::vector<std::pair<vk::DescriptorType, f32>> ratios;
        std::vector<vk::DescriptorPool> full_pools;
        std::vector<vk::DescriptorPool> ready_pools;
        u32 sets_per_pool;
    } inline _descriptor_allocator;

    struct DescriptorWriter {

        std::deque<vk::DescriptorImageInfo> img_info;
        std::deque<vk::DescriptorBufferInfo> buffer_info;
        std::vector<vk::WriteDescriptorSet> writes;

        DescriptorWriter& write_img(
            u32 binding,
            vk::ImageView img_view,
            vk::Sampler sampler,
            vk::ImageLayout layout,
            vk::DescriptorType type
        ) {
            vk::DescriptorImageInfo& info = img_info.emplace_back(
                vk::DescriptorImageInfo {
                    .sampler = sampler,
                    .imageView = img_view,
                    .imageLayout = layout,
                }
            );
            writes.push_back(
                vk::WriteDescriptorSet {
                    .dstSet = VK_NULL_HANDLE,
                    .dstBinding = binding,
                    .descriptorCount = 1,
                    .descriptorType = type,
                    .pImageInfo = &info,
                }
            );
            return *this;
        }

        DescriptorWriter& write_buffer(
            u32 binding,
            vk::Buffer buffer,
            size_t size,
            size_t offset,
            vk::DescriptorType type
        ) {
            vk::DescriptorBufferInfo& info = buffer_info.emplace_back(
                vk::DescriptorBufferInfo {
                    .buffer = buffer,
                    .offset = offset,
                    .range  = size,
                }
            );
            writes.push_back(
                vk::WriteDescriptorSet {
                    .dstSet = VK_NULL_HANDLE,
                    .dstBinding = binding,
                    .descriptorCount = 1,
                    .descriptorType = type,
                    .pBufferInfo = &info,
                }
            );
            return *this;
        }

        DescriptorWriter& update_set(vk::DescriptorSet set) {
            for( vk::WriteDescriptorSet& write : writes ) {
                write.dstSet = set;
            }
            device.updateDescriptorSets((u32)writes.size(), writes.data(), 0, nullptr);
            return *this;
        }

        DescriptorWriter& clear() {
            img_info.clear();
            buffer_info.clear();
            writes.clear();
            return *this;
        }
        
    } inline _descriptor_writer;

    struct DescriptorData {
        vk::DescriptorPool pool;
        struct {
            vk::DescriptorSet handle;
            vk::DescriptorSetLayout layout;
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
        } set;
    };

    struct DescriptorLayoutBuilder {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;

        DescriptorLayoutBuilder& add_binding(u32 binding, vk::DescriptorType type) {
            bindings.push_back(
                vk::DescriptorSetLayoutBinding {
                    .binding = binding,
                    .descriptorType = type,
                    .descriptorCount = 1,
                }
            );
            return *this;
        }

        DescriptorLayoutBuilder& clear() { bindings.clear(); return *this; }

        vk::DescriptorSetLayout build(vk::ShaderStageFlags shader_stages) {
            for( auto& bind : bindings ) {
                bind.stageFlags |= shader_stages;
            }
            vk::DescriptorSetLayoutCreateInfo info {
                .flags = vk::DescriptorSetLayoutCreateFlags(),
                .bindingCount = (u32)bindings.size(),
                .pBindings = bindings.data(),
            };
            vk::DescriptorSetLayout set;
            VK_CHECK(device.createDescriptorSetLayout(&info, nullptr, &set));
            return set;
        }
    };

    //_____________________________________
    struct AllocatedImage {
        struct {
            vk::Image       handle;
            vk::ImageView   view;
            vma::Allocation alloc;
            vk::Extent3D    extent;
            vk::Format      format;
        } img;
        vk::Extent2D extent;
        DescriptorData descriptor;
    };
    inline AllocatedImage _draw;
    inline AllocatedImage _depth;
    inline AllocatedImage _error_img;
    inline AllocatedImage _white_img;
    inline vk::Sampler _sampler_linear;
    inline vk::Sampler _sampler_nearest;

    inline vk::DescriptorSetLayout _single_img_layout;

    //_____________________________________
    static vma::Allocator vma_allocator;

    //_____________________________________
    constexpr u8 FRAME_OVERLAP = 2;
    struct FrameData {
        struct {
            vk::CommandPool   pool;
            vk::CommandBuffer buffer;
        } cmd;
        vk::Fence     fence;
        vk::Semaphore render_semaphore;
        vk::Semaphore swap_semaphore;
        DescriptorAllocator descriptors;
        DeletionQueue deletion_queue;
    };
    inline std::vector<FrameData> frames { FRAME_OVERLAP };
    inline u32 current_frame = 0;

    inline DeletionQueue _deletion_queue;

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
            // Add optional extras if not actually optional
            return *this;
        }

        PipelineBuilder& enable_depthtest(bool write_enable, vk::CompareOp op) {
            depth_stencil.depthTestEnable       = true;
            depth_stencil.depthWriteEnable      = write_enable;
            depth_stencil.depthCompareOp        = op;
            depth_stencil.depthBoundsTestEnable = false;
            depth_stencil.stencilTestEnable     = false;
            // Add optional extras if not actually optional
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

            return device
                .createGraphicsPipelines(nullptr, pipeline_info)
                .value
                .front();
        }
    };

    //_____________________________________
    struct {
        vk::Pipeline handle;
        vk::PipelineLayout layout;
    } inline compute_pipeline;

    struct {
        vk::Pipeline handle;
        u32 index;
        vk::PipelineLayout layout;
        vk::RenderPass pass;
        u32 subpass;
        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
    } inline graphics_pipeline;

    //_____________________________________
    struct {
        vk::Fence         fence;
        vk::CommandBuffer cmd;
        vk::CommandPool   pool;
    } inline immediate;

    //_____________________________________
    struct ComputePushConstants {
        glm::vec4 data1;
        glm::vec4 data2;
        glm::vec4 data3;
        glm::vec4 data4;
    };

    //_____________________________________
    struct ComputeEffect {
        const char* name;
        vk::Pipeline pipeline;
        vk::PipelineLayout pipeline_layout;
        ComputePushConstants data;
    };
    inline std::vector<ComputeEffect> bg_effects;
    inline int current_bg_effect { 0 };

    struct AllocatedBuffer {
        vk::Buffer handle;
        vma::Allocation allocation;
        vma::AllocationInfo info;
    };

    //_____________________________________
    struct Vertex {
        glm::vec3 position;
        float uv_x;
        glm::vec3 normal;
        float uv_y;
        glm::vec4 color;
    };

    struct GPUMeshBuffer {
        AllocatedBuffer index_buffer;
        AllocatedBuffer vertex_buffer;
        vk::DeviceAddress vertex_buffer_address;
    };

    struct GPUDrawPushConstants {
        glm::mat4 world_matrix;
        vk::DeviceAddress vertex_buffer;
    };

    //_____________________________________
    struct MaterialPipeline {
        vk::Pipeline       handle;
        vk::PipelineLayout layout;
    };

    struct MaterialInstance {
        MaterialPipeline* pipeline;
        vk::DescriptorSet material_set;
    } inline _default_material_instance;

    struct RenderObject {
        u32 count;
        u32 first;
        vk::Buffer index_buffer;
        MaterialInstance* material;
        glm::mat4         transform;
        vk::DeviceAddress address;
    };

    //_____________________________________
    inline vk::ResultValue<vk::ShaderModule> load_shader_module(const char* path) {
        std::cout << ">> Loading file at: " << path << "\n";

        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if( !file.is_open() ) {
            throw std::runtime_error("\tError: Failed to open file at:" + std::string(path));
        }
        size_t size = file.tellg();
        std::vector<char> buffer(size);
        file.seekg(0);
        file.read((char*)buffer.data(), size);
        file.close();
        return device.createShaderModule(vk::ShaderModuleCreateInfo {
            .codeSize = buffer.size(),
            .pCode = (const u32*)buffer.data(),
        });
    }

    //_____________________________________
    struct GPUSceneData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 view_proj;
        glm::vec4 ambient_color;
        glm::vec4 sun_dir;
        glm::vec4 sun_color;
    } inline _scene_data;
    inline vk::DescriptorSetLayout _scene_data_layout;

    //_____________________________________
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
        }

        MaterialInstance write_material(const MaterialResources& resources, DescriptorAllocator& descriptor_allocator) {
            MaterialInstance instance {
                .pipeline = &pipeline,
                .material_set = descriptor_allocator.allocate(layout),
            };
            writer.clear()
                .write_buffer(0, resources.data, sizeof(MaterialConstants), resources.offset, vk::DescriptorType::eUniformBuffer)
                .write_img(1, resources.color_img.img.view, resources.color_sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
                .write_img(1, resources.metal_roughness_img.img.view, resources.metal_roughness_sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
                .update_set(instance.material_set);
            return instance;
        }
    } inline _pbr_metallic_roughness;

    //_____________________________________
    struct PbrMaterial {
        MaterialInstance data;
    };

    struct GeoSurface {
        u32 start_index;
        u32 count;
        std::shared_ptr<PbrMaterial> material;
    };

    struct Mesh {
        size_t id;
        std::vector<GeoSurface> surfaces;
        GPUMeshBuffer mesh_buffer;
    };

    inline std::vector<Mesh> _meshes;
    inline std::vector<RenderObject> _render_objects;

    // Camera
    //_____________________________________
    inline static glm::vec3 world_up = glm::vec3(0.f, 0.f, -1.f);

    struct {
        struct {
            glm::vec3 position = glm::vec3( 1, 0, 0 );
            glm::vec3 rotation = glm::vec3( 0, 0, 0 );
            glm::vec3 scale    = glm::vec3( 0 );
        } transform;
        float fov =  70.f;
        float yaw = 180.f;
        float pitch = -13.f;
        glm::vec3 velocity;
        glm::vec3 forward_vector = glm::normalize(glm::vec3(
                    cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(pitch))
                ));
        glm::vec3 up_vector      = -world_up;
        glm::vec3 right_vector   = glm::cross(forward_vector, world_up);
        struct {
            float near =  0.01f;
            float far  = 256.0f;
        } clip_plane;
        struct {
            bool forward;
            bool right;
            bool back; 
            bool left; 
            bool up; 
            bool down; 
        } request_movement;
        float movement_speed = 1.f;
        bool follow_mouse = false;
        float mouse_sens = 24.f;

        inline void update(float delta_time) {
            if( request_movement.forward ) velocity += forward_vector;
            if( request_movement.back    ) velocity -= forward_vector;
            if( request_movement.right   ) velocity -= right_vector;
            if( request_movement.left    ) velocity += right_vector;
            if( request_movement.up      ) velocity -= up_vector;
            if( request_movement.down    ) velocity += up_vector;

            transform.position += velocity * movement_speed * delta_time;
            velocity = glm::vec3(0);
            if( follow_mouse ) {
                glm::dvec2 offset = -input::get_mouse_movement() * glm::dvec2(mouse_sens) * glm::dvec2(delta_time);
                yaw += offset.x;
                pitch = ( pitch - offset.y >  89.f ?  89.f
                        : pitch - offset.y < -89.f ? -89.f
                        : pitch - offset.y);
                forward_vector = glm::normalize(glm::vec3(
                    cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(pitch))
                ));
                right_vector = glm::cross(forward_vector, world_up);
                up_vector = glm::cross(forward_vector, right_vector);
                //transform.rotation = glm::qrot(transform.rotation, forward_vector);
            }
        }

        inline glm::mat4 get_view() {
            return glm::lookAt(
                transform.position,
                transform.position + forward_vector,
                up_vector
            );
        }
    } inline camera;


    namespace primitives {

        struct Plane {
            std::vector<Vertex> vertices;
            std::vector<u32> indices;
            GeoSurface surface;

            Plane() : Plane(10, 10) {};

            Plane(u32 div, f32 width) {
                f32 triangle_side = width / div;
                for( u32 row = 0; row < div + 1; row++ ) {
                    for( u32 col = 0; col < div + 1; col++ ) {
                        vertices.push_back(
                            Vertex { .position = glm::vec3(col * triangle_side, row * -triangle_side, 0) }
                        );
                    }
                }
                surface.start_index = (u32)indices.size();
                for( u32 row = 0; row < div; row++ ) {
                    for( u32 col = 0; col < div; col++ ) {
                        u32 index = row * (div + 1) + col;
                        indices.push_back(index);
                        indices.push_back(index + (div + 1) + 1);
                        indices.push_back(index + (div + 1));
                        indices.push_back(index);
                        indices.push_back(index + 1);
                        indices.push_back(index + (div + 1) + 1);
                    }
                }
                surface.count = (u32)indices.size();
            }
            
            //rect_vertices = {
            //    Vertex {
            //        .position = { .5, -.5, 0 },
            //        .color    = { .5, .5, .5, 1 },
            //    },
            //    Vertex {
            //        .position = { .5, .5, 0 },
            //        .color    = { .5, .5, .5, 1 },
            //    },
            //    Vertex {
            //        .position = { -.5, -.5, 0 },
            //        .color    = { .5, .5, .5, 1 },
            //    },
            //    Vertex {
            //        .position = { -.5, .5, 0 },
            //        .color    = { .5, .5, .5, 1 },
            //    },
            //};
            //rect_indices = {
            //    0, 1, 2,
            //    2, 1, 3,
            //};
        };
    } // primitives

    //_____________________________________
    namespace ui {

        inline void draw(vk::CommandBuffer cmd, vk::ImageView img_view) {

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Sigil", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(fmt::format("GPU: {}", phys_device.getProperties().deviceName.data()).c_str());
                ImGui::TextUnformatted(fmt::format("sigil   {}", sigil::version::as_string).c_str());
                ImGui::SetWindowPos(ImVec2(0, swapchain.extent.height - ImGui::GetWindowSize().y));
            }
            ImGui::End();

            ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoTitleBar   //| ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(
                    fmt::format(" Camera position:\n\tx: {:.3f}\n\ty: {:.3f}\n\tz: {:.3f}",
                    camera.transform.position.x, camera.transform.position.y, camera.transform.position.z).c_str()
                );
                ImGui::TextUnformatted(
                    fmt::format(" Yaw:   {:.2f}\n Pitch: {:.2f}",
                    camera.yaw, camera.pitch).c_str()
                );
                ImGui::TextUnformatted(
                    fmt::format(" Mouse position:\n\tx: {:.0f}\n\ty: {:.0f}",
                    input::mouse_position.x, input::mouse_position.y ).c_str()
                );
                ImGui::TextUnformatted(
                    fmt::format(" Mouse offset:\n\tx: {:.0f}\n\ty: {:.0f}",
                    input::get_mouse_movement().x, input::get_mouse_movement().y).c_str()
                );

                ImGui::SetWindowSize(ImVec2(108, 174));
                ImGui::SetWindowPos(ImVec2(8, 8));
            }
            ImGui::End();

            ImGui::Begin("Framerate", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(fmt::format(" FPS: {:.0f}", time::fps).c_str());
                ImGui::TextUnformatted(fmt::format(" ms: {:.2f}", time::ms).c_str());
                ImGui::SetWindowSize(ImVec2(82, 64));
                ImGui::SetWindowPos(ImVec2(swapchain.extent.width - ImGui::GetWindowSize().x, 8));
            }
            ImGui::End();
            ImGui::Render();

            vk::RenderingAttachmentInfo attach_info {
                .imageView = img_view,
                .imageLayout = vk::ImageLayout::eGeneral,
                .loadOp = vk::AttachmentLoadOp::eLoad,
                .storeOp = vk::AttachmentStoreOp::eStore,
            };
            vk::RenderingInfo render_info {
                .renderArea = vk::Rect2D { vk::Offset2D { 0, 0 }, swapchain.extent },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &attach_info,
                .pDepthAttachment = nullptr,
                .pStencilAttachment = nullptr,
            };
            cmd.beginRendering(&render_info);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
            cmd.endRendering();
        }
    } // ui

    //_____________________________________
    inline void build_swapchain() {
        int w, h;
        glfwGetFramebufferSize(window::handle, &w, &h);

        auto capabilities = phys_device.getSurfaceCapabilitiesKHR(surface).value;
        swapchain.handle = device.createSwapchainKHR(
            vk::SwapchainCreateInfoKHR {
                .surface            = surface,
                .minImageCount      = capabilities.minImageCount + 1,
                .imageFormat        = (swapchain.img_format = vk::Format::eB8G8R8A8Unorm),
                .imageColorSpace    = vk::ColorSpaceKHR::eSrgbNonlinear,
                .imageExtent        = { .width = (u32)w, .height = (u32)h },
                .imageArrayLayers   = 1,
                .imageUsage         = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment,
                .preTransform       = capabilities.currentTransform,
                .compositeAlpha     = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                .presentMode        = vk::PresentModeKHR::eMailbox,
                .clipped            = true,
        }   ).value;

        swapchain.extent = vk::Extent2D { .width = (u32)w, .height = (u32)h };
        swapchain.images = device.getSwapchainImagesKHR(swapchain.handle).value;
        for( auto image : swapchain.images ) {
            swapchain.img_views.push_back(
                device.createImageView(
                    vk::ImageViewCreateInfo {
                        .image      = image,
                        .viewType   = vk::ImageViewType::e2D,
                        .format     = swapchain.img_format,
                        .subresourceRange {
                            .aspectMask     = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel   = 0,
                            .levelCount     = 1,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
            },  }   ).value );
        }
        _draw.img = {
            .extent = vk::Extent3D { .width = (u32)w, .height = (u32)h, .depth = 1 },
            .format = vk::Format::eR16G16B16A16Sfloat,
        };
        std::tie( _draw.img.handle, _draw.img.alloc ) = vma_allocator.createImage(
            vk::ImageCreateInfo {
                .imageType = vk::ImageType::e2D,
                .format = _draw.img.format,
                .extent = _draw.img.extent,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eTransferSrc
                       | vk::ImageUsageFlagBits::eTransferDst
                       | vk::ImageUsageFlagBits::eStorage
                       | vk::ImageUsageFlagBits::eColorAttachment,
            },
            vma::AllocationCreateInfo {
                .usage = vma::MemoryUsage::eGpuOnly,
                .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
            }
        ).value;
        _draw.img.view = device.createImageView(
            vk::ImageViewCreateInfo {
                .image      = _draw.img.handle,
                .viewType   = vk::ImageViewType::e2D,
                .format     = _draw.img.format,
                .subresourceRange {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
        },  }   ).value;

        _depth.img = {
            .extent = _draw.img.extent,
            .format = vk::Format::eD32Sfloat,
        };
        std::tie( _depth.img.handle, _depth.img.alloc ) = vma_allocator.createImage(
            vk::ImageCreateInfo {
                .imageType = vk::ImageType::e2D,
                .format = _depth.img.format,
                .extent = _depth.img.extent,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            },
            vma::AllocationCreateInfo {
                .usage = vma::MemoryUsage::eGpuOnly,
                .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
            }
        ).value;
        _depth.img.view = device.createImageView(
            vk::ImageViewCreateInfo {
                .image      = _depth.img.handle,
                .viewType   = vk::ImageViewType::e2D,
                .format     = _depth.img.format,
                .subresourceRange {
                    .aspectMask     = vk::ImageAspectFlagBits::eDepth,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
        },  }   ).value;
    }

    //_____________________________________
    inline void rebuild_swapchain() {
        VK_CHECK(graphics_queue.handle.waitIdle());

        device.destroySwapchainKHR(swapchain.handle);
        //for( auto img : swapchain.images ) {
        //    device.destroyImage(img);
        //}
        for( auto img_view : swapchain.img_views ) {
            device.destroyImageView(img_view);
        }
        swapchain.img_views.clear();
        swapchain.images.clear();

        int w, h;
        glfwGetFramebufferSize(window::handle, &w, &h);

        auto capabilities = phys_device.getSurfaceCapabilitiesKHR(surface).value;
        swapchain.handle = device.createSwapchainKHR(
            vk::SwapchainCreateInfoKHR {
                .surface            = surface,
                .minImageCount      = capabilities.minImageCount + 1,
                .imageFormat        = (swapchain.img_format = vk::Format::eB8G8R8A8Unorm),
                .imageColorSpace    = vk::ColorSpaceKHR::eSrgbNonlinear,
                .imageExtent        = { .width = (u32)w, .height = (u32)h },
                .imageArrayLayers   = 1,
                .imageUsage         = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment,
                .preTransform       = capabilities.currentTransform,
                .compositeAlpha     = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                .presentMode        = vk::PresentModeKHR::eMailbox,
                .clipped            = true,
        }   ).value;

        swapchain.extent = vk::Extent2D { .width = (u32)w, .height = (u32)h };
        swapchain.images = device.getSwapchainImagesKHR(swapchain.handle).value;
        for( auto image : swapchain.images ) {
            swapchain.img_views.push_back(
                device.createImageView(
                    vk::ImageViewCreateInfo {
                        .image      = image,
                        .viewType   = vk::ImageViewType::e2D,
                        .format     = swapchain.img_format,
                        .subresourceRange {
                            .aspectMask     = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel   = 0,
                            .levelCount     = 1,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
            },  }   ).value );
        }
        request_resize = false;
    }

    //_____________________________________
    inline u32 get_queue_family(vk::QueueFlagBits type) {
        switch( type ) {
            case vk::QueueFlagBits::eGraphics: {
                for( u32 i = 0; auto family : phys_device.getQueueFamilyProperties() ) {
                    if( (family.queueFlags & type) == type ) { return i; }
                    i++;
                }
                break;
            }
            case vk::QueueFlagBits::eCompute:  { /* TODO */ }
            case vk::QueueFlagBits::eTransfer: { /* TODO */ }
            default: { break; }
        }
        return 0;
    }

    //_____________________________________
    inline void build_command_structures() {
        auto cmd_pool_info = vk::CommandPoolCreateInfo {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = graphics_queue.family,
        };
        for( u32 i = 0; i < FRAME_OVERLAP; i++ ) {
            VK_CHECK(device.createCommandPool(&cmd_pool_info, nullptr, &frames.at(i).cmd.pool));
            vk::CommandBufferAllocateInfo cmd_buffer_info {
                .commandPool        = frames.at(i).cmd.pool,
                .level              = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            VK_CHECK(device.allocateCommandBuffers(
                &cmd_buffer_info,
                &frames.at(i).cmd.buffer
            ));
        }
        VK_CHECK(device.createCommandPool(&cmd_pool_info, nullptr, &immediate.pool));
        vk::CommandBufferAllocateInfo cmd_buffer_info {
            .commandPool        = immediate.pool,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };
        VK_CHECK(device.allocateCommandBuffers(&cmd_buffer_info, &immediate.cmd));
    }

    //_____________________________________
    inline void build_sync_structures() {
        auto fence_info = vk::FenceCreateInfo { .flags = vk::FenceCreateFlagBits::eSignaled, };
        auto semaphore_info = vk::SemaphoreCreateInfo { .flags = vk::SemaphoreCreateFlagBits(), };

        for( u32 i = 0; i < FRAME_OVERLAP; i++ ) {
            VK_CHECK(device.createFence(&fence_info, nullptr, &frames.at(i).fence));

            VK_CHECK(device.createSemaphore(&semaphore_info, nullptr, &frames.at(i).swap_semaphore  ));
            VK_CHECK(device.createSemaphore(&semaphore_info, nullptr, &frames.at(i).render_semaphore));
        }
        VK_CHECK(device.createFence(&fence_info, nullptr, &immediate.fence));
    }

    //_____________________________________
    inline void build_descriptors() {
        std::vector<std::pair<vk::DescriptorType, f32>> sizes {
            { vk::DescriptorType::eStorageImage,  1 },
            { vk::DescriptorType::eUniformBuffer, 1 },
        };
        _descriptor_allocator.init(10, sizes);

        {
            _draw.descriptor.set.layout = DescriptorLayoutBuilder {}
                .add_binding(0, vk::DescriptorType::eStorageImage)
                .build(vk::ShaderStageFlagBits::eCompute);
        }
        {
            _single_img_layout = DescriptorLayoutBuilder {}
                .add_binding(0, vk::DescriptorType::eCombinedImageSampler)
                .build(vk::ShaderStageFlagBits::eFragment);
        }
        {
            _scene_data_layout = DescriptorLayoutBuilder {}
                .add_binding(0, vk::DescriptorType::eUniformBuffer)
                .build(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
        }

        _draw.descriptor.set.handle = _descriptor_allocator.allocate(_draw.descriptor.set.layout);

        _descriptor_writer
            .write_img(0, _draw.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
            .update_set(_draw.descriptor.set.handle);

        for( u32 i = 0; i < FRAME_OVERLAP; i++ ) {
            std::vector<std::pair<vk::DescriptorType, f32>> frame_sizes = {
                std::pair( vk::DescriptorType::eStorageImage,         3 ),
                std::pair( vk::DescriptorType::eStorageBuffer,        3 ),
                std::pair( vk::DescriptorType::eUniformBuffer,        3 ),
                std::pair( vk::DescriptorType::eCombinedImageSampler, 4 ),
            };
            frames[i].descriptors.init(1000, frame_sizes);
        }


    }

    //_____________________________________
    inline void immediate_submit(fn<void(vk::CommandBuffer cmd)>&& fn) {
        VK_CHECK(device.resetFences(1, &immediate.fence));
        VK_CHECK(immediate.cmd.reset());
        VK_CHECK(immediate.cmd.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, }));
        {
            fn(immediate.cmd);
        }
        VK_CHECK(immediate.cmd.end());
        vk::CommandBufferSubmitInfo cmd_info {
            .commandBuffer = immediate.cmd,
            .deviceMask    = 0,
        };
        vk::SubmitInfo2 submit {
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmd_info,
        };
        VK_CHECK(graphics_queue.handle.submit2(1, &submit, immediate.fence));
        VK_CHECK(device.waitForFences(immediate.fence, true, UINT64_MAX));
    }

    //_____________________________________
    inline void transition_img(vk::CommandBuffer cmd_buffer, vk::Image img, vk::ImageLayout from, vk::ImageLayout to) {

        vk::ImageMemoryBarrier2 img_barrier {
            .srcStageMask     = vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask    = vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask     = vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask    = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            .oldLayout        = from,
            .newLayout        = to,
            .image            = img,
            .subresourceRange = vk::ImageSubresourceRange {
                .aspectMask         = (to == vk::ImageLayout::eDepthAttachmentOptimal) 
                                        ? vk::ImageAspectFlagBits::eDepth 
                                        : vk::ImageAspectFlagBits::eColor,
                .baseMipLevel       = 0,
                .levelCount         = vk::RemainingMipLevels,
                .baseArrayLayer     = 0,
                .layerCount         = vk::RemainingArrayLayers,
            },
        };

        cmd_buffer.pipelineBarrier2(
            vk::DependencyInfo {
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &img_barrier,
            }
        );
    }

    //_____________________________________
    inline AllocatedBuffer create_buffer(size_t size, vk::BufferUsageFlags usage, vma::MemoryUsage mem_usage) {
        vk::BufferCreateInfo buffer_info {
            .size = size,
            .usage = usage,
        };
        vma::AllocationCreateInfo alloc_info {
            .flags = vma::AllocationCreateFlagBits::eMapped,
            .usage = mem_usage,
        };
        AllocatedBuffer buffer;
        VK_CHECK(vma_allocator.createBuffer(&buffer_info, &alloc_info, &buffer.handle, &buffer.allocation, &buffer.info));
        return buffer;
    }

    //_____________________________________
    inline void destroy_buffer(const AllocatedBuffer& buffer) {
        vma_allocator.destroyBuffer(buffer.handle, buffer.allocation);
    }

    //_____________________________________
    inline AllocatedImage create_img(
        vk::Extent3D extent,
        vk::Format format,
        vk::ImageUsageFlags usage,
        bool mipmapped = false
    ) {

        AllocatedImage alloc {
            .img {
                .extent = extent,
                .format = format,
            },
        };

        vk::ImageCreateInfo img_info {
            .imageType   = vk::ImageType::e2D,
            .format      = format,
            .extent      = extent,
            .mipLevels   = (mipmapped) 
                             ? (u32)std::floor(std::log2(std::max(extent.width, extent.height))) + 1
                             : 1,
            .arrayLayers = 1,
            .samples     = vk::SampleCountFlagBits::e1,
            .tiling      = vk::ImageTiling::eOptimal,
            .usage       = usage,
        };

        vma::AllocationCreateInfo alloc_info {
            .usage         = vma::MemoryUsage::eGpuOnly,
            .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
        };
        VK_CHECK(vma_allocator.createImage(&img_info, &alloc_info, &alloc.img.handle, &alloc.img.alloc, nullptr));

        vk::ImageViewCreateInfo view_info {
            .image      = alloc.img.handle,
            .viewType   = vk::ImageViewType::e2D,
            .format     = alloc.img.format,
            .subresourceRange {
                .aspectMask     = (format == vk::Format::eD32Sfloat)
                                    ? vk::ImageAspectFlagBits::eDepth
                                    : vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        VK_CHECK(device.createImageView(&view_info, nullptr, &alloc.img.view));
        return alloc;
    }

    //_____________________________________
    inline AllocatedImage create_img(
        void* data,
        vk::Extent3D extent,
        vk::Format format,
        vk::ImageUsageFlags usage,
        bool mipmapped = false
    ) {
        size_t data_size = extent.depth * extent.width * extent.height * 4;
        AllocatedBuffer upload_buffer = create_buffer(data_size, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuToGpu);
        memcpy(upload_buffer.info.pMappedData, data, data_size);

        AllocatedImage img = create_img(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipmapped);

        immediate_submit([&](vk::CommandBuffer cmd) {
            transition_img(cmd, img.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            vk::BufferImageCopy copy {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .imageExtent = extent,
            };
            cmd.copyBufferToImage(upload_buffer.handle, img.img.handle, vk::ImageLayout::eTransferDstOptimal, 1, &copy);
            transition_img(cmd, img.img.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        });
        destroy_buffer(upload_buffer);
        return img;
    }

    inline void copy_img_to_img(vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D src_extent, vk::Extent2D dst_extent) {

        vk::ImageBlit2 blit_region {
            .srcSubresource = vk::ImageSubresourceLayers {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
            .srcOffsets = {{
                vk::Offset3D { 0, 0, 0 },
                vk::Offset3D { (int)src_extent.width, (int)src_extent.height, 1 },
            }},
            .dstSubresource = vk::ImageSubresourceLayers {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
            .dstOffsets = {{
                vk::Offset3D { 0, 0, 0 },
                vk::Offset3D { (int)dst_extent.width, (int)dst_extent.height, 1 },
            }},
        };

        cmd.blitImage2(
            vk::BlitImageInfo2 {
                .srcImage = src,
                .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
                .dstImage = dst,
                .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                .regionCount = 1,
                .pRegions = &blit_region,
                .filter = vk::Filter::eLinear,
            }
        );
    }

    //_____________________________________
    inline void destroy_img(const AllocatedImage& img) {
        device.destroyImageView(img.img.view);
        device.destroyImage(img.img.handle);
    }

    //_____________________________________
    inline GPUMeshBuffer upload_mesh(std::span<u32> indices, std::span<Vertex> vertices) {
        const size_t vertex_buffer_size = vertices.size() * sizeof(Vertex);
        const size_t index_buffer_size = indices.size() * sizeof(u32);

        auto vertex_buffer = create_buffer(vertex_buffer_size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eGpuOnly);

        GPUMeshBuffer mesh_buffer {
            .index_buffer = create_buffer(index_buffer_size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vma::MemoryUsage::eGpuOnly),
            .vertex_buffer = vertex_buffer,
            .vertex_buffer_address = device.getBufferAddress(
                vk::BufferDeviceAddressInfo {
                    .buffer = vertex_buffer.handle,
                }
            ),
        };
        AllocatedBuffer staging_buffer = create_buffer(vertex_buffer_size + index_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuOnly);
        void* data = staging_buffer.info.pMappedData;
        memcpy(data, vertices.data(), vertex_buffer_size);
        memcpy((char*)data + vertex_buffer_size, indices.data(), index_buffer_size);
        immediate_submit([&](vk::CommandBuffer cmd) {
            vk::BufferCopy vertex_copy {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertex_buffer_size,
            };
            cmd.copyBuffer(staging_buffer.handle, mesh_buffer.vertex_buffer.handle, 1, &vertex_copy);
            vk::BufferCopy index_copy {
                .srcOffset = vertex_buffer_size,
                .dstOffset = 0,
                .size = index_buffer_size,
            };
            cmd.copyBuffer(staging_buffer.handle, mesh_buffer.index_buffer.handle, 1, &index_copy);
        });
        destroy_buffer(staging_buffer);
        return mesh_buffer;
    }

    //_____________________________________
    inline std::optional<std::vector<std::shared_ptr<Mesh>>> load_model(const char* path) {
        std::cout << ">> Loading file at: " << path << "\n";

        std::vector<std::shared_ptr<Mesh>> new_meshes;
        Assimp::Importer importer;
        if( auto file = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs) ) {
            std::vector<u32> indices;
            std::vector<Vertex> vertices;

            if( file->HasMeshes() ) {
                for( uint32_t i = 0; i < file->mNumMeshes; i++ ) {
                    Mesh mesh {
                        .id = _meshes.size(),
                    };
                    indices.clear();
                    vertices.clear();

                    for( u32 j = 0; j < file->mNumMeshes; j++ ) {
                        aiMesh* submesh = file->mMeshes[j];
                        GeoSurface surface;

                        surface.start_index = (u32)indices.size();
                        for( u32 f = 0; f < submesh->mNumFaces; f++ ) {
                            aiFace& face = submesh->mFaces[f];
                            assert(face.mNumIndices == 3);
                            indices.push_back(face.mIndices[0]);
                            indices.push_back(face.mIndices[1]);
                            indices.push_back(face.mIndices[2]);
                        }
                        surface.count = (u32)indices.size();

                        //std::cout << submesh->GetNumColorChannels() << "\n";
                        for( uint32_t j = 0; j <= submesh->mNumVertices; j++ ) {
                            vertices.push_back(Vertex {
                                .position = {
                                    submesh->mVertices[j].x,
                                    submesh->mVertices[j].y,
                                    submesh->mVertices[j].z,
                                },
                                .uv_x = submesh->mTextureCoords[0][j].x,
                                .normal = {
                                    submesh->mNormals[j].x,
                                    submesh->mNormals[j].y,
                                    submesh->mNormals[j].z,
                                },
                                .uv_y = submesh->mTextureCoords[0][j].y,
                                .color = {
                                    1.f,//submesh->mColors[0][j]->r,
                                    1.f,//submesh->mColors[0][j]->g,
                                    1.f,//submesh->mColors[0][j]->b,
                                    1.f,//submesh->mColors[0][j]->a,
                                },
                            });
                        }
                        mesh.surfaces.push_back(surface);
                    }
                    mesh.mesh_buffer = upload_mesh(indices, vertices);
                    new_meshes.push_back(std::make_shared<Mesh>(mesh));
                }
            }
            else {
                std::cout << "Error:\n>>\tNo models found in file.\n";
            }
        } else {
            std::cout << "Error:\n>>\tFailed to load model.\n";
            throw importer.GetException();
        }
        return new_meshes;
    }

    //_____________________________________
    inline void build_compute_pipeline() {

        vk::PushConstantRange push_const {
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset = 0,
            .size = sizeof(ComputePushConstants),
        };

        compute_pipeline.layout = device.createPipelineLayout(
                vk::PipelineLayoutCreateInfo {
                    .setLayoutCount = 1,
                    .pSetLayouts = &_draw.descriptor.set.layout,
                    .pushConstantRangeCount = 1,
                    .pPushConstantRanges = &push_const,
                }
        ).value;

        vk::ShaderModule sky = load_shader_module("res/shaders/gradient.comp.spv").value;

        vk::PipelineShaderStageCreateInfo stage_info {
            .stage  = vk::ShaderStageFlagBits::eCompute,
            .module = sky,
            .pName  = "main",
        };
        vk::ComputePipelineCreateInfo pipe_info {
            .stage = stage_info,
            .layout = compute_pipeline.layout,
        };
        ComputeEffect sky_effect {
            .name = "sky",
            .pipeline_layout = compute_pipeline.layout,
            .data = {
                { glm::vec4(.2, .4, .8, 1) },
                { glm::vec4(.4, .4, .4, 1) },
            },
        };

        //vk::PipelineCache cache = device.createPipelineCache(vk::PipelineCacheCreateInfo{}).value;
        compute_pipeline.handle  = device.createComputePipelines(nullptr, pipe_info).value.front();

        sky_effect.pipeline = device.createComputePipelines(nullptr, pipe_info).value.front();
        bg_effects.push_back(sky_effect);
        device.destroyShaderModule(sky);
    }

    //_____________________________________
    inline void build_graphics_pipeline() {

        //vk::ShaderModule vertex_shader   = load_shader_module("res/shaders/mesh.vert.spv").value;
        //vk::ShaderModule fragment_shader = load_shader_module("res/shaders/texture.frag.spv").value;

        //graphics_pipeline.shader_stages = {
        //    vk::PipelineShaderStageCreateInfo {
        //        .stage  = vk::ShaderStageFlagBits::eVertex,
        //        .module = vertex_shader,
        //        .pName  = "main",
        //    },
        //    vk::PipelineShaderStageCreateInfo {
        //        .stage  = vk::ShaderStageFlagBits::eFragment,
        //        .module = fragment_shader,
        //        .pName  = "main",
        //    }
        //};

        //vk::PipelineVertexInputStateCreateInfo vertex_input { };

        //vk::PipelineInputAssemblyStateCreateInfo input_assembly {
        //    .topology                = vk::PrimitiveTopology::eTriangleList,
        //    .primitiveRestartEnable  = false,
        //};

        //vk::PipelineViewportStateCreateInfo viewport_state {
        //    .viewportCount           = 1,
        //    .scissorCount            = 1,
        //};

        //vk::PipelineRasterizationStateCreateInfo rasterization_state {
        //    .polygonMode             = vk::PolygonMode::eFill,
        //    .cullMode                = vk::CullModeFlagBits::eNone,
        //    .frontFace               = vk::FrontFace::eClockwise,
        //    .lineWidth               = 1.f,
        //};

        //vk::PipelineMultisampleStateCreateInfo multisample_state {
        //    .rasterizationSamples    = vk::SampleCountFlagBits::e1,
        //    .sampleShadingEnable     = false,
        //    .minSampleShading        = 1.f,
        //    .pSampleMask             = nullptr,
        //    .alphaToCoverageEnable   = false,
        //    .alphaToOneEnable        = false,
        //};

        //vk::PipelineDepthStencilStateCreateInfo depth_stencil {
        //    .depthTestEnable         = true,
        //    .depthWriteEnable        = true,
        //    .depthCompareOp          = vk::CompareOp::eGreaterOrEqual,
        //    .depthBoundsTestEnable   = false,
        //    .stencilTestEnable       = false,
        //};

        //vk::PipelineColorBlendAttachmentState color_blend_attachment {
        //    .blendEnable             = false,
        //    .colorWriteMask          =  vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
        //                              | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        //};

        //vk::PipelineColorBlendStateCreateInfo blend_state {
        //    .logicOpEnable           = false,
        //    .logicOp                 = vk::LogicOp::eCopy,
        //    .attachmentCount         = 1,
        //    .pAttachments            = &color_blend_attachment,
        //    .blendConstants          {{ 0.f, 0.f, 0.f, 0.f }},
        //};

        //std::vector<vk::DynamicState> dynamic_states {
        //    vk::DynamicState::eViewport,
        //    vk::DynamicState::eScissor,
        //};
        //vk::PipelineDynamicStateCreateInfo dyn_state {
        //    .dynamicStateCount       = (u32)dynamic_states.size(),
        //    .pDynamicStates          =      dynamic_states.data(),
        //};

        //vk::PushConstantRange buffer_range {
        //    .stageFlags              = vk::ShaderStageFlagBits::eVertex,
        //    .offset                  = 0,
        //    .size                    = sizeof(GPUDrawPushConstants),
        //};
        //vk::PipelineLayoutCreateInfo layout_info {
        //    .setLayoutCount          = 1,
        //    .pSetLayouts             = &_single_img_layout,
        //    .pushConstantRangeCount  = 1,
        //    .pPushConstantRanges     = &buffer_range,
        //};

        //VK_CHECK(device.createPipelineLayout(&layout_info, nullptr, &graphics_pipeline.layout));

        //vk::PipelineRenderingCreateInfo render_info {
        //    .colorAttachmentCount    = 1,
        //    .pColorAttachmentFormats = &_draw.img.format,
        //    .depthAttachmentFormat   = _depth.img.format,
        //};

        //vk::GraphicsPipelineCreateInfo graphics_pipeline_info {
        //    .pNext                   = &render_info,
        //    .stageCount              = (u32)graphics_pipeline.shader_stages.size(),
        //    .pStages                 = graphics_pipeline.shader_stages.data(),
        //    .pVertexInputState       = &vertex_input,
        //    .pInputAssemblyState     = &input_assembly,
        //    .pViewportState          = &viewport_state,
        //    .pRasterizationState     = &rasterization_state,
        //    .pMultisampleState       = &multisample_state,
        //    .pDepthStencilState      = &depth_stencil,
        //    .pColorBlendState        = &blend_state,
        //    .pDynamicState           = &dyn_state,
        //    .layout                  = graphics_pipeline.layout,
        //};

        //graphics_pipeline.handle = device
        //    .createGraphicsPipelines(nullptr, graphics_pipeline_info)
        //    .value
        //    .front();

        //device.destroyShaderModule(vertex_shader);
        //device.destroyShaderModule(fragment_shader);

        _pbr_metallic_roughness.build_pipelines();
    }

    inline void setup_imgui() {
        vk::DescriptorPoolSize pool_sizes[] {
            { vk::DescriptorType::eSampler,              1000 },
            { vk::DescriptorType::eSampledImage,         1000 },
            { vk::DescriptorType::eStorageImage,         1000 },
            { vk::DescriptorType::eUniformTexelBuffer,   1000 },
            { vk::DescriptorType::eStorageTexelBuffer,   1000 },
            { vk::DescriptorType::eUniformBuffer,        1000 },
            { vk::DescriptorType::eStorageBuffer,        1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment,      1000 },
        };
        vk::DescriptorPoolCreateInfo pool_info {
            .flags          = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets        = 1000,
            .poolSizeCount  = (u32)std::size(pool_sizes),
            .pPoolSizes     = pool_sizes,
        };
        vk::DescriptorPool imgui_pool;
        VK_CHECK(device.createDescriptorPool(&pool_info, nullptr, &imgui_pool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window::handle, true);
        ImGui_ImplVulkan_InitInfo init_info {
            .Instance            = static_cast<VkInstance>(instance),
            .PhysicalDevice      = static_cast<VkPhysicalDevice>(phys_device),
            .Device              = static_cast<VkDevice>(device),
            .Queue               = static_cast<VkQueue>(graphics_queue.handle),
            .DescriptorPool      = static_cast<VkDescriptorPool>(imgui_pool),
            .RenderPass          = VK_NULL_HANDLE,
            .MinImageCount       = 3,
            .ImageCount          = 3,
            .MSAASamples         = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1),
            .UseDynamicRendering = true,
            .PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo {
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &swapchain.img_format,
            },
        };
        ImGui_ImplVulkan_Init(&init_info);

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.FontDefault = io.Fonts->AddFontFromFileTTF("res/fonts/NotoSansMono-Regular.ttf", 12.f);
        io.Fonts->Build();

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding    = 8.f;
        style.FrameRounding     = 8.f;
        style.ScrollbarRounding = 4.f;

        style.Colors[ImGuiCol_Text]     = ImVec4(.6f, .6f, .6f, 1.f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.f, 0.f, 0.f, .2f);
        style.Colors[ImGuiCol_Border]   = ImVec4(0.f, 0.f, 0.f, 0.f);

        _deletion_queue.push_function([&]{
            device.destroyDescriptorPool(imgui_pool);
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        });
    }

    //_____________________________________
    inline void setup_keybinds() {

        input::bind(GLFW_KEY_W,
            key_callback {
                .press   = [&]{ camera.request_movement.forward = 1; },
                .release = [&]{ camera.request_movement.forward = 0; }
        }   );
        input::bind(GLFW_KEY_S,
            key_callback {
                .press   = [&]{ camera.request_movement.back = 1; },
                .release = [&]{ camera.request_movement.back = 0; }
        }   );
        input::bind(GLFW_KEY_A,
            key_callback {
                .press   = [&]{ camera.request_movement.left = 1; },
                .release = [&]{ camera.request_movement.left = 0; }
        }   );
        input::bind(GLFW_KEY_D,
            key_callback {
                .press   = [&]{ camera.request_movement.right = 1; },
                .release = [&]{ camera.request_movement.right = 0; }
        }   );
        input::bind(GLFW_KEY_Q,
            key_callback {
                .press   = [&]{ camera.request_movement.down = 1; },
                .release = [&]{ camera.request_movement.down = 0; }
        }   );
        input::bind(GLFW_KEY_E,
            key_callback {
                .press   = [&]{ camera.request_movement.up = 1; },
                .release = [&]{ camera.request_movement.up = 0; }
        }   );
        input::bind(GLFW_KEY_LEFT_SHIFT,
            key_callback {
                .press   = [&]{ camera.movement_speed = 2.f; },
                .release = [&]{ camera.movement_speed = 1.f; }
        }   );
        input::bind(GLFW_MOUSE_BUTTON_2,
            key_callback {
                .press   = [&]{
                    glfwSetInputMode(window::handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    camera.follow_mouse = true;
                },
                .release = [&]{
                    glfwSetInputMode(window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    camera.follow_mouse = false;
                }
        }   );
    };

    //_____________________________________
    inline void init() {

        setup_keybinds();

        u32 glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        VULKAN_HPP_DEFAULT_DISPATCHER.init();

        instance = vk::createInstance(
            vk::InstanceCreateInfo {
                .pApplicationInfo        = &engine_info,
                .enabledLayerCount       = (u32)_layers.size(),
                .ppEnabledLayerNames     = _layers.data(),
                .enabledExtensionCount   = (u32)extensions.size(),
                .ppEnabledExtensionNames = extensions.data(),
        }   ).value;
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

        debug.messenger = instance.createDebugUtilsMessengerEXT(
            vk::DebugUtilsMessengerCreateInfoEXT {
                .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                  | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                  | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                  | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                .messageType     = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                  | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                                  | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
                .pfnUserCallback = &debug.callback,
            },
            nullptr
        ).value;

        glfwCreateWindowSurface(instance, sigil::window::handle, nullptr, (VkSurfaceKHR*)&surface);

        phys_device = instance.enumeratePhysicalDevices().value[0]; // TODO: propperly pick GPU
        float prio = 1.f;
        vk::DeviceQueueCreateInfo queue_info {
            .queueFamilyIndex   = graphics_queue.family,
            .queueCount         = 1,
            .pQueuePriorities   = &prio,
        };

        vk::PhysicalDeviceDynamicRenderingFeatures dyn_features {
            .dynamicRendering = true,
        };
        vk::PhysicalDeviceSynchronization2Features sync_features {
            .pNext = &dyn_features,
            .synchronization2 = true,
        };
        vk::PhysicalDeviceDescriptorIndexingFeatures desc_index_features {
            .pNext = &sync_features,
        };
        vk::PhysicalDeviceBufferDeviceAddressFeatures buffer_device_features {
            .pNext = &desc_index_features,
        };
        vk::PhysicalDeviceFeatures2 device_features_2 {
            .pNext = &buffer_device_features,
        };
        phys_device.getFeatures2(&device_features_2);
        device = phys_device.createDevice(
            vk::DeviceCreateInfo{
                .pNext                   = &device_features_2,
                .queueCreateInfoCount    = 1,
                .pQueueCreateInfos       = &queue_info,
                .enabledLayerCount       = (u32)_layers.size(),
                .ppEnabledLayerNames     = _layers.data(),
                .enabledExtensionCount   = (u32)_device_exts.size(),
                .ppEnabledExtensionNames =  _device_exts.data(),
                //.pEnabledFeatures        = &device_features,
            },
            nullptr
        ).value;
        //VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

        graphics_queue.family = get_queue_family(vk::QueueFlagBits::eGraphics);
        graphics_queue.handle = device.getQueue(graphics_queue.family, 0);

        vma::AllocatorCreateInfo alloc_info {
            .flags = vma::AllocatorCreateFlagBits::eBufferDeviceAddress,
            .physicalDevice = phys_device,
            .device = device,
            .instance = instance,
        };
        vma_allocator = vma::createAllocator(alloc_info).value;

        build_swapchain();

        build_command_structures();
        build_sync_structures();
        build_descriptors();
        build_compute_pipeline();
        build_graphics_pipeline();
        setup_imgui();

        //Vertex rect_vertices[4] {
        //    Vertex {
        //        .position = { .5, -.5, 0 },
        //        .color    = { 0, 1, 1, 1},
        //    },
        //    Vertex {
        //        .position = { .5, .5, 0 },
        //        .color    = { 0, 1, 1, 1 },
        //    },
        //    Vertex {
        //        .position = { -.5, -.5, 0 },
        //        .color    = { 0, 1, 1, 1 },
        //    },
        //    Vertex {
        //        .position = { -.5, .5, 0 },
        //        .color    = { 0, 1, 1, 1 },
        //    },
        //};
        //u32 rect_indices[6] {
        //    0, 1, 2,
        //    2, 1, 3,
        //};
        //auto rect = upload_mesh(rect_indices, rect_vertices);
        u32 white = 0xFFFFFFFF;
        _white_img = create_img((void*)&white, vk::Extent3D { 16, 16, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);

        u32 black = 0x00000000;
        u32 magenta = 0x00FF00FF;
        std::array<u32, 16 * 16> pixels;
        for( int x = 0; x < 16; x++ ) {
            for( int y = 0; y < 16; y++ ) {
                pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
            }
        }
        _error_img = create_img(pixels.data(), vk::Extent3D { 16, 16, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);

        vk::SamplerCreateInfo sampler {
            .magFilter = vk::Filter::eNearest,
            .minFilter = vk::Filter::eNearest,
        };
        VK_CHECK(device.createSampler(&sampler, nullptr, &_sampler_nearest));
        sampler = {
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
        };
        VK_CHECK(device.createSampler(&sampler, nullptr, &_sampler_linear));

        //auto loaded_meshes = load_model("res/models/SciFiHelmet.gltf");
        //auto loaded_meshes = load_model("res/models/model.obj");
        //auto loaded_meshes = load_model("res/models/DamagedHelmet.gltf");
        //if( !loaded_meshes.has_value() ) {
        //    std::cout << "\nError:\n>>\tFailed to load meshes.\n";
        //}
        //for( auto& mesh : loaded_meshes.value()) {
        //    _meshes.push_back(*mesh);
        //}
        
        ////
        PbrMetallicRoughness::MaterialResources material_resources {
            .color_img = _error_img,
            .color_sampler = _sampler_linear,
            .metal_roughness_img = _error_img,
            .metal_roughness_sampler = _sampler_linear,
        };
        AllocatedBuffer material_constants = create_buffer(sizeof(PbrMetallicRoughness::MaterialConstants), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);
        PbrMetallicRoughness::MaterialConstants* scene_uniform_data = (PbrMetallicRoughness::MaterialConstants*)material_constants.info.pMappedData;
        scene_uniform_data->color_factors = glm::vec4{ 1, 1, 1, 1 };
        scene_uniform_data->metal_roughness_factors = glm::vec4 { 1, .5, 0, 0 };
        _deletion_queue.push_function([=]{
            destroy_buffer(material_constants);
        });
        material_resources.data = material_constants.handle;
        material_resources.offset = 0;
        _default_material_instance = _pbr_metallic_roughness.write_material(material_resources, _descriptor_allocator);

        auto loaded_meshes = load_model("res/models/DamagedHelmet.gltf");
        if( !loaded_meshes.has_value() ) {
            std::cout << "\nError:\n>>\tFailed to load meshes.\n";
        }
        for( auto& mesh : loaded_meshes.value()) {
            _meshes.push_back(*mesh);

            for( auto& surface : mesh->surfaces ) {
                _render_objects.push_back(
                    RenderObject {
                        .count = surface.count,
                        .first = surface.start_index,
                        .index_buffer = mesh->mesh_buffer.index_buffer.handle,
                        .material = &_default_material_instance,//&surface.material->data,
                        .transform = glm::mat4 { 1 },
                        .address = mesh->mesh_buffer.vertex_buffer_address,
                    }
                );
            }
        }
        auto plane = primitives::Plane();
        auto plane_buffer = upload_mesh(plane.indices, plane.vertices);

        _render_objects.push_back(
            RenderObject {
                .count = plane.surface.count,
                .first = plane.surface.start_index,
                .index_buffer = plane_buffer.index_buffer.handle,
                .material = &_default_material_instance,
                .transform = glm::mat4 { 1 },
                .address = plane_buffer.vertex_buffer_address,
            }
        );
    }

    //_____________________________________
    inline void update_scene() {
        _scene_data = {
            .view = camera.get_view(),
            .proj = glm::perspective(
                glm::radians(70.f),
                (f32)swapchain.extent.width/(f32)swapchain.extent.height,
                camera.clip_plane.near,
                camera.clip_plane.far
            ),
            .ambient_color = glm::vec4 { .1f },
            .sun_dir = glm::vec4 { 0, 1, .5, 1.f },
            .sun_color = glm::vec4 { 1.f },
        };
        _scene_data.proj[1][1] *= -1;
    }

    //_____________________________________
    inline void draw_geometry(FrameData& frame, vk::CommandBuffer cmd) {

        vk::RenderingAttachmentInfo attach_info {
            .imageView = _draw.img.view,
            .imageLayout = vk::ImageLayout::eGeneral,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };

        vk::RenderingAttachmentInfo depth_info {
            .imageView = _depth.img.view,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };

        vk::RenderingInfo render_info {
            .renderArea = {
                .offset = vk::Offset2D {
                    .x = 0,
                    .y = 0,
                },
                .extent = _draw.extent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attach_info,
            .pDepthAttachment = &depth_info,
            .pStencilAttachment = nullptr,
        };

        cmd.beginRendering(&render_info);

        //cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline.handle);

        vk::Viewport viewport {
            .x = 0,
            .y = 0,
            .width = (float)_draw.extent.width,
            .height = (float)_draw.extent.height,
            .minDepth = 1.f,
            .maxDepth = 0.f,
        };
        cmd.setViewport(0, 1, &viewport);

        vk::Rect2D scissor {
            .offset {
                .x = 0,
                .y = 0,
            },
            .extent {
                .width = _draw.extent.width,
                .height = _draw.extent.height,
            },
        };
        cmd.setScissor(0, 1, &scissor);

        //vk::DescriptorSet img_set = frame.descriptors.allocate(_single_img_layout);
        //{
        //    DescriptorWriter {}
        //        .write_img(0, _error_img.img.view, _sampler_nearest, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
        //        .update_set(img_set);
        //}
        //cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline.layout, 0, img_set, nullptr);

        glm::mat4 projection = glm::perspective(
            glm::radians(70.f),
            (f32)swapchain.extent.width/(f32)swapchain.extent.height,
            camera.clip_plane.near,
            camera.clip_plane.far
        );

        AllocatedBuffer gpu_scene_data_buffer = create_buffer(sizeof(GPUSceneData), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);

        frame.deletion_queue.push_function([=] {
            destroy_buffer(gpu_scene_data_buffer);
        });

        GPUSceneData* scene_uniform_data = (GPUSceneData*)gpu_scene_data_buffer.info.pMappedData;
        *scene_uniform_data = _scene_data;

        vk::DescriptorSet descriptor_set = frame.descriptors.allocate(_scene_data_layout);

        DescriptorWriter {}
            .write_buffer(0, gpu_scene_data_buffer.handle, sizeof(GPUSceneData), 0, vk::DescriptorType::eUniformBuffer)
            .update_set(descriptor_set);
        
        for( auto& render : _render_objects ) {
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, render.material->pipeline->handle);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, render.material->pipeline->layout, 0, 1, &descriptor_set, 0, nullptr);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, render.material->pipeline->layout, 1, 1, &render.material->material_set, 0, nullptr);

            cmd.bindIndexBuffer(render.index_buffer, 0, vk::IndexType::eUint32);
            GPUDrawPushConstants push_constants {
                .world_matrix = render.transform * projection * camera.get_view(),
                .vertex_buffer = render.address,
            };
            cmd.pushConstants(render.material->pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(GPUDrawPushConstants), &push_constants);

            cmd.drawIndexed(render.count, 1, render.first, 0, 0);
        }
        cmd.endRendering();
    };
    
    //_____________________________________
    inline void tick() {
        if( request_resize ) { rebuild_swapchain(); }
        FrameData& frame = frames.at(current_frame % FRAME_OVERLAP);

        VK_CHECK(device.waitForFences(1, &frame.fence, true, UINT64_MAX));
        frame.deletion_queue.flush();
        frame.descriptors.clear();

        auto img = device.acquireNextImageKHR(swapchain.handle, UINT64_MAX, frame.swap_semaphore);
        if( img.result == vk::Result::eErrorOutOfDateKHR || img.result == vk::Result::eSuboptimalKHR ) {
            request_resize = true;
        }
        camera.update(time::delta_time);
        u32 img_index = img.value;
        vk::Image& swap_img = swapchain.images.at(img_index);

        VK_CHECK(device.resetFences(1, &frame.fence));
        frame.cmd.buffer.reset();

        _draw.extent.width  = _draw.img.extent.width;
        _draw.extent.height = _draw.img.extent.height;
        VK_CHECK(frame.cmd.buffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, }));
        {
            transition_img(frame.cmd.buffer, _draw.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

            {
                ComputeEffect& effect = bg_effects[current_bg_effect];

                frame.cmd.buffer.bindPipeline(vk::PipelineBindPoint::eCompute, effect.pipeline);
                frame.cmd.buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline.layout, 0, _draw.descriptor.set.handle, nullptr);
                frame.cmd.buffer.pushConstants(compute_pipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstants), &effect.data);
                frame.cmd.buffer.dispatch(std::ceil(_draw.extent.width / 16.f), std::ceil(_draw.extent.height / 16.f), 1);
            }

            transition_img(frame.cmd.buffer, _draw.img.handle, vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal);
            transition_img(frame.cmd.buffer, _depth.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

            update_scene();
            draw_geometry(frame, frame.cmd.buffer);

            transition_img(frame.cmd.buffer, _draw.img.handle, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
            transition_img(frame.cmd.buffer, swap_img, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

            copy_img_to_img(frame.cmd.buffer, _draw.img.handle, swap_img, _draw.extent, swapchain.extent);

            transition_img(frame.cmd.buffer, swap_img, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);

            ui::draw(frame.cmd.buffer, swapchain.img_views[img_index]);

            transition_img(frame.cmd.buffer, swap_img, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
        }
        VK_CHECK(frame.cmd.buffer.end());

        vk::CommandBufferSubmitInfo cmd_info {
            .commandBuffer = frame.cmd.buffer,
            .deviceMask    = 0,
        };

        vk::SemaphoreSubmitInfo wait_info {
            .semaphore   = frame.swap_semaphore,
            .value       = 1,
            .stageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .deviceIndex = 0,
        };

        vk::SemaphoreSubmitInfo signal_info {
            .semaphore   = frame.render_semaphore,
            .value       = 1,
            .stageMask   = vk::PipelineStageFlagBits2::eAllGraphics,
            .deviceIndex = 0,
        };

        vk::SubmitInfo2 submit {
            .waitSemaphoreInfoCount   = 1,
            .pWaitSemaphoreInfos      = &wait_info,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &cmd_info,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signal_info,
        };

        VK_CHECK(graphics_queue.handle.submit2(submit, frame.fence));

        vk::PresentInfoKHR present_info {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &frame.render_semaphore,
            .swapchainCount     = 1,
            .pSwapchains        = &swapchain.handle,
            .pImageIndices      = &img_index,
        };
        vk::Result present_result = graphics_queue.handle.presentKHR(&present_info);
        if( present_result == vk::Result::eErrorOutOfDateKHR || present_result == vk::Result::eSuboptimalKHR ) {
            request_resize = true;
        }

        current_frame++;
    }

    //_____________________________________
    inline void terminate() {
        VK_CHECK(device.waitIdle());

        _deletion_queue.flush();
        for( auto& mesh : _meshes ) {
            vma_allocator.destroyBuffer(mesh.mesh_buffer.index_buffer.handle, mesh.mesh_buffer.index_buffer.allocation);
            vma_allocator.destroyBuffer(mesh.mesh_buffer.vertex_buffer.handle, mesh.mesh_buffer.vertex_buffer.allocation);
        }

        device.destroyPipelineLayout(compute_pipeline.layout);
        device.destroyPipeline(compute_pipeline.handle);

        device.destroyPipelineLayout(graphics_pipeline.layout);
        device.destroyPipeline(graphics_pipeline.handle);

        //for( auto effect : bg_effects ) {
        //    device.destroyPipelineLayout(effect.pipeline_layout);
        //    device.destroyPipeline(effect.pipeline);
        //}

        device.destroySwapchainKHR(swapchain.handle);
        for( auto view : swapchain.img_views ) {
            device.destroyImageView(view);
        }
        device.destroyImageView(_draw.img.view);
        device.destroyImageView(_depth.img.view);
        vma_allocator.destroyImage(_draw.img.handle, _draw.img.alloc);
        vma_allocator.destroyImage(_depth.img.handle, _depth.img.alloc);
        device.destroyDescriptorPool(_draw.descriptor.pool);
        device.destroyDescriptorSetLayout(_draw.descriptor.set.layout);
        for( auto frame : frames ) {
            device.destroyCommandPool(frame.cmd.pool);
            device.destroyFence(frame.fence);
            device.destroySemaphore(frame.swap_semaphore);
            device.destroySemaphore(frame.render_semaphore);
        }
        device.destroyCommandPool(immediate.pool);
        device.destroyFence(immediate.fence);
        vma_allocator.destroy();
        instance.destroySurfaceKHR(surface);
        device.destroy();
        instance.destroyDebugUtilsMessengerEXT(debug.messenger);
        instance.destroy();
    }
} // sigil::renderer

struct vulkan {
    vulkan() {
        using namespace sigil;
        schedule(eInit, renderer::init     );
        schedule(eTick, renderer::tick     );
        schedule(eExit, renderer::terminate);
    }
};

