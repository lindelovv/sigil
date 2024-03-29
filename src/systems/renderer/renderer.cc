#include "renderer.hh"
#include "glfw.hh"
#include "sigil.hh"
#include "util.hh"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <glm/fwd.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui/imgui.h>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <cstring>
#include <map>
#include <set>
#include <algorithm>
#include <chrono>
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <imgui/imgui.h>
#include <imgui/imconfig.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <assimp/Importer.hpp>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

//_____________________________________
// Since a lot of the vulkan initialization logic is only needed once (with exceptions),
// the code is contained within the single function but contained within commented blocks.
void renderer::init() {

    //____________________________________
    // Setup keybinds
    {
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
    }

    //____________________________________
    // Instance creation
    {
#ifdef _DEBUG
        for( auto requsted_layer : validation_layers ) {
            bool layer_found = false;
            for( const auto& available_layer : unwrap(vk::enumerateInstanceLayerProperties()) ) {
                if( strcmp(requsted_layer, available_layer.layerName) == 0 ) {
                    layer_found = true;
                    break;
                }
            }
            if( !layer_found ) {
                throw std::runtime_error("\tError: Validation layers requested but not available.\n");
            }
        }
        auto debug_msgr_create_info = vk::DebugUtilsMessengerCreateInfoEXT {
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                              | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                              | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                              | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType     = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                              | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                              | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = &debug_callback,
        };
#endif
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
#ifdef _DEBUG
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        instance = expect("Failed to create instance.",
            vk::createInstance(
                vk::InstanceCreateInfo {
#ifdef _DEBUG
                    .pNext                      = &debug_msgr_create_info,
                    .pApplicationInfo           = &engine_info,
                    .enabledLayerCount          = static_cast<uint32_t>(validation_layers.size()),
                    .ppEnabledLayerNames        = validation_layers.data(),
                    .enabledExtensionCount      = static_cast<uint32_t>(extensions.size()),
                    .ppEnabledExtensionNames    = extensions.data(),
        }   )   );
        // @TODO: setup debug linking so _DEBUG will work at all
        debug_messenger = expect("Could not create debug messenger.",
            instance.createDebugUtilsMessengerEXT(debug_msgr_create_info, nullptr)
        );
#else 
                    .pNext                      = nullptr,
                    .pApplicationInfo           = &engine_info,
                    .enabledLayerCount          = 0,
                    .enabledExtensionCount      = static_cast<uint32_t>(extensions.size()),
                    .ppEnabledExtensionNames    = extensions.data(),
        }   )   );
