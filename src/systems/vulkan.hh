#pragma once

#include "sigil.hh"
#include "glfw.hh"

#include <deque>
#include <fstream>
#include <ranges>

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#define VMA_IMPLEMENTATION
#include "../../submodules/VulkanMemoryAllocator-Hpp/include/vk_mem_alloc.hpp"

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

    //_____________________________________
    struct DeletionQueue {
        std::deque<fn<void()>> queue;

        void push_fn(fn<void()>&& fn) { queue.push_back(fn); }

        void flush() {
            for( auto& fn : std::ranges::views::reverse(queue) ) {
                fn();
            } 
            queue.clear(); 
        }
    } inline main_delete_queue;

    //_____________________________________
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

    //_____________________________________
    inline vk::Instance         instance;
    inline vk::SurfaceKHR       surface;
    inline vk::PhysicalDevice   phys_device;
    inline vk::Device           device;

    struct {
        vk::SwapchainKHR           handle;
        vk::Format                 img_format;
        std::vector<vk::Image>     images;
        std::vector<vk::ImageView> img_views;
        vk::Extent2D               extent;
    } inline swapchain;

    //_____________________________________
    constexpr u8 FRAME_OVERLAP = 2;
    struct Frame {
        DeletionQueue deleteQueue;
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
    struct {
        struct {
            vk::Image       handle;
            vk::ImageView   view;
            vma::Allocation   alloc;
            vk::Extent3D    extent;
            vk::Format      format;
        }            img;
        vk::Extent2D extent;
    } inline draw;

    static vma::Allocator alloc;

    //_____________________________________
    struct {
        vk::DescriptorPool pool;
        struct {
            std::vector<vk::DescriptorSet> handles;
            vk::DescriptorSetLayout layout;
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
        } set;
    } inline descriptor;

    struct {
        std::vector<vk::Pipeline> handles;
        vk::PipelineLayout layout;
    } inline pipeline;

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
        for(auto image : swapchain.images) {
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
        draw.img = {
            .extent = vk::Extent3D { .width = (u32)w, .height = (u32)h, .depth = 1 },
            .format = vk::Format::eR16G16B16A16Sfloat,
        };
        std::tie( draw.img.handle, draw.img.alloc ) = alloc.createImage(
            vk::ImageCreateInfo {
                .imageType = vk::ImageType::e2D,
                .format = draw.img.format,
                .extent = draw.img.extent,
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
        draw.img.view = device.createImageView(
            vk::ImageViewCreateInfo {
                .image      = draw.img.handle,
                .viewType   = vk::ImageViewType::e2D,
                .format     = draw.img.format,
                .subresourceRange {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
        },  }   ).value;

        main_delete_queue.push_fn([&]{
            device.destroyImageView(draw.img.view);
            alloc.destroyImage(draw.img.handle, draw.img.alloc);
        });
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
    inline void build_descriptors() {
        std::vector<vk::DescriptorPoolSize> sizes;
        sizes.push_back(vk::DescriptorPoolSize {
            .type = vk::DescriptorType::eStorageImage,
            .descriptorCount = (u32)1 * 10,
        });

        descriptor.pool = device.createDescriptorPool(
            vk::DescriptorPoolCreateInfo {
                .flags = vk::DescriptorPoolCreateFlags(0),
                .maxSets = 10,
                .poolSizeCount = (u32)sizes.size(),
                .pPoolSizes = sizes.data(),
            },
            nullptr
        ).value;

        descriptor.set.bindings.push_back(vk::DescriptorSetLayoutBinding {
            .binding         = 0,
            .descriptorType  = vk::DescriptorType::eStorageImage,
            .descriptorCount = 1,
        });

        for( auto& bind : descriptor.set.bindings ) {
            bind.stageFlags |= vk::ShaderStageFlagBits::eCompute;
        }
        descriptor.set.layout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo {
            .flags        = vk::DescriptorSetLayoutCreateFlags(0),
            .bindingCount = (u32) descriptor.set.bindings.size(),
            .pBindings    = descriptor.set.bindings.data(),
        }).value;

        descriptor.set.handles = device.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo {
                .descriptorPool = descriptor.pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &descriptor.set.layout,
        }).value;
    }

    //_____________________________________
    inline vk::ResultValue<vk::ShaderModule> load_shader_module(const char* path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        size_t size = file.tellg();
        std::vector<u32> buffer(size / sizeof(u32));
        file.seekg(0);
        file.read((char*)buffer.data(), size);
        file.close();
        return device.createShaderModule(vk::ShaderModuleCreateInfo {
            .codeSize = buffer.size() * sizeof(u32),
            .pCode = buffer.data(),
        });
    }

    //_____________________________________
    inline void build_pipeline() {
        pipeline.layout = device.createPipelineLayout(
            vk::PipelineLayoutCreateInfo {
                .setLayoutCount = 1,
                .pSetLayouts = &descriptor.set.layout,
        }).value;

        auto compute = load_shader_module("../../shaders/gradient.comp.spv").value;
        //vk::PipelineCache cache = device.createPipelineCache(vk::PipelineCacheCreateInfo{}).value;
        pipeline.handles = device.createComputePipelines(nullptr, vk::ComputePipelineCreateInfo {
                .stage = vk::PipelineShaderStageCreateInfo {
                    .stage  = vk::ShaderStageFlagBits::eCompute,
                    .module = compute,
                    .pName  = "main",
                },
                .layout = pipeline.layout,
            }
        ).value;

        device.destroyShaderModule(compute);
        main_delete_queue.push_fn([&]{
            device.destroyPipelineLayout(pipeline.layout);
            for( auto pipe : pipeline.handles ) {
                device.destroyPipeline(pipe);
            }
        });
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
        //VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

        graphics_queue.family = get_queue_family(vk::QueueFlagBits::eGraphics);
        graphics_queue.handle = device.getQueue(graphics_queue.family, 0);

        auto alloc_info = vma::AllocatorCreateInfo {
            .flags = vma::AllocatorCreateFlagBits::eBufferDeviceAddress,
            .physicalDevice = phys_device,
            .device = device,
            .instance = instance,
        };
        alloc = vma::createAllocator(alloc_info).value;
        main_delete_queue.push_fn([&] { alloc.destroy(); });

        build_swapchain();

        build_command_structures();
        build_sync_structures();
        build_descriptors();
        build_pipeline();
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
    inline void tick() {
        auto frame = frames.at(current_frame % FRAME_OVERLAP);

        VK_CHECK(device.waitForFences(1, &frame.fence, true, UINT64_MAX));
        VK_CHECK(device.resetFences(1, &frame.fence));

        u32 img_index = device.acquireNextImageKHR(swapchain.handle, UINT64_MAX, frame.swap_semaphore).value;
        vk::Image img = swapchain.images.at(img_index);

        frame.cmd.buffer.reset();

        draw.extent.width = draw.img.extent.width;
        draw.extent.height = draw.img.extent.height;
        VK_CHECK(frame.cmd.buffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, }));
        {
            transition_img(frame.cmd.buffer, draw.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            frame.cmd.buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.handles[0]);
            frame.cmd.buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.layout, 0, descriptor.set.handles[0], nullptr);
            frame.cmd.buffer.dispatch(std::ceil(draw.extent.width / 16.f), std::ceil(draw.extent.height / 16.f), 1);
            //frame.cmd.buffer.clearColorImage(
            //    draw.img.handle,
            //    vk::ImageLayout::eGeneral,
            //    vk::ClearColorValue {{{ 0.f, 0.f, 0.f, 1.f }}},
            //    vk::ImageSubresourceRange {
            //        .aspectMask     = vk::ImageAspectFlagBits::eColor,
            //        .baseMipLevel   = 0,
            //        .levelCount     = vk::RemainingMipLevels,
            //        .baseArrayLayer = 0,
            //        .layerCount     = vk::RemainingArrayLayers,
            //    }
            //);
            //transition_img(frame.cmd.buffer, draw.img.handle, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
            transition_img(frame.cmd.buffer, img, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);

            auto blit_region = vk::ImageBlit2 {
                .srcSubresource = vk::ImageSubresourceLayers {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .srcOffsets = {{
                    vk::Offset3D { 0, 0, 0 },
                    vk::Offset3D { (int) draw.extent.width, (int) draw.extent.height, 1 },
                }},
                .dstSubresource = vk::ImageSubresourceLayers {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .dstOffsets = {{
                    vk::Offset3D { 0, 0, 0 },
                    vk::Offset3D { (int) swapchain.extent.width, (int) swapchain.extent.height, 1 },
                }},
            };
            frame.cmd.buffer.blitImage2(vk::BlitImageInfo2 {
                .srcImage = draw.img.handle,
                .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
                .dstImage = img,
                .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                .regionCount = 1,
                .pRegions = &blit_region,
                .filter = vk::Filter::eLinear,
            });

            transition_img(frame.cmd.buffer, img, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);
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
            device.destroyFence(frame.fence);
            device.destroySemaphore(frame.swap_semaphore);
            device.destroySemaphore(frame.render_semaphore);
        }
        device.destroy();
        instance.destroySurfaceKHR(surface);
        instance.destroyDebugUtilsMessengerEXT(debug.messenger);
        instance.destroy();
    }
} // sigil::renderer

struct vulkan {
    vulkan() {
        using namespace sigil;
        schedule(runlvl::init, renderer::init     );
        schedule(runlvl::tick, renderer::tick     );
        schedule(runlvl::exit, renderer::terminate);
    }
};

