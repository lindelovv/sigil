#pragma once

#include "sigil.hh"
#include "glfw.hh"
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

namespace sigil::renderer {

#define VK_CHECK(x) do { VkResult e = (VkResult)x; if(e) { std::cout << "Vulkan error: " << e << "\n"; } } while(0)
#define SIGIL_V VK_MAKE_VERSION(version::major, version::minor, version::patch)

    const vk::ApplicationInfo engine_info = {
        .pApplicationName                       = "sigil",
        .applicationVersion                     = SIGIL_V,
        .pEngineName                            = "sigil",
        .engineVersion                          = SIGIL_V,
        .apiVersion                             = VK_API_VERSION_1_3,
    };

    const std::vector<const char*> layers            = { "VK_LAYER_KHRONOS_validation",   };
    const std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, };

    struct {
        vk::DebugUtilsMessengerEXT messenger;

        static VKAPI_ATTR VkBool32 VKAPI_CALL callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
            VkDebugUtilsMessageTypeFlagsEXT msg_type,
            const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
            void* p_user_data
        ) {
            std::cerr << "\n>> " << p_callback_data->pMessage << "\n";
            return false;
        }
    } inline debug;

    inline vk::Instance instance;
    inline vk::SurfaceKHR surface;
    inline vk::PhysicalDevice phys_device;
    inline vk::Device device;

    struct {
        vk::SwapchainKHR           handle;
        vk::Format                 img_format;
        std::vector<vk::Image>     images;
        std::vector<vk::ImageView> img_views;
        vk::Extent2D               extent;
    } inline swapchain;

    constexpr u8 FRAME_OVERLAP = 2;
    struct Frame {
        struct {
            vk::CommandPool   pool;
            vk::CommandBuffer buffer;
        } cmd;
        vk::Fence     fence;
        vk::Semaphore render_semaphore;
        vk::Semaphore swap_semaphore;
    };
    inline std::vector<Frame> frames { FRAME_OVERLAP };
    inline u32 current_frame = 0;

    struct {
        vk::Queue handle;
        u32       family;
    } inline graphics_queue;

    //_____________________________________
    inline void build_swapchain() {
        int w, h;
        glfwGetFramebufferSize(window::handle, &w, &h);

        auto capabilities = phys_device.getSurfaceCapabilitiesKHR(surface).value;
        swapchain.handle = device.createSwapchainKHR(vk::SwapchainCreateInfoKHR {
            .surface            = surface,
            .minImageCount      = capabilities.minImageCount + 1,
            .imageFormat        = (swapchain.img_format = vk::Format::eB8G8R8A8Srgb),
            .imageColorSpace    = vk::ColorSpaceKHR::eSrgbNonlinear,
            .imageExtent        = { (u32)w, (u32)h },
            .imageArrayLayers   = 1,
            .imageUsage         = vk::ImageUsageFlagBits::eColorAttachment,
            .preTransform       = capabilities.currentTransform,
            .compositeAlpha     = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode        = vk::PresentModeKHR::eMailbox,
            .clipped            = true,
        }   ).value;

        swapchain.extent = vk::Extent2D { (u32)w, (u32)h };
        swapchain.images = device.getSwapchainImagesKHR(swapchain.handle).value;
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
            auto cmd_buffer_info = vk::CommandBufferAllocateInfo {
                .commandPool        = frames.at(i).cmd.pool,
                .level              = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            VK_CHECK(device.allocateCommandBuffers(
                &cmd_buffer_info,
                &frames.at(i).cmd.buffer
            ));
        }
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
    }

    //_____________________________________
    inline void init() {
        u32 glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        VULKAN_HPP_DEFAULT_DISPATCHER.init();

        instance = vk::createInstance(
            vk::InstanceCreateInfo {
                .pApplicationInfo        = &engine_info,
                .enabledLayerCount       = (u32)layers.size(),
                .ppEnabledLayerNames     = layers.data(),
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
        auto queue_info = vk::DeviceQueueCreateInfo {
            .queueFamilyIndex   = graphics_queue.family,
            .queueCount         = 1,
            .pQueuePriorities   = &prio,
        };
        vk::PhysicalDeviceFeatures device_features {
            .sampleRateShading              = true,
            .samplerAnisotropy              = true,
        };
        auto sync_features = vk::PhysicalDeviceSynchronization2Features {
            .synchronization2 = true,
        };
        device = phys_device.createDevice(
            vk::DeviceCreateInfo{
                .pNext                   = &sync_features,
                .queueCreateInfoCount    = 1,
                .pQueueCreateInfos       = &queue_info,
                .enabledLayerCount       = (u32)layers.size(),
                .ppEnabledLayerNames     = layers.data(),
                .enabledExtensionCount   = (u32)device_extensions.size(),
                .ppEnabledExtensionNames =  device_extensions.data(),
                .pEnabledFeatures        = &device_features,
            },
            nullptr
        ).value;
        VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

        build_swapchain();

        graphics_queue.family = get_queue_family(vk::QueueFlagBits::eGraphics);
        graphics_queue.handle = device.getQueue(graphics_queue.family, 0);

        build_command_structures();
        build_sync_structures();
    }

    //_____________________________________
    inline void transition_img(vk::CommandBuffer cmd_buffer, vk::Image img, vk::ImageLayout from, vk::ImageLayout to) {
        auto img_barrier = vk::ImageMemoryBarrier2 {
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
        cmd_buffer.pipelineBarrier2(vk::DependencyInfo {
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &img_barrier,
        });
    }

    //_____________________________________
    inline void draw() {
        auto frame = frames.at(current_frame % FRAME_OVERLAP);

        VK_CHECK(device.waitForFences(1, &frame.fence, true, UINT64_MAX));
        VK_CHECK(device.resetFences(1, &frame.fence));

        u32 img_index = device.acquireNextImageKHR(swapchain.handle, UINT64_MAX, frame.swap_semaphore).value;
        vk::Image img = swapchain.images.at(img_index);

        frame.cmd.buffer.reset();
        VK_CHECK(frame.cmd.buffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, }));
        {
            transition_img(frame.cmd.buffer, img, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            frame.cmd.buffer.clearColorImage(
                img,
                vk::ImageLayout::eGeneral,
                vk::ClearColorValue {{{ 0.f, 0.f, 0.f, 1.f }}},
                vk::ImageSubresourceRange {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = 0,
                    .levelCount     = vk::RemainingMipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = vk::RemainingArrayLayers,
                }
            );
            transition_img(frame.cmd.buffer, img, vk::ImageLayout::eGeneral, vk::ImageLayout::ePresentSrcKHR);
        }
        VK_CHECK(frame.cmd.buffer.end());

        auto cmd_info = vk::CommandBufferSubmitInfo {
            .commandBuffer = frame.cmd.buffer,
            .deviceMask    = 0,
        };
        auto wait_info = vk::SemaphoreSubmitInfo {
            .semaphore   = frame.swap_semaphore,
            .value       = 1,
            .stageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .deviceIndex = 0,
        };
        auto signal_info = vk::SemaphoreSubmitInfo {
            .semaphore   = frame.render_semaphore,
            .value       = 1,
            .stageMask   = vk::PipelineStageFlagBits2::eAllGraphics,
            .deviceIndex = 0,
        };
        auto submit = vk::SubmitInfo2 {
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &wait_info,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmd_info,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signal_info,
        };
        VK_CHECK(graphics_queue.handle.submit2(submit, frame.fence));

        auto present_info = vk::PresentInfoKHR {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame.render_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.handle,
            .pImageIndices = &img_index,
        };
        VK_CHECK(graphics_queue.handle.presentKHR(&present_info));

        current_frame++;
    }

    //_____________________________________
    inline void terminate() {
        VK_CHECK(device.waitIdle());

        device.destroySwapchainKHR(swapchain.handle);
        for( auto view : swapchain.img_views ) {
            device.destroyImageView(view);
        }

        for( auto frame : frames ) {
            device.destroyCommandPool(frame.cmd.pool);
        }

        device.destroy();
        instance.destroySurfaceKHR(surface);
        instance.destroy();
    }
} // sigil::renderer

struct vulkan {
    vulkan() {
        using namespace sigil;
        schedule(runlvl::init, renderer::init     );
        schedule(runlvl::tick, renderer::draw     );
        schedule(runlvl::exit, renderer::terminate);
    }
};