#endif
    }

    //_____________________________________
    // Bind surface to window
    {
        expect("Failed to create main window surface.",
            glfwCreateWindowSurface(instance, window::handle, nullptr, (VkSurfaceKHR*)&surface)
        );
    }

    //_____________________________________
    // Select physical device   
    {
        for( const vk::PhysicalDevice& phys_device : instance.enumeratePhysicalDevices().value ) {
            std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());
            for( const auto& extension : phys_device.enumerateDeviceExtensionProperties().value ) {
                required_extensions.erase(extension.extensionName);
            }
            if( !required_extensions.empty()
                || (find_queue_families(phys_device).is_complete()
                    && phys_device.getSurfaceFormatsKHR(surface).value.empty()
                    && phys_device.getSurfacePresentModesKHR(surface).value.empty()
                    && phys_device.getFeatures().samplerAnisotropy)
            ) { continue; }
            physical_device = phys_device;
            vk::PhysicalDeviceProperties properties = physical_device.getProperties();
            vk::SampleCountFlags counts = properties.limits.framebufferColorSampleCounts 
                                        & properties.limits.framebufferDepthSampleCounts;
            msaa_samples = counts & vk::SampleCountFlagBits::e64 ? vk::SampleCountFlagBits::e64 :
                           counts & vk::SampleCountFlagBits::e32 ? vk::SampleCountFlagBits::e32 :
                           counts & vk::SampleCountFlagBits::e16 ? vk::SampleCountFlagBits::e16 :
                           counts & vk::SampleCountFlagBits::e8  ? vk::SampleCountFlagBits::e8  :
                           counts & vk::SampleCountFlagBits::e4  ? vk::SampleCountFlagBits::e4  :
                           counts & vk::SampleCountFlagBits::e2  ? vk::SampleCountFlagBits::e2  :
                                                                   vk::SampleCountFlagBits::e1  ;
            break;
        }
        expect("Failed to find suitable GPU.", physical_device);
    }

    //_____________________________________
    // Device creation
    {
        QueueFamilyIndices indices = find_queue_families(physical_device);
        std::vector<vk::DeviceQueueCreateInfo> queue_info_vec;
        std::set<uint32_t> unique_queue_familes = { indices.graphics_family.value(),
                                                    indices.present_family .value() };
        float queue_priority = 1.f;
        for( uint32_t queue_family : unique_queue_familes ) {
            vk::DeviceQueueCreateInfo queue_info {
                .queueFamilyIndex           = queue_family,
                .queueCount                 = 1,
                .pQueuePriorities           = &queue_priority,
            };
            queue_info_vec.push_back(queue_info);
        }
        vk::PhysicalDeviceFeatures device_features {
            .sampleRateShading              = true,
            .samplerAnisotropy              = true,
        };
        vk::DeviceCreateInfo device_info {
            .queueCreateInfoCount           = static_cast<uint32_t>(queue_info_vec.size()),
            .pQueueCreateInfos              = queue_info_vec.data(),
#ifdef _DEBUG
            .enabledLayerCount   = static_cast<uint32_t>(validation_layers.size()),
            .ppEnabledLayerNames = validation_layers.data(),
#else
            .enabledLayerCount   = 0,
#endif
            .enabledExtensionCount          = static_cast<uint32_t>(device_extensions.size()),
            .ppEnabledExtensionNames        = device_extensions.data(),
            .pEnabledFeatures               = &device_features,
        };
        expect("Failed to create logical device.",
            physical_device.createDevice(&device_info, nullptr, &device)
        );
        graphics_queue = device.getQueue(indices.graphics_family.value(), 0);
        present_queue  = device.getQueue(indices.present_family .value(), 0);
    }

    //_____________________________________
    // Swapchain creation
    {
        create_swap_chain();
        create_img_views();
    }

    //_____________________________________
    // Create render pass
    {
        std::array<vk::AttachmentDescription, 3> attachments = {
            vk::AttachmentDescription { // Color Attachment
                .format         = swapchain.img_format,
                .samples        = msaa_samples,
                .loadOp         = vk::AttachmentLoadOp::eClear,
                .storeOp        = vk::AttachmentStoreOp::eDontCare,
                .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout  = vk::ImageLayout::eUndefined,
                .finalLayout    = vk::ImageLayout::eColorAttachmentOptimal,
            },
            vk::AttachmentDescription { // Depth Attachment
                .format         = find_depth_format(),
                .samples        = msaa_samples,
                .loadOp         = vk::AttachmentLoadOp::eClear,
                .storeOp        = vk::AttachmentStoreOp::eDontCare,
                .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout  = vk::ImageLayout::eUndefined,
                .finalLayout    = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            },
            vk::AttachmentDescription { // Color Attachment Resolve
                .format         = swapchain.img_format,
                .samples        = vk::SampleCountFlagBits::e1,
                .loadOp         = vk::AttachmentLoadOp::eDontCare,
                .storeOp        = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout  = vk::ImageLayout::eUndefined,
                .finalLayout    = vk::ImageLayout::ePresentSrcKHR,
            },
        };
        vk::AttachmentReference color_attachment_ref {
            .attachment     = 0,
            .layout         = vk::ImageLayout::eColorAttachmentOptimal,
        };
        vk::AttachmentReference depth_attachment_ref {
            .attachment     = 1,
            .layout         = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        };
        vk::AttachmentReference color_attachment_resolve_ref {
            .attachment     = 2,
            .layout         = vk::ImageLayout::eColorAttachmentOptimal,
        };
        vk::SubpassDescription subpass {
            .pipelineBindPoint       = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount    = 1,
            .pColorAttachments       = &color_attachment_ref,
            .pResolveAttachments     = &color_attachment_resolve_ref,
            .pDepthStencilAttachment = &depth_attachment_ref,
        };
        vk::SubpassDependency dependency {
            .srcSubpass    = VK_SUBPASS_EXTERNAL,
            .dstSubpass    = 0,
            .srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput
                            | vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput
                            | vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
                            | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        };
        vk::RenderPassCreateInfo render_pass_info {
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments    = attachments.data(),
            .subpassCount    = 1,
            .pSubpasses      = &subpass,
            .dependencyCount = 1,
            .pDependencies   = &dependency,
        };
        render_pass = expect("Failed to create render pass.",
            device.createRenderPass(render_pass_info)
        );
    }

    //_____________________________________
    // Descriptor set layout creation
    {
        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
            vk::DescriptorSetLayoutBinding { // UBO Layout Binding
                .binding            = 0,
                .descriptorType     = vk::DescriptorType::eUniformBuffer,
                .descriptorCount    = 1,
                .stageFlags         = vk::ShaderStageFlagBits::eVertex,
                .pImmutableSamplers = nullptr,
            },
            vk::DescriptorSetLayoutBinding { // Sampler Layout Bindning
                .binding            = 1,
                .descriptorType     = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount    = 1,
                .stageFlags         = vk::ShaderStageFlagBits::eFragment,
                .pImmutableSamplers = nullptr,
            },
        };
        vk::DescriptorSetLayoutCreateInfo layout_info {
            .bindingCount       = static_cast<uint32_t>(bindings.size()),
            .pBindings          = bindings.data(),
        };
        descriptor.uniform_set_layout = expect("Failed to create descriptor set layout.",
            device.createDescriptorSetLayout(layout_info)
        );
    }

    //_____________________________________
    // Graphics pipeline creation
    {
        std::vector<char> vertex_code        = read_file("./shaders/simple_shader.vert.spv");
        std::vector<char> fragment_code      = read_file("./shaders/simple_shader.frag.spv");
        vk::ShaderModule  vert_shader_module = create_shader_module(vertex_code);
        vk::ShaderModule  frag_shader_module = create_shader_module(fragment_code);

        vk::PipelineShaderStageCreateInfo vert_shader_stage_info {
            .stage  = vk::ShaderStageFlagBits::eVertex,
            .module = vert_shader_module,
            .pName  = "main",
        };
        vk::PipelineShaderStageCreateInfo frag_shader_stage_info {
            .stage  = vk::ShaderStageFlagBits::eFragment,
            .module = frag_shader_module,
            .pName  = "main",
        };
        vk::PipelineShaderStageCreateInfo shader_stages[] = {
            vert_shader_stage_info,
            frag_shader_stage_info
        };
        auto binding_description = Vertex::get_binding_description();
        auto attribute_descriptions = Vertex::get_attribute_descriptions();
        vk::PipelineVertexInputStateCreateInfo vertex_input_info {
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &binding_description,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
            .pVertexAttributeDescriptions    = attribute_descriptions.data(),
        };
        vk::PipelineInputAssemblyStateCreateInfo input_assembly {
            .topology                = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable  = false,
        };
        vk::PipelineViewportStateCreateInfo viewport_state {
            .viewportCount           = 1,
            .scissorCount            = 1,
        };
        vk::PipelineRasterizationStateCreateInfo rasterizer {
            .depthClampEnable        = false,
            .rasterizerDiscardEnable = false,
            .polygonMode             = vk::PolygonMode::eFill,
            .cullMode                = vk::CullModeFlagBits::eBack,
            .frontFace               = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable         = false,
            .lineWidth               = 1.f,
        };
        vk::PipelineMultisampleStateCreateInfo multisampling {
            .rasterizationSamples    = msaa_samples,
            .sampleShadingEnable     = true,
            .minSampleShading        = .2f,
        };
        vk::PipelineDepthStencilStateCreateInfo depth_stencil {
            .depthTestEnable         = true,
            .depthWriteEnable        = true,
            .depthCompareOp          = vk::CompareOp::eLess,
            .depthBoundsTestEnable   = false,
            .stencilTestEnable       = false,
        };
        vk::PipelineColorBlendAttachmentState color_blend_attachment {
            .blendEnable             = false,
            .colorWriteMask          = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                     | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };
        vk::PipelineColorBlendStateCreateInfo color_blending {
            .logicOpEnable           = false,
            .logicOp                 = vk::LogicOp::eCopy,
            .attachmentCount         = 1,
            .pAttachments            = &color_blend_attachment,
            .blendConstants          {{ 0.f, 0.f, 0.f, 0.f }},
                                     // [0]  [1]  [2]  [3]
        };
        std::vector<vk::DynamicState> dynamic_states {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamic_state {
            .dynamicStateCount   = static_cast<uint32_t>(dynamic_states.size()),
            .pDynamicStates      = dynamic_states.data(),
        };
        pipeline_layout = expect("Failed to create pipeline layout.",
            device.createPipelineLayout(
                vk::PipelineLayoutCreateInfo{
                    .setLayoutCount = 1,
                    .pSetLayouts = &descriptor.uniform_set_layout,
        }   )   );
        graphics_pipeline = expect("Failed to create graphics pipeline.",
            device.createGraphicsPipeline(
                nullptr,
                vk::GraphicsPipelineCreateInfo {
                    .stageCount          = 2,
                    .pStages             = shader_stages,
                    .pVertexInputState   = &vertex_input_info,
                    .pInputAssemblyState = &input_assembly,
                    .pViewportState      = &viewport_state,
                    .pRasterizationState = &rasterizer,
                    .pMultisampleState   = &multisampling,
                    .pDepthStencilState  = &depth_stencil,
                    .pColorBlendState    = &color_blending,
                    .pDynamicState       = &dynamic_state,
                    .layout              = pipeline_layout,
                    .renderPass          = render_pass,
                    .subpass             = 0,
                    .basePipelineHandle  = VK_NULL_HANDLE,
        }   )   );
        device.destroyShaderModule(frag_shader_module);
        device.destroyShaderModule(vert_shader_module);
    }

    //_____________________________________
    // Command pool creation
    {
        QueueFamilyIndices queue_family_indices = find_queue_families(physical_device);
        vk::CommandPoolCreateInfo pool_info {
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queue_family_indices.graphics_family.value(),
        };
        command_pool = expect("Failed to create command pool.",
            device.createCommandPool(pool_info)
        );
    }

    //_____________________________________
    // Create swapchain resources
    {
        create_color_resources();
        create_depth_resources();
        create_framebuffers();
    }

    //_____________________________________
    // Texture image, view and sampler creation
    {
        for( auto& texture : textures ) {
            int t_width, t_height, t_channels;
            stbi_uc* pixels = stbi_load(texture.path.c_str(), &t_width, &t_height, &t_channels, STBI_rgb_alpha);
            vk::DeviceSize img_size = t_width * t_height * 4;
            //expect("Failed to load texture image.", pixels);

            vk::Buffer staging_buffer;
            vk::DeviceMemory staging_buffer_memory;
            create_buffer(
                img_size,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging_buffer,
                staging_buffer_memory
            );
            void* data = device.mapMemory(staging_buffer_memory, 0, img_size).value;
            {
                memcpy(data, pixels, static_cast<size_t>(img_size));
            }
            device.unmapMemory(staging_buffer_memory);
            stbi_image_free(pixels);
            texture.mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(t_width, t_height)))) + 1;

            create_img(
                t_width, 
                t_height, 
                texture.mip_levels,
                vk::SampleCountFlagBits::e1,
                vk::Format::eR8G8B8A8Srgb,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                texture.image,
                texture.image_memory
            );
            transition_img_layout(
                texture.image,
                vk::Format::eR8G8B8A8Srgb,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                texture.mip_levels
            );
            copy_buffer_to_img(
                staging_buffer,
                texture.image,
                static_cast<uint32_t>(t_width),
                static_cast<uint32_t>(t_width)
            );
            device.destroyBuffer(staging_buffer);
            device.freeMemory(staging_buffer_memory);
            generate_mipmaps(texture.image, vk::Format::eR8G8B8A8Srgb, t_width, t_height, texture.mip_levels);

            texture.image_view = create_img_view(
                                    texture.image,
                                    vk::Format::eR8G8B8A8Srgb,
                                    vk::ImageAspectFlagBits::eColor,
                                    texture.mip_levels
                                );

            texture.sampler = expect("Failed to create texture sampler.",
                device.createSampler(
                    vk::SamplerCreateInfo {
                        .magFilter               = vk::Filter::eLinear,
                        .minFilter               = vk::Filter::eLinear,
                        .mipmapMode              = vk::SamplerMipmapMode::eLinear,
                        .addressModeU            = vk::SamplerAddressMode::eRepeat,
                        .addressModeV            = vk::SamplerAddressMode::eRepeat,
                        .addressModeW            = vk::SamplerAddressMode::eRepeat,
                        .mipLodBias              = 0.f,
                        .anisotropyEnable        = true,
                        .maxAnisotropy           = physical_device.getProperties().limits.maxSamplerAnisotropy,
                        .compareEnable           = false,
                        .compareOp               = vk::CompareOp::eAlways,
                        .minLod                  = 0.f,
                        .maxLod                  = static_cast<float>(texture.mip_levels),
                        .borderColor             = vk::BorderColor::eIntOpaqueBlack,
                        .unnormalizedCoordinates = false,
            }   )  );
        }
    }

    //_____________________________________
    // Load model
    {
        // assimp
        Assimp::Importer importer;
        std::vector<Vertex> VertexBuffer;
        if( auto file = importer.ReadFile(MODEL_PATH.c_str(), aiProcess_Triangulate | aiProcess_FlipUVs) ) {
            Model model;
            for( uint32_t i = 0; i < file->mNumMeshes; i++ ) {
                Mesh mesh;
                std::unordered_map<Vertex, uint32_t> unique_vertices {};
                for( uint32_t j = 0; j <= file->mMeshes[i]->mNumVertices; j++ ) {
                    Vertex vertex {
                        .pos = {
                            file->mMeshes[i]->mVertices[j].x,
                            file->mMeshes[i]->mVertices[j].y,
                            file->mMeshes[i]->mVertices[j].z,
                        },
                        .color = {
                            1.f,
                            1.f,
                            1.f,
                        },
                        .uvs = {
                            file->mMeshes[i]->mTextureCoords[0][j].x,
                            file->mMeshes[i]->mTextureCoords[0][j].y,
                        }
                    };
                    mesh.vertices.pos.push_back(vertex);
                }
                for( uint64_t j = 0; j < file->mMeshes[i]->mNumFaces; j++ ) {
                    const aiFace& face = file->mMeshes[i]->mFaces[j];
                    assert(face.mNumIndices == 3);
                    mesh.indices.unique.push_back(face.mIndices[0]);
                    mesh.indices.unique.push_back(face.mIndices[1]);
                    mesh.indices.unique.push_back(face.mIndices[2]);
                }
                //_____________________________________
                // Vertex buffer creation
                {
                    vk::DeviceSize buffer_size = sizeof(mesh.vertices.pos[0]) * mesh.vertices.pos.size();

                    vk::Buffer staging_buffer;
                    vk::DeviceMemory staging_buffer_memory;
                    create_buffer(
                        buffer_size,
                        vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                        staging_buffer,
                        staging_buffer_memory
                    );
                    void* data = device.mapMemory(staging_buffer_memory, 0, buffer_size).value;
                    {
                        memcpy(data, mesh.vertices.pos.data(), (size_t) buffer_size);
                    }
                    device.unmapMemory(staging_buffer_memory);

                    create_buffer(
                        buffer_size,
                        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                        vk::MemoryPropertyFlagBits::eDeviceLocal,
                        mesh.vertices.buffer,
                        mesh.vertices.memory
                    );
                    copy_buffer(staging_buffer, mesh.vertices.buffer, buffer_size);
                    device.destroyBuffer(staging_buffer);
                    device.freeMemory(staging_buffer_memory);
                }

                //_____________________________________
                // Index buffer creation
                {
                    vk::DeviceSize buffer_size = sizeof(mesh.indices.unique[0]) * mesh.indices.unique.size();

                    vk::Buffer staging_buffer;
                    vk::DeviceMemory staging_buffer_memory;
                    create_buffer(
                        buffer_size,
                        vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                        staging_buffer,
                        staging_buffer_memory
                    );
                    void* data = device.mapMemory(staging_buffer_memory, 0, buffer_size).value;
                    {
                        memcpy(data, mesh.indices.unique.data(), (size_t) buffer_size);
                    }
                    device.unmapMemory(staging_buffer_memory);

                    create_buffer(
                        buffer_size,
                        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                        vk::MemoryPropertyFlagBits::eDeviceLocal,
                        mesh.indices.buffer,
                        mesh.indices.memory
                    );
                    copy_buffer(staging_buffer, mesh.indices.buffer, buffer_size);
                    device.destroyBuffer(staging_buffer);
                    device.freeMemory(staging_buffer_memory);
                }
                model.meshes.push_back(mesh);
            }
            models.push_back(model);
        } else {
            throw std::runtime_error("Error:\n>>\tFailed to load model.");
        }
    }

    //_____________________________________
    // Create uniform buffers
    {
        vk::DeviceSize buffer_size = sizeof(UniformBufferObject);
        uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_buffers_memory.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            create_buffer(
                buffer_size,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                uniform_buffers[i],
                uniform_buffers_memory[i]
            );
            uniform_buffers_mapped[i] = device.mapMemory(uniform_buffers_memory[i], 0, buffer_size).value;
        }
    }

    //_____________________________________
    // Create descriptor pool
    {
        std::array<vk::DescriptorPoolSize, 2> pool_sizes {{
            { // [0]
                .type                = vk::DescriptorType::eUniformBuffer,
                .descriptorCount     = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        },  { // [1]
                .type                = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount     = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2,
        },  }};
        descriptor.pool = expect("Failed to create descriptor pool.",
            device.createDescriptorPool(
                vk::DescriptorPoolCreateInfo {
                    .flags              = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                    .maxSets            = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2,
                    .poolSizeCount      = static_cast<uint32_t>(pool_sizes.size()),
                    .pPoolSizes         = pool_sizes.data(),
        }   )   );
    }

    //_____________________________________
    // Create descriptor sets
    {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptor.uniform_set_layout);
        descriptor.sets = expect("Failed to allocate descriptor sets.",
            device.allocateDescriptorSets(
                vk::DescriptorSetAllocateInfo {
                    .descriptorPool      = descriptor.pool,
                    .descriptorSetCount  = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
                    .pSetLayouts         = layouts.data(),
        }   )   );
        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            vk::DescriptorBufferInfo buffer_info {
                .buffer          = uniform_buffers[i],
                .offset          = 0,
                .range           = sizeof(UniformBufferObject),
            };
            vk::DescriptorImageInfo image_info {
                .sampler         = textures[0].sampler,
                .imageView       = textures[0].image_view,
                .imageLayout     = vk::ImageLayout::eShaderReadOnlyOptimal,
            };
            std::array<vk::WriteDescriptorSet, 2> descriptor_writes {{
                { // [0]
                    .dstSet          = descriptor.sets[i],
                    .dstBinding      = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType  = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo     = &buffer_info,
            },  { // [1]
                    .dstSet          = descriptor.sets[i],
                    .dstBinding      = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo      = &image_info,
            },  }};
            device.updateDescriptorSets(descriptor_writes, 0);
        }
    }

    //_____________________________________
    // Create command buffers
    {
        command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        command_buffers = expect("Failed to allocate command buffer.",
            device.allocateCommandBuffers(
                vk::CommandBufferAllocateInfo   {
                    .commandPool        = command_pool,
                    .level              = vk::CommandBufferLevel::ePrimary,
                    .commandBufferCount = (uint32_t) command_buffers.size(),
        }   )  );
    }

    //_____________________________________
    // Create sync objects
    {
        img_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

        vk::SemaphoreCreateInfo semaphore_info {};
        vk::FenceCreateInfo fence_info {
            .flags = vk::FenceCreateFlagBits::eSignaled,
        };
        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            img_available_semaphores[i]     = device.createSemaphore(semaphore_info).value;
            render_finished_semaphores[i]   = device.createSemaphore(semaphore_info).value;
            in_flight_fences[i]             = device.createFence(fence_info).value;
        }
    }
    //_____________________________________
    // Initialize ImGui
    {
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window::handle, true);
        ImGui_ImplVulkan_InitInfo init_info {
            .Instance           = static_cast<VkInstance>(instance),
            .PhysicalDevice     = static_cast<VkPhysicalDevice>(physical_device),
            .Device             = static_cast<VkDevice>(device),
            .Queue              = static_cast<VkQueue>(graphics_queue),
            .DescriptorPool     = static_cast<VkDescriptorPool>(descriptor.pool),
            .MinImageCount      = MAX_FRAMES_IN_FLIGHT,
            .ImageCount         = MAX_FRAMES_IN_FLIGHT,
            .MSAASamples        = static_cast<VkSampleCountFlagBits>(msaa_samples),
        };
        ImGui_ImplVulkan_Init(&init_info, render_pass);

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.FontDefault = io.Fonts->AddFontFromFileTTF("fonts/NotoSansMono-Regular.ttf", 12.f);
        io.Fonts->Build();

        vk::CommandBuffer command_buffer = begin_single_time_commands();
        {
            ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
        }
        end_single_time_commands(command_buffer);
        if( device.waitIdle() != vk::Result::eSuccess ) {
            throw std::runtime_error("\n>>>\tError: Command buffer failed setting up ImGui.");
        }
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }
    //_____________________________________
    // Set ImGui style
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 8.f;
        style.FrameRounding = 8.f;
        style.ScrollbarRounding = 4.f;

        style.Colors[ImGuiCol_Text] = ImVec4(.6f, .6f, .6f, 1.f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.f, 0.f, 0.f, .2f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.f, 0.f, 0.f, 0.f);
    }
}

