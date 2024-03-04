#pragma once

#include "sigil.hh"
#include "glfw.hh"
#include <cstdint>

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace sigil {
    namespace renderer {

#define SIGIL_V VK_MAKE_VERSION(version::major, version::minor, version::patch)

        const vk::ApplicationInfo engine_info = {
            .pApplicationName                       = "sigil",
            .applicationVersion                     = SIGIL_V,
            .pEngineName                            = "sigil",
            .engineVersion                          = SIGIL_V,
            .apiVersion                             = VK_API_VERSION_1_3,
        };

        const std::vector<const char*> validation_layers = { "VK_LAYER_KHRONOS_validation",  };
        const std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        inline vk::Instance instance;
        inline vk::SurfaceKHR* surface;
        inline vk::PhysicalDevice phys_device;
        inline vk::Device device;

        struct {
            vk::SwapchainKHR             handle;
            vk::Format                   img_format;
            std::vector<vk::Image>       images;
            std::vector<vk::ImageView>   img_views;
            vk::Extent2D                 extent;
        } inline swapchain;

        constexpr u16 FRAME_OVERLAP = 2;
        struct {
            struct {
                vk::CommandPool   pool;
                vk::CommandBuffer buffer;
            }             cmd;
            vk::Fence     fence;
            vk::Semaphore render_semaphore;
            vk::Semaphore swap_semaphore;
        } inline frames[FRAME_OVERLAP];
        u32 current_frame = 0;

        struct {
            vk::Queue handle;
            u32       family;
        } inline graphics_queue;

        //_____________________________________
        inline void build_swapchain() {
            int w, h;
            glfwGetFramebufferSize(window::handle, &w, &h);

            auto capabilities = phys_device.getSurfaceCapabilitiesKHR(*surface).value;
            swapchain.handle = device.createSwapchainKHR(vk::SwapchainCreateInfoKHR {
                .surface            = *surface,
                .minImageCount      = capabilities.minImageCount,
                .imageFormat        = vk::Format::eB8G8R8A8Srgb,
                .imageColorSpace    = vk::ColorSpaceKHR::eSrgbNonlinear,
                .imageExtent        = { (u32)w, (u32)h },
                .imageArrayLayers   = 1,
                .imageUsage         = vk::ImageUsageFlagBits::eColorAttachment,
                .preTransform       = capabilities.currentTransform,
                .compositeAlpha     = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                .presentMode        = vk::PresentModeKHR::eMailbox,
                .clipped            = true,
            }   ).value;

            swapchain.images = device.getSwapchainImagesKHR(swapchain.handle).value;
            swapchain.img_format = vk::Format::eB8G8R8A8Srgb;
            swapchain.extent = { (u32)w, (u32)h };
        }

        //_____________________________________
        inline u32 get_queue_family(vk::QueueFlagBits type) {
            auto q_families = phys_device.getQueueFamilyProperties();
            switch( type ) {
                case vk::QueueFlagBits::eGraphics: {
                    for( u32 i = 0; i < q_families.size(); i++ ) {
                        if( (q_families[i].queueFlags & type) == type ) { return i; }
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
            for( auto frame : frames ) {
                ALLOW_UNUSED(device.createCommandPool(&cmd_pool_info, nullptr, &frame.cmd.pool));
                ALLOW_UNUSED(device.allocateCommandBuffers(
                    vk::CommandBufferAllocateInfo {
                        .commandPool = frame.cmd.pool,
                        .level = vk::CommandBufferLevel::ePrimary,
                        .commandBufferCount = 1,
                    },
                    &frame.cmd.buffer
                ));
            }
        }

        //_____________________________________
        inline void build_sync_structures() {
            auto fence_info = vk::FenceCreateInfo { .flags = vk::FenceCreateFlagBits::eSignaled, };
            auto semaphore_info = vk::SemaphoreCreateInfo { .flags = vk::SemaphoreCreateFlagBits(), };
            for( auto frame : frames ) {
                ALLOW_UNUSED(device.createFence(&fence_info, nullptr, &frame.fence));
                ALLOW_UNUSED(device.createSemaphore(&semaphore_info, nullptr, &frame.swap_semaphore));
                ALLOW_UNUSED(device.createSemaphore(&semaphore_info, nullptr, &frame.render_semaphore));
            }
        }

        //_____________________________________
        inline void init() {
            u32 glfw_extension_count = 0;
            const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
            std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
            
            instance = vk::createInstance(
                vk::InstanceCreateInfo {
                    .pApplicationInfo           = &engine_info,
                    .enabledLayerCount          = 0,
                    .enabledExtensionCount      = (u32)extensions.size(),
                    .ppEnabledExtensionNames    = extensions.data(),
            }   ).value;

            glfwCreateWindowSurface(instance, sigil::window::handle, nullptr, (VkSurfaceKHR*)&surface);

            phys_device = instance.enumeratePhysicalDevices().value[0]; // TODO: propperly pick GPU
            device = phys_device.createDevice(vk::DeviceCreateInfo{}, nullptr).value;

            build_swapchain();

            graphics_queue.family = get_queue_family(vk::QueueFlagBits::eGraphics);
            graphics_queue.handle = device.getQueue(graphics_queue.family, 0);

            build_command_structures();
            build_sync_structures();
        }

        //_____________________________________
        inline void transition_img(vk::CommandBuffer cmd_buffer, vk::Image img, vk::ImageLayout from, vk::ImageLayout to) {
            auto img_barrier = vk::ImageMemoryBarrier2 {
                .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
                .oldLayout = from,
                .newLayout = to,
                .image = img,
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
            auto frame = frames[current_frame % FRAME_OVERLAP];
            ALLOW_UNUSED(device.waitForFences(1, &frame.fence, true, UINT64_MAX));
            ALLOW_UNUSED(device.resetFences(1, &frame.fence));

            vk::Image img = swapchain.images[device.acquireNextImageKHR(swapchain.handle, UINT64_MAX, frame.swap_semaphore).value];

            frame.cmd.buffer.reset();
            ALLOW_UNUSED(frame.cmd.buffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, }));
            {
                transition_img(frame.cmd.buffer, img, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                frame.cmd.buffer.clearColorImage(
                    img,
                    vk::ImageLayout::eGeneral,
                    vk::ClearColorValue {{{ 0.f, 0.f, 0.f, 1.f }}},
                    vk::ImageSubresourceRange {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel       = 0,
                        .levelCount         = vk::RemainingMipLevels,
                        .baseArrayLayer     = 0,
                        .layerCount         = vk::RemainingArrayLayers,
                    }
                );
                transition_img(frame.cmd.buffer, img, vk::ImageLayout::eGeneral, vk::ImageLayout::ePresentSrcKHR);
            }
            ALLOW_UNUSED(frame.cmd.buffer.end());

            current_frame++;
        }

        //_____________________________________
        inline void terminate() {
            ALLOW_UNUSED(device.waitIdle());

            device.destroySwapchainKHR(swapchain.handle);
            for( auto view : swapchain.img_views ) {
                device.destroyImageView(view);
            }

            for( auto frame : frames ) {
                device.destroyCommandPool(frame.cmd.pool);
            }

            device.destroy();
            instance.destroySurfaceKHR(*surface);
            instance.destroy();
        }
    } // renderer
} // sigil

struct vulkan {
    vulkan() {
        using namespace sigil;
        schedule(runlvl::init, renderer::init     );
        schedule(runlvl::tick, renderer::draw     );
        schedule(runlvl::exit, renderer::terminate);
    }
};


