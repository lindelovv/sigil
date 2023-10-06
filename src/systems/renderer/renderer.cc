#include "renderer.hh"
#include "system.hh"

#include <GLFW/glfw3.h>
#include <cstdint>
#include <fstream>
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
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobj.h"

namespace sigil {

    Renderer renderer;

    void Renderer::init() {
        can_tick = true;
        for( auto& system : core->systems ) {
            if( Glfw* glfw = dynamic_cast<Glfw*>(system.get()) ) {
                window = glfw->window;
                break;
            }
        }
        // done
        create_instance();
        expect("Failed to create main window surface.",
            glfwCreateWindowSurface(instance, window, nullptr, (VkSurfaceKHR*)&surface)
        );
        select_physical_device();
        create_logical_device();
        create_swap_chain();
        create_img_views();

        // rewriting
        create_render_pass();

        // old / todo
        create_descriptor_set_layout();
        create_graphics_pipeline();
        create_command_pool();
        create_color_resources();
        create_depth_resources();
        create_framebuffers();
        create_texture_img();
        create_texture_img_view();
        create_texture_sampler();
        load_model(); // replace with own .obj loading code
        create_vertex_buffer();
        create_index_buffer();
        create_uniform_buffers();
        create_descriptor_pool();
        create_descriptor_sets();
        create_command_buffers();
        create_sync_objects();
    }

    void Renderer::tick() {
        draw();
    }

    void Renderer::create_instance() {
#ifdef DEBUG
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
        auto debug_info = expect("Failed to create debug utils messenger.",
            instance.createDebugUtilsMessengerEXT(
                vk::DebugUtilsMessengerCreateInfoEXT {
                    .sType           = vk::StructureType::eDebugUtilsMessengerCreateInfoEXT,
                    .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                      | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                      | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                      | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                    .messageType     = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                      | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                                      | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
                    .pfnUserCallback = debug_callback,
                },
                nullptr,
                &debug_messenger
        )   );
#endif
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
#ifdef DEBUG
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        instance = expect("Failed to create instance.",
            vk::createInstance(
                vk::InstanceCreateInfo {
#ifdef DEBUG
                    .pNext                      = &debug_info,
                    .pApplicationInfo           = &engine_info,
                    .enabledLayerCount          = static_cast<uint32_t>(validation_layers.size()),
                    .ppEnabledLayerNames        = validation_layers.data(),
                    .enabledExtensionCount      = static_cast<uint32_t>(extensions.size()),
                    .ppEnabledExtensionNames    = extensions.data(),
#else 
                    .pNext                      = nullptr,
                    .pApplicationInfo           = &engine_info,
                    .enabledLayerCount          = 0,
                    .enabledExtensionCount      = static_cast<uint32_t>(extensions.size()),
                    .ppEnabledExtensionNames    = extensions.data(),
#endif
        }   )   );
    }
    