renderer::QueueFamilyIndices renderer::find_queue_families(vk::PhysicalDevice phys_device) {
    QueueFamilyIndices indices;
    for( int i = 0; const vk::QueueFamilyProperties& queue_family : phys_device.getQueueFamilyProperties() ) {
        if( queue_family.queueFlags & vk::QueueFlagBits::eGraphics ) { indices.graphics_family = i; }
        if( phys_device.getSurfaceSupportKHR(i, surface).value )     { indices.present_family  = i; }
        if( indices.is_complete() ) {  break;  } else {  i++;  };
    }
    return indices;
}

vk::SurfaceFormatKHR renderer::choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
    for( const vk::SurfaceFormatKHR& available_format : available_formats ) {
        if( available_format.format     == vk::Format::eB8G8R8A8Srgb
         && available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear ) {
            return available_format;
        }
    } return available_formats[0];
}

vk::PresentModeKHR renderer::choose_swap_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes) {
    for( const vk::PresentModeKHR& available_present_mode : available_present_modes ) {
        if( available_present_mode == vk::PresentModeKHR::eMailbox ) {
            return available_present_mode;
        }
    } return vk::PresentModeKHR::eFifo;
}

vk::Extent2D renderer::choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    if( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window::handle, &width, &height);
        
        vk::Extent2D actual_extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
        };
        actual_extent.width  = std::clamp(
                actual_extent.width,
                capabilities.minImageExtent.width,
                capabilities.minImageExtent.width
            );
        actual_extent.height = std::clamp(
                actual_extent.height,
                capabilities.minImageExtent.height,
                capabilities.minImageExtent.height
            );
        return actual_extent;
    }
}

