#include "renderer.hh"
#include "engine.hh"

#include <array>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <algorithm>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace sigil {

    void Renderer::init() {
        create_instance();
        setup_debug_messenger();
        create_surface();
        select_physical_device();
        create_logical_device();
        create_swap_chain();
        create_img_views();
        create_render_pass();
        create_descriptor_set_layout();
        create_graphics_pipeline();
        create_command_pool();
        create_depth_resources();
        create_framebuffers();
        create_texture_img();
        create_texture_img_view();
        create_texture_sampler();
        create_vertex_buffer();
        create_index_buffer();
        create_uniform_buffers();
        create_descriptor_pool();
        create_descriptor_sets();
        create_command_buffers();
        create_sync_objects();
    }

    void Renderer::terminate() {
        vkDeviceWaitIdle(device); // tmp
        cleanup_swap_chain();
        vkDestroyPipeline(device, graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        vkDestroyRenderPass(device, render_pass, nullptr);
        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            vkDestroyBuffer(device, uniform_buffers[i], nullptr);
            vkFreeMemory(device, uniform_buffers_memory[i], nullptr);
        }
        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        vkDestroySampler(device, texture_sampler, nullptr);
        vkDestroyImageView(device, texture_image_view, nullptr);
        vkDestroyImage(device, texture_image, nullptr);
        vkFreeMemory(device, texture_image_memory, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
        vkDestroyBuffer(device, index_buffer, nullptr);
        vkFreeMemory(device, index_buffer_memory, nullptr);
        vkDestroyBuffer(device, vertex_buffer, nullptr);
        vkFreeMemory(device, vertex_buffer_memory, nullptr);
        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            vkDestroySemaphore(device, img_available_semaphores[i], nullptr);
            vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
            vkDestroyFence(device, in_flight_fences[i], nullptr);
        }
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroyDevice(device, nullptr);
        if( enable_validation_layers ) {
            destroy_debug_util_messenger_ext(instance, debug_messenger, nullptr);
        }
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
    
    void Renderer::create_instance() {
        if( enable_validation_layers && !check_validation_layer_support() ) {
            throw std::runtime_error("Error: Validation layers requested but not available.");
        }
        VkApplicationInfo engine_info{};
        engine_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        engine_info.pApplicationName = "sigil";
        engine_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        engine_info.pEngineName = "sigil";
        engine_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
        engine_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &engine_info;

        //print_extensions();
        std::vector<const char*> extensions = get_required_extensions();

        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        if( enable_validation_layers ) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            create_info.ppEnabledLayerNames = validation_layers.data();
            populate_debug_messenger_create_info(debug_create_info);
            create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debug_create_info;
        } else {
            create_info.enabledLayerCount = 0;
            create_info.pNext = nullptr;
        }
        if( vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create vulkan instance.");
        }
    }
    
    std::vector<const char*> Renderer::get_required_extensions() {
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
        if( enable_validation_layers ) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    void Renderer::print_extensions() {
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

        std::cout << "VULKAN ─┬─ Available Extensions:\n";
        for( const VkExtensionProperties& extension : extensions) {
            std::cout << '\t' << "│ " << extension.extensionName << '\n';
        }
        std::cout << '\t' << "└─" << '\n';
    }
    
    void Renderer::select_physical_device() {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
        if( device_count == 0 ) {
            throw std::runtime_error("\tError: Could not find GPU with Vulkan support.");
        }
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

        for( const VkPhysicalDevice& device : devices ) {
            if( is_device_suitable(device) ) {
                physical_device = device;
                break;
            }
        }
        //std::multimap<int, VkPhysicalDevice> candidates;

        //for( const VkPhysicalDevice& device : devices ) { // Find best suited compatible device
        //    QueueFamilyIndices indices = find_queue_families(device);
        //    if( indices.graphics_family.has_value() ) {
        //        int score = score_device_suitability(device);
        //        candidates.insert(std::make_pair(score, device));
        //    }
        //}
        //if( candidates.rbegin()->first > 0 ) {
        //    physical_device = candidates.rbegin()->second;
        //} else {
        if( physical_device == VK_NULL_HANDLE ) {
            throw std::runtime_error("\tError: Failed to find suitable GPU.");
        }
    }

    bool Renderer::is_device_suitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = find_queue_families(device);

        bool b_extensions_supported = check_device_extension_support(device);
        bool b_valid_swapchain = false;
        if( b_extensions_supported ) {
            SwapChainSupportDetails swap_chain_support = query_swap_chain_support(device);
            b_valid_swapchain = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
        }
        VkPhysicalDeviceFeatures supported_features;
        vkGetPhysicalDeviceFeatures(device, &supported_features);
        return indices.is_complete() && b_extensions_supported && b_valid_swapchain && supported_features.samplerAnisotropy;
    }

    int Renderer::score_device_suitability(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(device, &device_properties);
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures(device, &device_features);

        int score = 0;
        if( device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
            score += 1000;
        }
        bool b_extensions_supported = check_device_extension_support(device);
        bool b_valid_swapchain = false;
        if( b_extensions_supported ) {
            SwapChainSupportDetails swap_chain_support = query_swap_chain_support(device);
            b_valid_swapchain = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
        }
        score += device_properties.limits.maxImageDimension2D;
        if( !device_features.geometryShader && !b_extensions_supported && !b_valid_swapchain ) {
            return 0;
        }
        return score;
    }

    bool Renderer::check_device_extension_support(VkPhysicalDevice device) {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

        std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());
        for( const auto& extension : available_extensions ) {
            required_extensions.erase(extension.extensionName);
        }
        return required_extensions.empty();
    }

    QueueFamilyIndices Renderer::find_queue_families(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        int i = 0;
        for( const VkQueueFamilyProperties& queue_family : queue_families ) {
            if( queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
                indices.graphics_family = i;
            }
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if( present_support ) {
                indices.present_family = i;
            }
            if( indices.is_complete() ) {
                break; 
            } i++;
        }
        return indices;
    }

    void Renderer::create_logical_device() {
        QueueFamilyIndices indices = find_queue_families(physical_device);

        std::vector<VkDeviceQueueCreateInfo> queue_create_info_vec;
        std::set<uint32_t> unique_queue_familes = { indices.graphics_family.value(), indices.present_family.value() };

        float queue_priority = 1.f;
        for( uint32_t queue_family : unique_queue_familes ) {
            VkDeviceQueueCreateInfo queue_create_info {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_info_vec.push_back(queue_create_info);
        }

        VkPhysicalDeviceFeatures device_features {};
        device_features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo create_info {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_info_vec.size());
        create_info.pQueueCreateInfos = queue_create_info_vec.data();
        create_info.pEnabledFeatures = &device_features;
        create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
        create_info.ppEnabledExtensionNames = device_extensions.data();

        if( enable_validation_layers ) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            create_info.ppEnabledLayerNames = validation_layers.data();
        } else {
            create_info.enabledLayerCount = 0;
        }
        if( vkCreateDevice(physical_device, &create_info, nullptr, &device) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create logical device.");
        }
        vkGetDeviceQueue(device, indices.graphics_family.value(), 0, &graphics_queue);
        vkGetDeviceQueue(device, indices.present_family.value(), 0, &present_queue);
    }

    void Renderer::create_surface() {
        if( glfwCreateWindowSurface(instance, core.window.ptr, nullptr, &surface) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create window surface.");
        }
    }

    SwapChainSupportDetails Renderer::query_swap_chain_support(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
        if( format_count != 0 ) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
        }
        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
        if( present_mode_count != 0 ) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
        }
        return details;
    }

    VkSurfaceFormatKHR Renderer::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) {
        for( const VkSurfaceFormatKHR& available_format : available_formats ) {
            if( available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
                return available_format;
            }
        } return available_formats[0];
    }

    VkPresentModeKHR Renderer::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes) {
        for( const VkPresentModeKHR& available_present_mode : available_present_modes ) {
            if( available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR ) {
                return available_present_mode;
            }
        } return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Renderer::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(core.window.ptr, &width, &height);
            
            VkExtent2D actual_extent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
            };
            actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.minImageExtent.width);
            actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.minImageExtent.height);
            return actual_extent;
        }
    }

    void Renderer::create_swap_chain() {
        SwapChainSupportDetails swap_chain_support = query_swap_chain_support(physical_device);

        VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats);
        VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.present_modes);
        VkExtent2D extent = choose_swap_extent(swap_chain_support.capabilities);

        uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
        if( swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount ) {
            image_count = swap_chain_support.capabilities.maxImageCount;
        }
        VkSwapchainCreateInfoKHR create_info {};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = surface;
        create_info.minImageCount = image_count;
        create_info.imageFormat = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = find_queue_families(physical_device);
        uint32_t queue_family_indices[] = { indices.graphics_family.value(), indices.present_family.value() };

        if( indices.graphics_family != indices.present_family ) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        } else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        create_info.preTransform = swap_chain_support.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE;
        create_info.oldSwapchain = VK_NULL_HANDLE;

        if( vkCreateSwapchainKHR(device, &create_info, nullptr, &swap_chain) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create swap chain.");
        }
        vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr);
        swap_chain_images.resize(image_count);
        vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images.data());
        
        swap_chain_img_format = surface_format.format;
        swap_chain_extent = extent;
    }

    void Renderer::cleanup_swap_chain() {
        vkDestroyImageView(device, depth_img_view, nullptr);
        vkDestroyImage(device, depth_img, nullptr);
        vkFreeMemory(device, depth_img_memory, nullptr);

        for( auto frambuffer : swap_chain_framebuffers ) {
            vkDestroyFramebuffer(device, frambuffer, nullptr);
        }
        for( auto img_view : swap_chain_img_views ) {
            vkDestroyImageView(device, img_view, nullptr);
        }
        vkDestroySwapchainKHR(device, swap_chain, nullptr);
    }

    void Renderer::recreate_swap_chain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(core.window.ptr, &width, &height);
        while( width == 0 || height == 0 ) {
            glfwGetFramebufferSize(core.window.ptr, &width, &height);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(device);
        cleanup_swap_chain();

        create_swap_chain();
        create_img_views();
        create_depth_resources();
        create_framebuffers();
    }

    VkImageView Renderer::create_img_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
        VkImageViewCreateInfo view_info {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = aspect_flags;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkImageView img_view;
        if( vkCreateImageView(device, &view_info, nullptr, &img_view) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create image views.");
        }
        return img_view;
    }

    void Renderer::create_img_views() {
        swap_chain_img_views.resize(swap_chain_images.size());

        for( size_t i = 0; i < swap_chain_images.size(); i++ ) {
            swap_chain_img_views[i] = create_img_view(
                                        swap_chain_images[i],
                                        swap_chain_img_format,
                                        VK_IMAGE_ASPECT_COLOR_BIT
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
        std::vector<char> vertex_code = read_file("./shaders/simple_shader.vert.spv");
        std::vector<char> fragment_code = read_file("./shaders/simple_shader.frag.spv");

        VkShaderModule vert_shader_module = create_shader_module(vertex_code);
        VkShaderModule frag_shader_module = create_shader_module(fragment_code);

        VkPipelineShaderStageCreateInfo vert_shader_stage_info {};
        vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_shader_stage_info.module = vert_shader_module;
        vert_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo frag_shader_stage_info {};
        frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_stage_info.module = frag_shader_module;
        frag_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

        VkPipelineVertexInputStateCreateInfo vertex_input_info {};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto binding_description = Vertex::get_binding_description();
        auto attribute_descriptions = Vertex::get_attribute_descriptions();

        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexBindingDescriptions = &binding_description;
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly {};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport_state {};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth_stencil {};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment {};
        color_blend_attachment.colorWriteMask =
              VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blending {};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;
        color_blending.blendConstants[0] = 0.f;
        color_blending.blendConstants[1] = 0.f;
        color_blending.blendConstants[2] = 0.f;
        color_blending.blendConstants[3] = 0.f;

        std::vector<VkDynamicState> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamic_state {};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        VkPipelineLayoutCreateInfo pipeline_layout_info {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 0;

        if( vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipeline_info {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = nullptr;//&depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = pipeline_layout;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

        if( vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create graphics pipeline.");
        }

        vkDestroyShaderModule(device, frag_shader_module, nullptr);
        vkDestroyShaderModule(device, vert_shader_module, nullptr);
    }
    
    VkShaderModule Renderer::create_shader_module(const std::vector<char>& code) {
        VkShaderModuleCreateInfo create_info {};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size();
        create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule shader_module;
        if( vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create shader module.");
        }
        return shader_module;
    }

    void Renderer::create_descriptor_set_layout() {
        VkDescriptorSetLayoutBinding ubo_layout_binding {};
        ubo_layout_binding.binding = 0;
        ubo_layout_binding.descriptorCount = 1;
        ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_layout_binding.pImmutableSamplers = nullptr;
        ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding sampler_layout_binding {};
        sampler_layout_binding.binding = 1;
        sampler_layout_binding.descriptorCount = 1;
        sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_layout_binding.pImmutableSamplers = nullptr;
        sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { ubo_layout_binding, sampler_layout_binding };
        VkDescriptorSetLayoutCreateInfo layout_info {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        if( vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create descriptor set layout.");
        }
    }

    void Renderer::create_descriptor_pool() {
        std::array<VkDescriptorPoolSize, 2> pool_sizes {};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo pool_info {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if( vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create descriptor pool.");
        }
    }

    void Renderer::create_descriptor_sets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptor_set_layout);
        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool;
        alloc_info.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        alloc_info.pSetLayouts = layouts.data();

        descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
        if( vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.data()) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to allocate descriptor sets.");
        }
        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            VkDescriptorBufferInfo buffer_info {};
            buffer_info.buffer = uniform_buffers[i];
            buffer_info.offset = 0;
            buffer_info.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo image_info {};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = texture_image_view;
            image_info.sampler = texture_sampler;

            std::array<VkWriteDescriptorSet, 2> descriptor_writes {};

            descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].dstSet = descriptor_sets[i];
            descriptor_writes[0].dstBinding = 0;
            descriptor_writes[0].dstArrayElement = 0;
            descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_writes[0].descriptorCount = 1;
            descriptor_writes[0].pBufferInfo = &buffer_info;

            descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[1].dstSet = descriptor_sets[i];
            descriptor_writes[1].dstBinding = 1;
            descriptor_writes[1].dstArrayElement = 0;
            descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[1].descriptorCount = 1;
            descriptor_writes[1].pImageInfo = &image_info;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
        }
    }

    void Renderer::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) {
        VkBufferCreateInfo buffer_info {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if( vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Could not create buffer.");
        }
        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

        VkMemoryAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);
        if( vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to allocate buffer memory.");
        }
        vkBindBufferMemory(device, buffer, buffer_memory, 0);
    }

    void Renderer::create_vertex_buffer() {
        VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        create_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
            );

        void* data;
        vkMapMemory(device, staging_buffer_memory, 0, buffer_size, 0, &data);
        {
            memcpy(data, vertices.data(), (size_t) buffer_size);
        }
        vkUnmapMemory(device, staging_buffer_memory);

        create_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertex_buffer,
            vertex_buffer_memory
            );
        copy_buffer(staging_buffer, vertex_buffer, buffer_size);
        
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);
    }

    void Renderer::create_index_buffer() {
        VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        create_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
            );

        void* data;
        vkMapMemory(device, staging_buffer_memory, 0, buffer_size, 0, &data);
        {
            memcpy(data, indices.data(), (size_t) buffer_size);
        }
        vkUnmapMemory(device, staging_buffer_memory);

        create_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            index_buffer,
            index_buffer_memory
            );
        copy_buffer(staging_buffer, index_buffer, buffer_size);
        
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);
    }

    void Renderer::create_uniform_buffers() {
        VkDeviceSize buffer_size = sizeof(UniformBufferObject);
        uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_buffers_memory.resize(MAX_FRAMES_IN_FLIGHT);
        uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            create_buffer(
                buffer_size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniform_buffers[i],
                uniform_buffers_memory[i]
                );
            vkMapMemory(device, uniform_buffers_memory[i], 0, buffer_size, 0, &uniform_buffers_mapped[i]);
        }
    }

    void Renderer::update_uniform_buffer(uint32_t current_image) {
        static auto start_time = std::chrono::high_resolution_clock::now();
        auto current_time = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
        UniformBufferObject ubo {};
        ubo.model = glm::rotate(glm::mat4(1.f), time * glm::radians(45.f), glm::vec3(0.f, 0.f, 1.f));
        ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
        ubo.proj = glm::perspective(glm::radians(45.f), swap_chain_extent.width / (float) swap_chain_extent.height, 0.f, 10.f);
        ubo.proj[1][1] *= -1;
        memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
    }

    void Renderer::copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
        VkCommandBuffer command_buffer = begin_single_time_commands();
        {
            VkBufferCopy copy_region {};
            copy_region.srcOffset = 0;
            copy_region.dstOffset = 0;
            copy_region.size = size;
            vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
        }
        end_single_time_commands(command_buffer);
    }

    uint32_t Renderer::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
        for( uint32_t i = 0; i < mem_properties.memoryTypeCount; i++ ) {
            if( type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
                return i;
            }
        }
        throw std::runtime_error("\tError: Failed ot find suitable memory type.");
    }

    void Renderer::create_texture_img() {
        int t_width, t_height, t_channels;
        stbi_uc* pixels = stbi_load("textures/missing_texture.jpg", &t_width, &t_height, &t_channels, STBI_rgb_alpha);
        VkDeviceSize img_size = t_width * t_height * 4;
        if( !pixels ) {
            throw std::runtime_error("\tError: Failed to load texture image.");
        }
        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        create_buffer(
            img_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
            );
        void* data;
        vkMapMemory(device, staging_buffer_memory, 0, img_size, 0, &data);
        {
            memcpy(data, pixels, static_cast<size_t>(img_size));
        }
        vkUnmapMemory(device, staging_buffer_memory);
        stbi_image_free(pixels);

        create_img(
            t_width, 
            t_height, 
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture_image,
            texture_image_memory
        );
        transition_img_layout(
            texture_image,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        copy_buffer_to_img(
            staging_buffer,
            texture_image,
            static_cast<uint32_t>(t_width),
            static_cast<uint32_t>(t_width)
        );
        transition_img_layout(
            texture_image,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);
    }

    void Renderer::create_texture_img_view() {
        texture_image_view = create_img_view(
                                texture_image,
                                VK_FORMAT_R8G8B8A8_SRGB,
                                VK_IMAGE_ASPECT_COLOR_BIT
                            );
    }

    void Renderer::create_texture_sampler() {
        VkPhysicalDeviceProperties properties {};
        vkGetPhysicalDeviceProperties(physical_device, &properties);

        VkSamplerCreateInfo sampler_info {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if( vkCreateSampler(device, &sampler_info, nullptr, &texture_sampler) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create texture sampler.");
        }
    }

    void Renderer::create_img(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageTiling tiling,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkImage& image,
            VkDeviceMemory& image_memory
        ) {
        VkImageCreateInfo image_info {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = tiling;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        if( vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create image.");
        }
        VkMemoryRequirements mem_requirements;
        vkGetImageMemoryRequirements(device, image, &mem_requirements);

        VkMemoryAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);
        if( vkAllocateMemory(device, &alloc_info, nullptr, &image_memory) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to allocate memory.");
        }
        vkBindImageMemory(device, image, image_memory, 0);
    }

    void Renderer::transition_img_layout(VkImage img, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
        VkCommandBuffer command_buffer = begin_single_time_commands();
        {
            VkImageMemoryBarrier barrier {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = img;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            VkPipelineStageFlags source_stage;
            VkPipelineStageFlags destination_stage;

            if( new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

                if( has_stencil_component(format) ) {
                    barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
            } else {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }

            if( old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if( old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            } else if( old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            } else {
                throw std::runtime_error("\tError: Unsupported layout transition.");
            }

            vkCmdPipelineBarrier(
                command_buffer,
                source_stage, destination_stage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }
        end_single_time_commands(command_buffer);
    }

    void Renderer::copy_buffer_to_img(VkBuffer buffer, VkImage img, uint32_t width, uint32_t height) {
        VkCommandBuffer command_buffer = begin_single_time_commands();
        {
            VkBufferImageCopy region {};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;

            region.imageOffset = { 0, 0, 0 };
            region. imageExtent = { width, height, 1 };

            vkCmdCopyBufferToImage(
                command_buffer,
                buffer,
                img,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region
            );
        }
        end_single_time_commands(command_buffer);
    }

    void Renderer::create_depth_resources() {
        VkFormat depth_format = find_depth_format();
        create_img(
                swap_chain_extent.width,
                swap_chain_extent.height,
                depth_format,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depth_img,
                depth_img_memory
            );
        depth_img_view = create_img_view(
                                    depth_img,
                                    depth_format,
                                    VK_IMAGE_ASPECT_DEPTH_BIT
                                );
    }
    
    VkFormat Renderer::find_supported_format(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) {
        for( VkFormat format : candidates ) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

            if( tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features ) {
                return format;
            } else if( tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) ) {
                return format;
            }
        }
        throw std::runtime_error("\tError: Failed to find supported format.");
    }

    bool Renderer::has_stencil_component(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT
            || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    VkFormat Renderer::find_depth_format() {
        return find_supported_format(
                {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            );
    }

    VkCommandBuffer Renderer::begin_single_time_commands() {
        VkCommandBufferAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &begin_info);
        return command_buffer;
    }

    void Renderer::end_single_time_commands(VkCommandBuffer command_buffer) {
        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_queue);

        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }

    void Renderer::create_render_pass() {
        VkAttachmentDescription color_attachment {};
        color_attachment.format = swap_chain_img_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref {};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription deapth_attachment {};
        deapth_attachment.format = find_depth_format();
        deapth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        deapth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        deapth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        deapth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        deapth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        deapth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        deapth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_ref {};
        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pDepthStencilAttachment = nullptr;//&depth_attachment_ref;

        VkSubpassDependency dependency {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                 | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {
            color_attachment,
            deapth_attachment
        };

        VkRenderPassCreateInfo render_pass_info {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        if( vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create render pass.");
        }
    }

    void Renderer::create_framebuffers() {
        swap_chain_framebuffers.resize(swap_chain_img_views.size());
        for( size_t i = 0; i < swap_chain_img_views.size(); i++ ) {
            std::array<VkImageView, 2> attachments = {
                swap_chain_img_views[i],
                depth_img_view
            };
            VkFramebufferCreateInfo framebuffer_info {};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = render_pass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = swap_chain_extent.width;
            framebuffer_info.height = swap_chain_extent.height;
            framebuffer_info.layers = 1;
            if( vkCreateFramebuffer(device, &framebuffer_info, nullptr, &swap_chain_framebuffers[i]) != VK_SUCCESS ) {
                throw std::runtime_error("\tError: Failed to create framebuffer.");
            }
        }
    }

    void Renderer::create_command_pool() {
        QueueFamilyIndices queue_family_indices = find_queue_families(physical_device);
        VkCommandPoolCreateInfo pool_info {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
        if( vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to create command pool.");
        }
    }

    void Renderer::create_command_buffers() {
        command_buffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = (uint32_t) command_buffers.size();
        if( vkAllocateCommandBuffers(device, &alloc_info, command_buffers.data()) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to allocate command buffer.");
        }
    }

    void Renderer::record_command_buffer(VkCommandBuffer command_buffer, uint32_t img_index) {
        VkCommandBufferBeginInfo begin_info {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if( vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to begin recording command buffer.");
        }

        VkRenderPassBeginInfo render_pass_info {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass;
        render_pass_info.framebuffer = swap_chain_framebuffers[img_index];
        render_pass_info.renderArea.offset = { 0, 0 };
        render_pass_info.renderArea.extent = swap_chain_extent;

        std::array<VkClearValue, 2> clear_values {};
        clear_values[0].color = {{ 0.f, 0.f, 0.f, 1.f }};
        //clear_values[1].depthStencil = { 1.f, 0 };

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
        {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

            VkViewport viewport {};
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = (float) swap_chain_extent.width;
            viewport.height = (float) swap_chain_extent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);

            VkRect2D scissor {};
            scissor.offset = { 0, 0 };
            scissor.extent = swap_chain_extent;
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);

            VkBuffer vertex_buffers[] = { vertex_buffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
            vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_sets[current_frame], 0, nullptr);

            vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(command_buffer);
        
        if( vkEndCommandBuffer(command_buffer) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to record command buffer.");
        }
    }
    
    void Renderer::create_sync_objects() {
        img_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphore_info {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
            if( vkCreateSemaphore(device, &semaphore_info, nullptr, &img_available_semaphores[i]) != VK_SUCCESS
             || vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS
             || vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS ) {
                throw std::runtime_error("\tError: Failed to create frame sync objects.");
            }
        }
    }

    void Renderer::draw() {
        vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

        uint32_t img_index;
        VkResult result = vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, img_available_semaphores[current_frame], VK_NULL_HANDLE, &img_index);
        if( result == VK_ERROR_OUT_OF_DATE_KHR ) {
            recreate_swap_chain();
            return;
        } else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
            throw std::runtime_error("\tError: Failed to acquire swap chain image.");
        }
        vkResetFences(device, 1, &in_flight_fences[current_frame]);

        vkResetCommandBuffer(command_buffers[current_frame], 0);
        record_command_buffer(command_buffers[current_frame], img_index);

        update_uniform_buffer(current_frame);

        VkSubmitInfo submit_info {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = { img_available_semaphores[current_frame] };
        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers[current_frame];

        VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        if( vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to submit draw command buffer.");
        }

        VkPresentInfoKHR present_info {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swap_chains[] = { swap_chain };
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swap_chains;
        present_info.pImageIndices = &img_index;

        result = vkQueuePresentKHR(present_queue, &present_info);
        if( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized ) {
            framebuffer_resized = false;
            recreate_swap_chain();
        } else if ( result != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to present swap chain image.");
        }
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    VkResult Renderer::create_debug_util_messenger_ext(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* p_create_info, const VkAllocationCallbacks* p_allocator, VkDebugUtilsMessengerEXT* p_debug_messenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if( func != nullptr ) {
            return func(instance, p_create_info, p_allocator, p_debug_messenger);
        } else { return VK_ERROR_EXTENSION_NOT_PRESENT; }
    }
    
    void Renderer::destroy_debug_util_messenger_ext(VkInstance instance, VkDebugUtilsMessengerEXT p_debug_messenger, const VkAllocationCallbacks* p_allocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if( func != nullptr ) {
            return func(instance, p_debug_messenger, p_allocator);
        }
    }

    void Renderer::populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
        create_info = {};
        create_info.sType =   VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = debug_callback;
    }

    void Renderer::setup_debug_messenger() {
        if( !enable_validation_layers ) return;

        VkDebugUtilsMessengerCreateInfoEXT create_info;
        populate_debug_messenger_create_info(create_info);
        if( create_debug_util_messenger_ext(instance, &create_info, nullptr, &debug_messenger) != VK_SUCCESS ) {
            throw std::runtime_error("\tError: Failed to set up debug messenger.");
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
        VkDebugUtilsMessageTypeFlagsEXT msg_type,
        const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
        void* p_user_data) {
            std::cerr << "Validation layer: " << p_callback_data->pMessage << "\n";
            return VK_FALSE;
    }

    bool Renderer::check_validation_layer_support() {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for( const char* layer_name : validation_layers ) {
            bool layer_found = false;
            for( const auto& layer_properties : available_layers ) {
                if( strcmp(layer_name, layer_properties.layerName) == 0 ) {
                    layer_found = true;
                    break;
                }
            }
            if( !layer_found ) return false;
        }
        return true;
    }

    void Renderer::resize_callback(GLFWwindow* window, int width, int height) {
        core.renderer.framebuffer_resized = true;
    }
}