    void Renderer::select_physical_device() {
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

    void Renderer::create_logical_device() {
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
            .sampleRateShading              = vk::True,
            .samplerAnisotropy              = vk::True,
        };
        vk::DeviceCreateInfo device_info {
            .queueCreateInfoCount           = static_cast<uint32_t>(queue_info_vec.size()),
            .pQueueCreateInfos              = queue_info_vec.data(),
#ifdef DEBUG
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

    QueueFamilyIndices Renderer::find_queue_families(vk::PhysicalDevice phys_device) {
        QueueFamilyIndices indices;
        for( int i = 0; const vk::QueueFamilyProperties& queue_family : phys_device.getQueueFamilyProperties() ) {
            if( queue_family.queueFlags & vk::QueueFlagBits::eGraphics ) { indices.graphics_family = i; }
            if( phys_device.getSurfaceSupportKHR(i, surface).value )     { indices.present_family  = i; }
            if( indices.is_complete() ) {  break;  } else {  i++;  };
        }
        return indices;
    }

    void Renderer::create_swap_chain() {
        SwapChainSupportDetails swap_chain_support {
            .capabilities   = physical_device.getSurfaceCapabilitiesKHR (surface).value,
            .formats        = physical_device.getSurfaceFormatsKHR      (surface).value,
            .present_modes  = physical_device.getSurfacePresentModesKHR (surface).value,
        };
        vk::SurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats      );
        vk::PresentModeKHR present_mode     = choose_swap_present_mode  (swap_chain_support.present_modes);
        vk::Extent2D extent                 = choose_swap_extent        (swap_chain_support.capabilities );

        uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
        if( swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount ) {
            image_count = swap_chain_support.capabilities.maxImageCount;
        }
        vk::SwapchainCreateInfoKHR swapchain_info {
            .surface            = surface,
            .minImageCount      = image_count,
            .imageFormat        = surface_format.format,
            .imageColorSpace    = surface_format.colorSpace,
            .imageExtent        = extent,
            .imageArrayLayers   = 1,
            .imageUsage         = vk::ImageUsageFlagBits::eColorAttachment,
            .preTransform       = swap_chain_support.capabilities.currentTransform,
            .compositeAlpha     = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode        = present_mode,
            .clipped            = vk::True,
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
        swap_chain = expect("Failed to create swap chain.",
            device.createSwapchainKHR(swapchain_info)
        );
        swap_chain_images = expect("Failed to get swapchain images.",
            device.getSwapchainImagesKHR(swap_chain)
        );
        swap_chain_img_format = surface_format.format;
        swap_chain_extent     = extent;
    }

    vk::SurfaceFormatKHR Renderer::choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
        for( const vk::SurfaceFormatKHR& available_format : available_formats ) {
            if( available_format.format     == vk::Format::eB8G8R8A8Srgb
             && available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear ) {
                return available_format;
            }
        } return available_formats[0];
    }

    vk::PresentModeKHR Renderer::choose_swap_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes) {
        for( const vk::PresentModeKHR& available_present_mode : available_present_modes ) {
            if( available_present_mode == vk::PresentModeKHR::eMailbox ) {
                return available_present_mode;
            }
        } return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D Renderer::choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            
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

    void Renderer::cleanup_swap_chain() {
        device.destroyImageView(color_img_view);
        device.destroyImage(color_img);
        device.freeMemory(color_img_memory);
        device.destroyImageView(depth_img_view);
        device.destroyImage(depth_img);
        device.freeMemory(depth_img_memory);
        for( auto frambuffer : swap_chain_framebuffers ) {
            device.destroyFramebuffer(frambuffer);
        }
        for( auto img_view : swap_chain_img_views ) {
            device.destroyImageView(img_view);
        }
        device.destroySwapchainKHR(swap_chain);
    }

    void Renderer::recreate_swap_chain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while( width == 0 || height == 0 ) {
            glfwGetFramebufferSize(window, &width, &height);
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

    vk::ImageView Renderer::create_img_view(vk::Image image, vk::Format format, vk::ImageAspectFlags aspect_flags, uint32_t mip_levels) {
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
                }
            )
        );
    }

    void Renderer::create_img_views() {
        swap_chain_img_views.resize(swap_chain_images.size());

        for( uint32_t i = 0; i < swap_chain_images.size(); i++ ) {
            swap_chain_img_views[i] = create_img_view(
                                        swap_chain_images[i],
                                        swap_chain_img_format,
                                        vk::ImageAspectFlagBits::eColor,
                                        1
                                    );
        }
    }

    std::vector<char> Renderer::read_file(const std::string& path) {
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

    void Renderer::create_graphics_pipeline() {
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
            .primitiveRestartEnable  = vk::False,
        };
        vk::PipelineViewportStateCreateInfo viewport_state {
            .viewportCount           = 1,
            .scissorCount            = 1,
        };
        vk::PipelineRasterizationStateCreateInfo rasterizer {
            .depthClampEnable        = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode             = vk::PolygonMode::eFill,
            .cullMode                = vk::CullModeFlagBits::eBack,
            .frontFace               = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable         = vk::False,
            .lineWidth               = 1.f,
        };
        vk::PipelineMultisampleStateCreateInfo multisampling {
            .rasterizationSamples    = msaa_samples,
            .sampleShadingEnable     = vk::True,
            .minSampleShading        = .2f,
        };
        vk::PipelineDepthStencilStateCreateInfo depth_stencil {
            .depthTestEnable         = vk::True,
            .depthWriteEnable        = vk::True,
            .depthCompareOp          = vk::CompareOp::eLess,
            .depthBoundsTestEnable   = vk::False,
            .stencilTestEnable       = vk::False,
        };
        vk::PipelineColorBlendAttachmentState color_blend_attachment {
            .blendEnable             = vk::False,
            .colorWriteMask          = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                     | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };
        vk::PipelineColorBlendStateCreateInfo color_blending {
            .logicOpEnable           = vk::False,
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
                    .pSetLayouts = &descriptor_set_layout,
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
    
    vk::ShaderModule Renderer::create_shader_module(const std::vector<char>& code) {
        return expect("Failed to create shader module.",
            device.createShaderModule(
                vk::ShaderModuleCreateInfo {
                    .codeSize = code.size(),
                    .pCode    = reinterpret_cast<const uint32_t*>(code.data()),
        }   )   );
    }

    void Renderer::create_descriptor_set_layout() {
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
        descriptor_set_layout = expect("Failed to create descriptor set layout.",
            device.createDescriptorSetLayout(layout_info)
        );
    }

    void Renderer::create_descriptor_pool() {
        std::array<vk::DescriptorPoolSize, 2> pool_sizes {{
            { // [0]
                .type                = vk::DescriptorType::eUniformBuffer,
                .descriptorCount     = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        },  { // [1]
                .type                = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount     = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        },  }};
        descriptor_pool = expect("Failed to create descriptor pool.",
            device.createDescriptorPool(
                vk::DescriptorPoolCreateInfo {
                    .maxSets             = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
                    .poolSizeCount       = static_cast<uint32_t>(pool_sizes.size()),
                    .pPoolSizes          = pool_sizes.data(),
        }   )   );
    }

    void Renderer::create_descriptor_sets() {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptor_set_layout);
        descriptor_sets = expect("Failed to allocate descriptor sets.",
            device.allocateDescriptorSets(
                vk::DescriptorSetAllocateInfo {
                    .descriptorPool      = descriptor_pool,
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
                .sampler         = texture_sampler,
                .imageView       = texture_image_view,
                .imageLayout     = vk::ImageLayout::eShaderReadOnlyOptimal,
            };
            std::array<vk::WriteDescriptorSet, 2> descriptor_writes {{
                { // [0]
                    .dstSet          = descriptor_sets[i],
                    .dstBinding      = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType  = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo     = &buffer_info,
            },  { // [1]
                    .dstSet          = descriptor_sets[i],
                    .dstBinding      = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo      = &image_info,
            },  }};
            device.updateDescriptorSets(descriptor_writes, 0);
        }
    }

    void Renderer::create_buffer(
            vk::DeviceSize size,
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

    void Renderer::create_vertex_buffer() {
        vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

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
            memcpy(data, vertices.data(), (size_t) buffer_size);
        }
        device.unmapMemory(staging_buffer_memory);

        create_buffer(
            buffer_size,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vertex_buffer,
            vertex_buffer_memory
            );
        copy_buffer(staging_buffer, vertex_buffer, buffer_size);
        device.destroyBuffer(staging_buffer);
        device.freeMemory(staging_buffer_memory);
    }

    void Renderer::create_index_buffer() {
        vk::DeviceSize buffer_size = sizeof(indices[0]) * indices.size();

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
            memcpy(data, indices.data(), (size_t) buffer_size);
        }
        device.unmapMemory(staging_buffer_memory);

        create_buffer(
            buffer_size,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            index_buffer,
            index_buffer_memory
            );
        copy_buffer(staging_buffer, index_buffer, buffer_size);
        device.destroyBuffer(staging_buffer);
        device.freeMemory(staging_buffer_memory);
    }

    void Renderer::create_uniform_buffers() {
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

    void Renderer::update_uniform_buffer(uint32_t current_image) {
        static auto start_time = std::chrono::high_resolution_clock::now();
        auto current_time = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
        UniformBufferObject ubo {
            .model = glm::rotate(glm::mat4(1.f), time * glm::radians(45.f), glm::vec3(0.f, 0.f, 1.f)),
            .view  = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f)),
            .proj  = glm::perspective(glm::radians(45.f), swap_chain_extent.width / (float) swap_chain_extent.height, 0.1f, 256.f),
        };
        ubo.proj[1][1] *= -1;
        memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
    }

    void Renderer::copy_buffer(vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size) {
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

    uint32_t Renderer::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties mem_properties = physical_device.getMemoryProperties();
        for( uint32_t i = 0; i < mem_properties.memoryTypeCount; i++ ) {
            if( type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
                return i;
            }
        }
        throw std::runtime_error("\tError: Failed to find suitable memory type.");
    }

    void Renderer::create_texture_img() {
        int t_width, t_height, t_channels;
        stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &t_width, &t_height, &t_channels, STBI_rgb_alpha);
        vk::DeviceSize img_size = t_width * t_height * 4;
        expect("Failed to load texture image.", pixels);

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
        mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(t_width, t_height)))) + 1;

        create_img(
            t_width, 
            t_height, 
            mip_levels,
            vk::SampleCountFlagBits::e1,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            texture_image,
            texture_image_memory
        );
        transition_img_layout(
            texture_image,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            mip_levels
        );
        copy_buffer_to_img(
            staging_buffer,
            texture_image,
            static_cast<uint32_t>(t_width),
            static_cast<uint32_t>(t_width)
        );
        device.destroyBuffer(staging_buffer);
        device.freeMemory(staging_buffer_memory);
        generate_mipmaps(texture_image, vk::Format::eR8G8B8A8Srgb, t_width, t_height, mip_levels);
    }

    void Renderer::create_texture_img_view() {
        texture_image_view = create_img_view(
                                texture_image,
                                vk::Format::eR8G8B8A8Srgb,
                                vk::ImageAspectFlagBits::eColor,
                                mip_levels
                            );
    }

    void Renderer::create_texture_sampler() {
        texture_sampler = expect("Failed to create texture sampler.",
            device.createSampler(
                vk::SamplerCreateInfo {
                    .magFilter               = vk::Filter::eLinear,
                    .minFilter               = vk::Filter::eLinear,
                    .mipmapMode              = vk::SamplerMipmapMode::eLinear,
                    .addressModeU            = vk::SamplerAddressMode::eRepeat,
                    .addressModeV            = vk::SamplerAddressMode::eRepeat,
                    .addressModeW            = vk::SamplerAddressMode::eRepeat,
                    .mipLodBias              = 0.f,
                    .anisotropyEnable        = vk::True,
                    .maxAnisotropy           = physical_device.getProperties().limits.maxSamplerAnisotropy,
                    .compareEnable           = vk::False,
                    .compareOp               = vk::CompareOp::eAlways,
                    .minLod                  = 0.f,
                    .maxLod                  = static_cast<float>(mip_levels),
                    .borderColor             = vk::BorderColor::eIntOpaqueBlack,
                    .unnormalizedCoordinates = vk::False,
        }   )  );
    }

    void Renderer::create_img(
            uint32_t width,
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

    void Renderer::transition_img_layout(
            vk::Image img,
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

    void Renderer::copy_buffer_to_img(vk::Buffer buffer, vk::Image img, uint32_t width, uint32_t height) {
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

    void Renderer::generate_mipmaps(
            vk::Image image,
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

    void Renderer::create_depth_resources() {
        vk::Format depth_format = find_depth_format();
        create_img(
            swap_chain_extent.width,
            swap_chain_extent.height,
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
    
    vk::Format Renderer::find_supported_format(
            const std::vector<vk::Format>& candidates,
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

    bool Renderer::has_stencil_component(vk::Format format) {
        return format == vk::Format::eD32SfloatS8Uint
            || format == vk::Format::eD24UnormS8Uint;
    }

    vk::Format Renderer::find_depth_format() {
        return find_supported_format(
            { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment
        );
    }

    void Renderer::create_color_resources() {
        vk::Format color_format = swap_chain_img_format;
        create_img(
            swap_chain_extent.width,
            swap_chain_extent.height,
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

    void Renderer::load_model() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;
        expect("Failed loading model.\n" + warn + err,
            tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())
        );
        std::unordered_map<Vertex, uint32_t> unique_vertices {};
        for( const tinyobj::shape_t& shape : shapes ) {
            for( const tinyobj::index_t& index : shape.mesh.indices ) {
                Vertex vertex {
                    .pos = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2],
                    },
                    .color = {
                        1.f,
                        1.f,
                        1.f,
                    },
                    .uvs = {
                              attrib.texcoords[2 * index.texcoord_index + 0],
                        1.f - attrib.texcoords[2 * index.texcoord_index + 1],
                    },
                };
                if( unique_vertices.count(vertex) == 0 ) {
                    unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(unique_vertices[vertex]);
            }
        }
    }

    vk::CommandBuffer Renderer::begin_single_time_commands() {
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

    void Renderer::end_single_time_commands(vk::CommandBuffer command_buffer) {
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

    void Renderer::create_render_pass() {
        std::array<vk::AttachmentDescription, 3> attachments = {
            vk::AttachmentDescription { // Color Attachment
                .format         = swap_chain_img_format,
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
                .format         = swap_chain_img_format,
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

    void Renderer::create_framebuffers() {
        swap_chain_framebuffers.resize(swap_chain_img_views.size());
        for( size_t i = 0; i < swap_chain_img_views.size(); i++ ) {
            std::array<vk::ImageView, 3> attachments = {
                color_img_view,
                depth_img_view,
                swap_chain_img_views[i],
            };
            vk::FramebufferCreateInfo framebuffer_info {
                .renderPass      = render_pass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments    = attachments.data(),
                .width           = swap_chain_extent.width,
                .height          = swap_chain_extent.height,
                .layers          = 1,
            };
            swap_chain_framebuffers[i] = expect("Failed to create framebuffer.",
                device.createFramebuffer(framebuffer_info)
            );
        }
    }

    void Renderer::create_command_pool() {
        QueueFamilyIndices queue_family_indices = find_queue_families(physical_device);
        vk::CommandPoolCreateInfo pool_info {
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queue_family_indices.graphics_family.value(),
        };
        command_pool = expect("Failed to create command pool.",
            device.createCommandPool(pool_info)
        );
    }

    void Renderer::create_command_buffers() {
        command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        command_buffers = expect("Failed to allocate command buffer.",
            device.allocateCommandBuffers(
                vk::CommandBufferAllocateInfo   {
                    .commandPool        = command_pool,
                    .level              = vk::CommandBufferLevel::ePrimary,
                    .commandBufferCount = (uint32_t) command_buffers.size(),
        }   )  );
    }

    void Renderer::record_command_buffer(vk::CommandBuffer command_buffer, uint32_t img_index) {
        vk::CommandBufferBeginInfo begin_info {};
        expect("Failed to begin recording command buffer.",
            command_buffer.begin(begin_info)
        );
        std::array<vk::ClearValue, 2> clear_values {{
            { .color         = {{{ .015f, .015f, .015f, 1.f }}} }, // [0]
            { .depthStencil  = { 1.f, 0 } }                        // [1]
        }};
        vk::RenderPassBeginInfo render_pass_info {
            .renderPass      = render_pass,
            .framebuffer     = swap_chain_framebuffers[img_index],
            .renderArea {
                .offset         = { 0, 0 },
                .extent         = swap_chain_extent,
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
                .width    = (float) swap_chain_extent.width,
                .height   = (float) swap_chain_extent.height,
                .minDepth = 0.f,
                .maxDepth = 1.f,
            };
            command_buffer.setViewport(0, viewport);
            vk::Rect2D scissor {
                .offset   = { 0, 0 },
                .extent   = swap_chain_extent,
            };
            command_buffer.setScissor(0, scissor);

            vk::Buffer vertex_buffers[] = { vertex_buffer };
            vk::DeviceSize offsets[]    = { 0 };
            command_buffer.bindVertexBuffers(0, vertex_buffers, offsets);
            command_buffer.bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint32);
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptor_sets[current_frame], nullptr);
            command_buffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        }
        command_buffer.endRenderPass();
        expect("Failed to record command buffer.",
            command_buffer.end()
        );
    }
    
    void Renderer::create_sync_objects() {
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

    void Renderer::draw() {
        expect("Renderer; Wait for fences failed.",
            device.waitForFences(in_flight_fences, true, UINT64_MAX)
        );
        vk::ResultValue<uint32_t> next_img = device.acquireNextImageKHR(swap_chain, UINT64_MAX, img_available_semaphores[current_frame]);
        uint32_t img_index = next_img.value;
        if( next_img.result == vk::Result::eErrorOutOfDateKHR ) {
            recreate_swap_chain();
            return;
        } else if ( next_img.result != vk::Result::eSuccess && next_img.result != vk::Result::eSuboptimalKHR ) {
            throw std::runtime_error("\tError: Failed to acquire swap chain image.");
        }
        update_uniform_buffer(current_frame);
        device.resetFences(in_flight_fences[current_frame]);
        command_buffers[current_frame].reset();
        record_command_buffer(command_buffers[current_frame], img_index);

        vk::Semaphore wait_semaphores[]      = { img_available_semaphores[current_frame]            };
        vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput  };
        vk::Semaphore signal_semaphores[]    = { render_finished_semaphores[current_frame]          };
        
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
        vk::SwapchainKHR swap_chains[] = { swap_chain };
        vk::PresentInfoKHR present_info {
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = signal_semaphores,
            .swapchainCount       = 1,
            .pSwapchains          = swap_chains,
            .pImageIndices        = &img_index,
        };
        vk::Result result = present_queue.presentKHR(present_info);
        if( result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebuffer_resized ) {
            framebuffer_resized = false;
            recreate_swap_chain();
        } else if ( result != vk::Result::eSuccess ) {
            throw std::runtime_error("\tError: Failed to present swap chain image.");
        }
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    VkResult Renderer::create_debug_util_messenger_ext(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
            const VkAllocationCallbacks* p_allocator,
            VkDebugUtilsMessengerEXT* p_debug_messenger
    ) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if( func != nullptr ) {
            return func(instance, p_create_info, p_allocator, p_debug_messenger);
        } else { return VK_ERROR_EXTENSION_NOT_PRESENT; }
    }
    
    void Renderer::destroy_debug_util_messenger_ext(
            VkInstance instance,
            VkDebugUtilsMessengerEXT p_debug_messenger,
            const VkAllocationCallbacks* p_allocator
    ) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if( func != nullptr ) {
            return func(instance, p_debug_messenger, p_allocator);
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debug_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
            VkDebugUtilsMessageTypeFlagsEXT msg_type,
            const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
            void* p_user_data
    ) {
        std::cerr << "Validation layer: " << p_callback_data->pMessage << "\n";
        return vk::False;
    }

    void Renderer::resize_callback(GLFWwindow* window, int width, int height) {
        renderer.framebuffer_resized = true;
    }

    void Renderer::terminate() {
        expect("Device wait idle failed.", device.waitIdle());
        cleanup_swap_chain();
        device.destroyPipeline(graphics_pipeline);
        device.destroyPipelineLayout(pipeline_layout);
        device.destroyRenderPass(render_pass);
        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            device.destroyBuffer(uniform_buffers[i]);
            device.freeMemory(uniform_buffers_memory[i]);
        }
        device.destroyDescriptorPool(descriptor_pool);
        device.destroySampler(texture_sampler);
        device.destroyImageView(texture_image_view);
        device.destroyImage(texture_image);
        device.freeMemory(texture_image_memory);
        device.destroyDescriptorSetLayout(descriptor_set_layout);
        device.destroyBuffer(index_buffer);
        device.freeMemory(index_buffer_memory);
        device.destroyBuffer(vertex_buffer);
        device.freeMemory(vertex_buffer_memory);
        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            device.destroySemaphore(img_available_semaphores[i]);
            device.destroySemaphore(render_finished_semaphores[i]);
            device.destroyFence(in_flight_fences[i]);
        }
        device.destroyCommandPool(command_pool);
        device.destroy();
#ifdef DEBUG
        instance.destroyDebugUtilsMessengerEXT(debug_messenger);
#endif
        instance.destroySurfaceKHR(surface);
        instance.destroy();
    }
}