void renderer::cleanup_swap_chain() {
    device.destroyImageView(color_img_view);
    device.destroyImage(color_img);
    device.freeMemory(color_img_memory);
    device.destroyImageView(depth_img_view);
    device.destroyImage(depth_img);
    device.freeMemory(depth_img_memory);
    for( auto frambuffer : swapchain.framebuffers ) {
        device.destroyFramebuffer(frambuffer);
    }
    for( auto img_view : swapchain.img_views ) {
        device.destroyImageView(img_view);
    }
    device.destroySwapchainKHR(swapchain.handle);
}

void renderer::create_swap_chain() {
    SwapChainSupportDetails swapchain_support {
        .capabilities   = physical_device.getSurfaceCapabilitiesKHR (surface).value,
        .formats        = physical_device.getSurfaceFormatsKHR      (surface).value,
        .present_modes  = physical_device.getSurfacePresentModesKHR (surface).value,
    };
    vk::SurfaceFormatKHR surface_format = choose_swap_surface_format(swapchain_support.formats      );
    vk::PresentModeKHR present_mode     = choose_swap_present_mode  (swapchain_support.present_modes);
    vk::Extent2D extent                 = choose_swap_extent        (swapchain_support.capabilities );

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if( swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount ) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }
    vk::SwapchainCreateInfoKHR swapchain_info {
        .surface            = surface,
        .minImageCount      = image_count,
        .imageFormat        = surface_format.format,
        .imageColorSpace    = surface_format.colorSpace,
        .imageExtent        = extent,
        .imageArrayLayers   = 1,
        .imageUsage         = vk::ImageUsageFlagBits::eColorAttachment,
        .preTransform       = swapchain_support.capabilities.currentTransform,
        .compositeAlpha     = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode        = present_mode,
        .clipped            = true,
    };
    QueueFamilyIndices indices = find_queue_families(physical_device);
    uint32_t queue_family_indices[] = {
        indices.graphics_family.value(),
        indices.present_family.value()
    };
    if( indices.graphics_family != indices.present_family ) {
        swapchain_info.imageSharingMode        = vk::SharingMode::eConcurrent;
        swapchain_info.queueFamilyIndexCount   = 2;
        swapchain_info.pQueueFamilyIndices     = queue_family_indices;
    } else {
        swapchain_info.imageSharingMode        = vk::SharingMode::eExclusive;
    }
    swapchain.handle = expect("Failed to create swap chain.",
        device.createSwapchainKHR(swapchain_info)
    );
    swapchain.images = expect("Failed to get swapchain images.",
        device.getSwapchainImagesKHR(swapchain.handle)
    );
    swapchain.img_format = surface_format.format;
    swapchain.extent     = extent;
}

