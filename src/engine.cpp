#include "engine.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>
#include <cstring>
#include <map>
#include <optional>
#include <vulkan/vulkan_core.h>

namespace sigil {
    void Engine::run() {
        init_vulkan();
        while( !window.should_close() ) {
            glfwPollEvents();
        }
        vkDestroyInstance(instance, nullptr);
    }

    void Engine::init_vulkan() {
        create_instance();
    }
    
    void Engine::create_instance() {
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

        print_extensions();
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions;
        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        create_info.enabledExtensionCount = glfw_extension_count;
        create_info.ppEnabledExtensionNames = glfw_extensions;
        create_info.enabledLayerCount = 0;

        if( vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS ) {
            throw std::runtime_error("Error: Failed to create vulkan instance.");
        }
    }
    
    std::vector<const char*> Engine::get_required_extensions() {
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions;
        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
        return extensions;
    }

    void Engine::print_extensions() {
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

        std::cout << "\nVULKAN -- Available Extensions:\n";
        for( const VkExtensionProperties& extension : extensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }
    }
    
    void Engine::select_physical_device() {
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
        if( device_count == 0 ) {
            throw std::runtime_error("Error: Could not find GPU with Vulkan support.");
        }
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

        std::multimap<int, VkPhysicalDevice> candidates;

        for( const VkPhysicalDevice& device : devices ) {
            int score = score_device_suitability(device);
            candidates.insert(std::make_pair(score, device));
        }
        if( candidates.rbegin()->first > 0 ) {
            physical_device = candidates.rbegin()->second;
        } else {
            throw std::runtime_error("Error: Failed to find suitable GPU.");
        }
    }

    int Engine::score_device_suitability(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(device, &device_properties);
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures(device, &device_features);

        int score = 0;
        if( device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
            score += 1000;
        }
        score += device_properties.limits.maxImageDimension2D;
        if( !device_features.geometryShader ) {
            return 0;
        }
        return score;
    }
    
    struct  QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
    };

    QueueFamilyIndices Engine::find_queue_families(VkPhysicalDevice) {
       QueueFamilyIndices indices;
       return indices;
    }
}
