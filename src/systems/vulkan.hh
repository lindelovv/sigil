#pragma once

#include "sigil.hh"
#include "glfw.hh"

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
            vk::SwapchainKHR handle;
            vk::Format img_format;
            std::vector<vk::Image> images;
            std::vector<vk::ImageView> img_views;
            vk::Extent2D extent;
        } inline swapchain;

        inline void init() {
            uint32_t glfw_extension_count = 0;
            const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
            std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
            
            instance = vk::createInstance(
                vk::InstanceCreateInfo {
                    .pNext                      = nullptr,
                    .pApplicationInfo           = &engine_info,
                    .enabledLayerCount          = 0,
                    .enabledExtensionCount      = static_cast<uint32_t>(extensions.size()),
                    .ppEnabledExtensionNames    = extensions.data(),
            }   ).value;

            glfwCreateWindowSurface(instance, sigil::window::handle, nullptr, (VkSurfaceKHR*)&surface);

            phys_device = instance.enumeratePhysicalDevices().value[0]; // TODO: propperly pick GPU
            device = phys_device.createDevice(vk::DeviceCreateInfo{}, nullptr).value;
        }

        inline void draw() {

        }

        inline void create_swapchain(u32 w, u32 h) {
            
        }

        inline void terminate() {
            device.destroySwapchainKHR(swapchain.handle);
            for( auto view : swapchain.img_views ) {
                device.destroyImageView(view);
            }
        }
    };
};
struct vulkan {
    vulkan() {
        using namespace sigil;
        schedule(runlvl::init, renderer::init     );
        schedule(runlvl::tick, renderer::draw     );
        schedule(runlvl::exit, renderer::terminate);
    }
};