void renderer::recreate_swap_chain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window::handle, &width, &height);
    while( width == 0 || height == 0 ) {
        glfwGetFramebufferSize(window::handle, &width, &height);
        glfwWaitEvents();
    }
    expect("Device wait idle failed.", device.waitIdle());
    cleanup_swap_chain();

    create_swap_chain();
    create_img_views();
    create_color_resources();
    create_depth_resources();
    create_framebuffers();
}

vk::ImageView renderer::create_img_view(vk::Image image,
                                        vk::Format format,
                                        vk::ImageAspectFlags aspect_flags,
                                        uint32_t mip_levels
) {
    return expect("Failed to create image view.", 
        device.createImageView(
            vk::ImageViewCreateInfo {
                .image      = image,
                .viewType   = vk::ImageViewType::e2D,
                .format     = format,
                .subresourceRange {
                    .aspectMask     = aspect_flags,
                    .baseMipLevel   = 0,
                    .levelCount     = mip_levels,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
    }   )   );
}

void renderer::create_img_views() {
    swapchain.img_views.resize(swapchain.images.size());

    for( uint32_t i = 0; i < swapchain.images.size(); i++ ) {
        swapchain.img_views[i] = create_img_view(
                                    swapchain.images[i],
                                    swapchain.img_format,
                                    vk::ImageAspectFlagBits::eColor,
                                    1
                                );
    }
}

std::vector<char> renderer::read_file(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if( !file.is_open() ) {
        throw std::runtime_error("\tError: Failed to open file at:" + path);
    }
    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();
    return buffer;
}

vk::ShaderModule renderer::create_shader_module(const std::vector<char>& code) {
    return expect("Failed to create shader module.",
        device.createShaderModule(
            vk::ShaderModuleCreateInfo {
                .codeSize = code.size(),
                .pCode    = reinterpret_cast<const uint32_t*>(code.data()),
    }   )   );
}

void renderer::create_buffer(vk::DeviceSize size,
                             vk::BufferUsageFlags usage,
                             vk::MemoryPropertyFlags properties,
                             vk::Buffer& buffer,
                             vk::DeviceMemory& buffer_memory
) {
    buffer = expect("Could not create buffer.",
        device.createBuffer(vk::BufferCreateInfo {
            .size        = size,
            .usage       = usage,
            .sharingMode = vk::SharingMode::eExclusive,
    }   )  );
    vk::MemoryRequirements mem_requirements = device.getBufferMemoryRequirements(buffer);
    buffer_memory = expect("Failed to allocate buffer memory.",
        device.allocateMemory(
            vk::MemoryAllocateInfo {
                .allocationSize  = mem_requirements.size,
                .memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties),
    }   )   );
    vkBindBufferMemory(device, buffer, buffer_memory, 0);
}

void renderer::update_uniform_buffer(uint32_t current_image) {
    UniformBufferObject ubo {
        .model = glm::rotate(glm::mat4(1.f), glm::radians(45.f), glm::vec3(0.f, 0.f, 1.f)),
        .view  = camera.get_view(),
        .proj  = glm::perspective(
                glm::radians(camera.fov),
                swapchain.extent.width/(float)swapchain.extent.height,
                camera.clip_plane.near,
                camera.clip_plane.far
            ),
    };
    ubo.proj[1][1] *= -1;
    memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
}

void renderer::copy_buffer(vk::Buffer src_buffer,
                           vk::Buffer dst_buffer,
                           vk::DeviceSize size
) {
    vk::CommandBuffer command_buffer = begin_single_time_commands();
    {
        command_buffer.copyBuffer(
                src_buffer,
                dst_buffer,
                vk::BufferCopy {
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size   = size,
                }
            );
    }
    end_single_time_commands(command_buffer);
}

uint32_t renderer::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties mem_properties = physical_device.getMemoryProperties();
    for( uint32_t i = 0; i < mem_properties.memoryTypeCount; i++ ) {
        if( type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
            return i;
        }
    }
    throw std::runtime_error("\tError: Failed to find suitable memory type.");
}

void renderer::create_img(uint32_t width,
                          uint32_t height,
                          uint32_t mip_levels,
                          vk::SampleCountFlagBits num_samples,
                          vk::Format format,
                          vk::ImageTiling tiling,
                          vk::ImageUsageFlags usage,
                          vk::MemoryPropertyFlags properties,
                          vk::Image& image,
                          vk::DeviceMemory& image_memory
) {
    image = expect("Failed to create image.",
        device.createImage(
            vk::ImageCreateInfo {
                .imageType     = vk::ImageType::e2D,
                .format        = format,
                .extent {
                    .width         = width,
                    .height        = height,
                    .depth         = 1,
                },
                .mipLevels     = mip_levels,
                .arrayLayers   = 1,
                .samples       = num_samples,
                .tiling        = tiling,
                .usage         = usage,
                .sharingMode   = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined,
    }   )   );
    vk::MemoryRequirements mem_requirements = device.getImageMemoryRequirements(image);
    image_memory = expect("Failed to allocate memory.",
        device.allocateMemory(
            vk::MemoryAllocateInfo {
                .allocationSize  = mem_requirements.size,
                .memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties),
    }   )  );
    expect("Failed to bind image memory.",
        device.bindImageMemory(image, image_memory, 0)
    );
}

void renderer::transition_img_layout(vk::Image img,
                                     vk::Format format,
                                     vk::ImageLayout old_layout,
                                     vk::ImageLayout new_layout,
                                     uint32_t mip_levels
) {
    vk::CommandBuffer command_buffer = begin_single_time_commands();
    {
        vk::ImageMemoryBarrier barrier {
            .oldLayout           = old_layout,
            .newLayout           = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = img,
            .subresourceRange {
                .aspectMask          = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel        = 0,
                .levelCount          = mip_levels,
                .baseArrayLayer      = 0,
                .layerCount          = 1,
            },
        };
        vk::PipelineStageFlags source_stage;
        vk::PipelineStageFlags destination_stage;
        if( old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal ) {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            source_stage          = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage     = vk::PipelineStageFlagBits::eTransfer;
        } else if( old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal ) {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            source_stage          = vk::PipelineStageFlagBits::eTransfer;
            destination_stage     = vk::PipelineStageFlagBits::eFragmentShader;
        } else {
            throw std::runtime_error("\tError: Unsupported layout transition.");
        }
        command_buffer.pipelineBarrier(
            source_stage, destination_stage,
            vk::DependencyFlags(),
            nullptr,
            nullptr,
            barrier
        );
    }
    end_single_time_commands(command_buffer);
}

void renderer::copy_buffer_to_img(vk::Buffer buffer,
                                  vk::Image img,
                                  uint32_t width,
                                  uint32_t height
) {
    vk::CommandBuffer command_buffer = begin_single_time_commands();
    {
        vk::BufferImageCopy region {
            .bufferOffset       = 0,
            .bufferRowLength    = 0,
            .bufferImageHeight  = 0,
            .imageSubresource {
                .aspectMask         = vk::ImageAspectFlagBits::eColor,
                .mipLevel           = 0,
                .baseArrayLayer     = 0,
                .layerCount         = 1,
            },
            .imageOffset        = { 0, 0, 0 },
            .imageExtent        = { width, height, 1 },
        };
        command_buffer.copyBufferToImage(
            buffer,
            img,
            vk::ImageLayout::eTransferDstOptimal,
            region
        );
    }
    end_single_time_commands(command_buffer);
}

void renderer::generate_mipmaps(vk::Image image,
                                vk::Format image_format,
                                int32_t t_width,
                                int32_t t_height,
                                uint32_t mip_levels
) {
    vk::FormatProperties format_properties = physical_device.getFormatProperties(image_format);
    if( !(format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear) ) {
        throw std::runtime_error("\tError: Texture image format does not support linear blitting.");
    }
    vk::CommandBuffer command_buffer = begin_single_time_commands();
    {
        vk::ImageMemoryBarrier barrier {
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = image,
            .subresourceRange {
                .aspectMask         = vk::ImageAspectFlagBits::eColor,
                .levelCount         = 1,
                .baseArrayLayer     = 0,
                .layerCount         = 1,
            },
        };
        int32_t mip_w = t_width;
        int32_t mip_h = t_height;
        for( uint32_t i = 1; i < mip_levels; i++ ) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
            barrier.newLayout                     = vk::ImageLayout::eTransferSrcOptimal;
            barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferRead;
            barrier.dstAccessMask                 = vk::AccessFlagBits::eTransferWrite;
            command_buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                    vk::DependencyFlags(),
                    nullptr,
                    nullptr,
                    barrier
                );
            vk::ImageBlit blit {
                .srcSubresource {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .mipLevel       = i - 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
                .srcOffsets = {{
                    vk::Offset3D({ 0,     0,     0 }),
                    vk::Offset3D({ mip_w, mip_h, 1 }),
                }},
                .dstSubresource {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .mipLevel       = i,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
                .dstOffsets = {{
                    vk::Offset3D({ 0,                         0,                         0 }),
                    vk::Offset3D({ mip_w > 1 ? mip_w / 2 : 1, mip_h > 1 ? mip_h / 2 : 1, 1 }),
                }},
            };
            command_buffer.blitImage(
                    image, vk::ImageLayout::eTransferSrcOptimal,
                    image, vk::ImageLayout::eTransferDstOptimal,
                    blit,
                    vk::Filter::eLinear
                );
            barrier.oldLayout                 = vk::ImageLayout::eTransferSrcOptimal;
            barrier.newLayout                 = vk::ImageLayout::eShaderReadOnlyOptimal;
            barrier.srcAccessMask             = vk::AccessFlagBits::eTransferRead;
            barrier.dstAccessMask             = vk::AccessFlagBits::eShaderRead;
            command_buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                    vk::DependencyFlags(),
                    nullptr,
                    nullptr,
                    barrier
                );
            if( mip_w > 1 ) { mip_w /= 2; }
            if( mip_h > 1 ) { mip_h /= 2; }
        }
        barrier.subresourceRange.baseMipLevel = mip_levels - 1;
        barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout                     = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask                 = vk::AccessFlagBits::eShaderRead;
        command_buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                vk::DependencyFlags(),
                nullptr,
                nullptr,
                barrier
            );
    }
    end_single_time_commands(command_buffer);
}

void renderer::create_depth_resources() {
    vk::Format depth_format = find_depth_format();
    create_img(
        swapchain.extent.width,
        swapchain.extent.height,
        1,
        msaa_samples,
        depth_format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        depth_img,
        depth_img_memory
    );
    depth_img_view = create_img_view(
                            depth_img,
                            depth_format,
                            vk::ImageAspectFlagBits::eDepth,
                            1
                        );
}

vk::Format renderer::find_supported_format(const std::vector<vk::Format>& candidates,
                                           vk::ImageTiling tiling,
                                           vk::FormatFeatureFlags features
) {
    for( vk::Format format : candidates ) {
        vk::FormatProperties props = physical_device.getFormatProperties(format);
        if( tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features ) {
            return format;
        } else if( tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) ) {
            return format;
        }
    }
    throw std::runtime_error("\tError: Failed to find supported format.");
}

bool renderer::has_stencil_component(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint
        || format == vk::Format::eD24UnormS8Uint;
}

vk::Format renderer::find_depth_format() {
    return find_supported_format(
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

void renderer::create_color_resources() {
    vk::Format color_format = swapchain.img_format;
    create_img(
        swapchain.extent.width,
        swapchain.extent.height,
        1,
        msaa_samples,
        color_format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        color_img,
        color_img_memory
    );
    color_img_view = create_img_view(
        color_img,
        color_format,
        vk::ImageAspectFlagBits::eColor,
        1
    );
}

vk::CommandBuffer renderer::begin_single_time_commands() {
    vk::CommandBufferAllocateInfo alloc_info {
        .commandPool        = command_pool,
        .level              = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    vk::CommandBuffer command_buffer = device.allocateCommandBuffers(alloc_info).value.front();
    vk::CommandBufferBeginInfo begin_info {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };
    expect("Failed to begin command buffer.",
        command_buffer.begin(begin_info)
    );
    return command_buffer;
}

void renderer::end_single_time_commands(vk::CommandBuffer command_buffer) {
    expect("Failed to end command buffer.",
        command_buffer.end()
    );
    vk::SubmitInfo submit_info {
        .commandBufferCount = 1,
        .pCommandBuffers    = &command_buffer,
    };
    expect("Failed to sumbit graphics queue.",
        graphics_queue.submit(submit_info)
    );
    expect("Graphics queue wait idle failed.",
        graphics_queue.waitIdle()
    );
    device.freeCommandBuffers(command_pool, command_buffer);
}

void renderer::create_framebuffers() {
    swapchain.framebuffers.resize(swapchain.img_views.size());
    for( size_t i = 0; i < swapchain.img_views.size(); i++ ) {
        std::array<vk::ImageView, 3> attachments = {
            color_img_view,
            depth_img_view,
            swapchain.img_views[i],
        };
        vk::FramebufferCreateInfo framebuffer_info {
            .renderPass      = render_pass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments    = attachments.data(),
            .width           = swapchain.extent.width,
            .height          = swapchain.extent.height,
            .layers          = 1,
        };
        swapchain.framebuffers[i] = expect("Failed to create framebuffer.",
            device.createFramebuffer(framebuffer_info)
        );
    }
}

void renderer::record_command_buffer(vk::CommandBuffer command_buffer, uint32_t img_index) {
    vk::CommandBufferBeginInfo begin_info {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };
    expect("Failed to begin recording command buffer.",
        command_buffer.begin(begin_info)
    );
    std::array<vk::ClearValue, 2> clear_values {{
        { .color         = {{{ .015f, .015f, .015f, 1.f }}} }, // [0]
        { .depthStencil  = { 1.f, 0 } }                        // [1]
    }};
    vk::RenderPassBeginInfo render_pass_info {
        .renderPass      = render_pass,
        .framebuffer     = swapchain.framebuffers[img_index],
        .renderArea {
            .offset         = { 0, 0 },
            .extent         = swapchain.extent,
        },
        .clearValueCount = static_cast<uint32_t>(clear_values.size()),
        .pClearValues    = clear_values.data(),
    };
    command_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
    {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
        vk::Viewport viewport {
            .x        = 0.f,
            .y        = 0.f,
            .width    = (float) swapchain.extent.width,
            .height   = (float) swapchain.extent.height,
            .minDepth = 0.f,
            .maxDepth = 1.f,
        };
        command_buffer.setViewport(0, viewport);
        vk::Rect2D scissor {
            .offset   = { 0, 0 },
            .extent   = swapchain.extent,
        };
        command_buffer.setScissor(0, scissor);

        std::vector<vk::Buffer> vertex_buffers;
        std::vector<vk::DeviceSize> offsets = { 0 };
        for( auto model : models ) {
            for( auto mesh : model.meshes ) {
                vertex_buffers.push_back(mesh.vertices.buffer);
                command_buffer.bindVertexBuffers(0, vertex_buffers, offsets);
                //offsets.push_back(mesh.vertices.pos.size());
            }
        }
        for( auto model : models ) {
            for( auto mesh : model.meshes ) {
                command_buffer.bindIndexBuffer(mesh.indices.buffer, 0, vk::IndexType::eUint32);
            }
        }
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptor.sets[current_frame], nullptr);
        for( auto model : models ) {
            for( auto mesh : model.meshes ) {
                command_buffer.drawIndexed(static_cast<uint32_t>(mesh.indices.unique.size()), 1, 0, 0, 0);
            }
        }
        //____________________________________
        // ImGui specific updates
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("Sigil", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(std::format("GPU: {}", physical_device.getProperties().deviceName.data()).c_str());
                ImGui::TextUnformatted(std::format("sigil   {}", sigil::version::as_string).c_str());
                ImGui::SetWindowPos(ImVec2(0, swapchain.extent.height - ImGui::GetWindowSize().y));
            }
            ImGui::End();
            ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoTitleBar   //| ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(
                    std::format(" Camera position:\n\tx: {:.3f}\n\ty: {:.3f}\n\tz: {:.3f}",
                    camera.transform.position.x, camera.transform.position.y, camera.transform.position.z).c_str()
                );
                ImGui::TextUnformatted(
                    std::format(" Yaw:   {:.2f}\n Pitch: {:.2f}",
                    camera.yaw, camera.pitch).c_str()
                );
                ImGui::TextUnformatted(
                    std::format(" Mouse position:\n\tx: {:.0f}\n\ty: {:.0f}",
                    input::mouse_position.x, input::mouse_position.y ).c_str()
                );
                ImGui::TextUnformatted(
                    std::format(" Mouse offset:\n\tx: {:.0f}\n\ty: {:.0f}",
                    input::get_mouse_movement().x, input::get_mouse_movement().y).c_str()
                );
                ImGui::SetWindowSize(ImVec2(108, 182));
                ImGui::SetWindowPos(ImVec2(8, 8));
            }
            ImGui::End();
            ImGui::Begin("Framerate", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(std::format(" FPS: {:.0f}", time::fps).c_str());
                ImGui::TextUnformatted(std::format(" ms: {:.2f}", time::ms).c_str());
                ImGui::SetWindowSize(ImVec2(82, 64));
                ImGui::SetWindowPos(ImVec2(swapchain.extent.width - ImGui::GetWindowSize().x, 8));
            }
            ImGui::End();
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
        }
    }
    command_buffer.endRenderPass();
    expect("Failed to record command buffer.",
        command_buffer.end()
    );
}

void renderer::draw() {
    expect("Wait for fences failed.",
        device.waitForFences(in_flight_fences, true, UINT64_MAX)
    );
    vk::ResultValue<uint32_t> next_img = device.acquireNextImageKHR(swapchain.handle, UINT64_MAX, img_available_semaphores[current_frame]);
    uint32_t img_index = next_img.value;
    if( next_img.result == vk::Result::eErrorOutOfDateKHR ) {
        recreate_swap_chain();
        return;
    } else if ( next_img.result != vk::Result::eSuccess && next_img.result != vk::Result::eSuboptimalKHR ) {
        throw std::runtime_error("\tError: Failed to acquire swap chain image.");
    }
    camera.update(time::delta_time);
    update_uniform_buffer(current_frame);
    record_command_buffer(command_buffers[current_frame], img_index);
    device.resetFences(in_flight_fences[current_frame]);
    //command_buffers[current_frame].reset();

    vk::Semaphore wait_semaphores[]      = { img_available_semaphores[current_frame]           };
    vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    vk::Semaphore signal_semaphores[]    = { render_finished_semaphores[current_frame]         };

    vk::SubmitInfo submit_info {
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = wait_semaphores,
        .pWaitDstStageMask    = wait_stages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &command_buffers[current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = signal_semaphores,
    };
    expect("Failed to submit draw command buffer.",
        graphics_queue.submit(submit_info, in_flight_fences[current_frame])
    );
    vk::SwapchainKHR swap_chains[] = { swapchain.handle };
    vk::PresentInfoKHR present_info {
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = signal_semaphores,
        .swapchainCount       = 1,
        .pSwapchains          = swap_chains,
        .pImageIndices        = &img_index,
    };
    vk::Result result = present_queue.presentKHR(present_info);
    if( result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || window::resized ) {
        window::resized = false;
        recreate_swap_chain();
    } else if ( result != vk::Result::eSuccess ) {
        throw std::runtime_error("\tError: Failed to present swap chain image.");
    }
    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

#ifdef _DEBUG
VkResult renderer::create_debug_util_messenger_ext(VkInstance instance,
                                                   const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
                                                   const VkAllocationCallbacks* p_allocator,
                                                   VkDebugUtilsMessengerEXT* p_debug_messenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if( func != nullptr ) {
        return func(instance, p_create_info, p_allocator, p_debug_messenger);
    } else { return VK_ERROR_EXTENSION_NOT_PRESENT; }
}

void renderer::destroy_debug_util_messenger_ext(VkInstance instance,
                                                VkDebugUtilsMessengerEXT p_debug_messenger,
                                                const VkAllocationCallbacks* p_allocator
) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if( func != nullptr ) {
        return func(instance, p_debug_messenger, p_allocator);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL renderer::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                                                        VkDebugUtilsMessageTypeFlagsEXT msg_type,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                                        void* p_user_data
) {
    std::cerr << "Validation layer: " << p_callback_data->pMessage << "\n";
    return false;
}
#endif

void renderer::terminate() {
    expect("Device wait idle failed.", device.waitIdle());
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    cleanup_swap_chain();
    device.destroyPipeline(graphics_pipeline);
    device.destroyPipelineLayout(pipeline_layout);
    device.destroyRenderPass(render_pass);
    for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        device.destroyBuffer(uniform_buffers[i]);
        device.freeMemory(uniform_buffers_memory[i]);
    }
    device.destroyDescriptorPool(descriptor.pool);
    for( auto& texture : textures ) {
        device.destroySampler(texture.sampler);
        device.destroyImageView(texture.image_view);
        device.destroyImage(texture.image);
        device.freeMemory(texture.image_memory);
    }
    device.destroyDescriptorSetLayout(descriptor.uniform_set_layout);
    device.destroyDescriptorSetLayout(descriptor.texture_set_layout);
    
    for( auto model : models ) {
        for( auto mesh : model.meshes ) {
            device.destroyBuffer(mesh.indices.buffer);
            device.freeMemory(mesh.indices.memory);
            device.destroyBuffer(mesh.vertices.buffer);
            device.freeMemory(mesh.vertices.memory);
        }
    }

    for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        device.destroySemaphore(img_available_semaphores[i]);
        device.destroySemaphore(render_finished_semaphores[i]);
        device.destroyFence(in_flight_fences[i]);
    }
    device.destroyCommandPool(command_pool);
    device.destroy();
#ifdef _DEBUG
    instance.destroyDebugUtilsMessengerEXT(debug_messenger);
#endif
    instance.destroySurfaceKHR(surface);
    instance.destroy();
}

