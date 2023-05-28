#pragma once

#include <string>
#include <vector>
#include <optional>
#include <iostream>

#include <vulkan/vulkan_core.h>

namespace sigil {

    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    #ifdef NDEBUG
        const bool enable_validation_layers = false;
    #else
        const bool enable_validation_layers = true;
    #endif

    struct QueueFamilyIndices {
        std::optional<uint32_t>       graphics_family;
        std::optional<uint32_t>        present_family;

        bool is_complete() {
            return         graphics_family.has_value()
                        && present_family.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR         capabilities;
        std::vector<VkSurfaceFormatKHR>       formats;
        std::vector<VkPresentModeKHR>   present_modes;
    };

    class Renderer {
        public:
            void                                          init();
            void                                     terminate();
            void                                          draw();

        private:
            void                               create_instance();
            void                                create_surface();
            void                        select_physical_device();
            void                         create_logical_device();
            void                             create_swap_chain();
            void                              create_img_views();
            void                              print_extensions();
            std::vector<const char*>   get_required_extensions();
            int       score_device_suitability(VkPhysicalDevice);
            bool check_device_extension_support(VkPhysicalDevice);
            QueueFamilyIndices find_queue_families(VkPhysicalDevice);
            SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice);
            VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>&);
            VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>&);
            VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR&);
            VkShaderModule create_shader_module(const std::vector<char>&);
            static std::vector<char> read_file(const std::string& path);
            void                      create_graphics_pipeline();
            void                            create_render_pass();
            void                           create_framebuffers();
            void                           create_command_pool();
            void                         create_command_buffer();
            void record_command_buffer(VkCommandBuffer, uint32_t);
            void                           create_sync_objects();

                         //// VALIDATION LAYERS ////
            VkResult create_debug_util_messenger_ext(
                    VkInstance instance, 
                    const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
                    const VkAllocationCallbacks* p_allocator,
                    VkDebugUtilsMessengerEXT* p_debug_messenger
                    );
            void                         setup_debug_messenger();
            void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info);
            bool                check_validation_layer_support();
            static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
                    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                    VkDebugUtilsMessageTypeFlagsEXT msg_type,
                    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                    void* p_user_data) {
                std::cerr << "Validation layer: " << p_callback_data->pMessage << "\n";
                return VK_FALSE;
            }
            VkDebugUtilsMessengerEXT             debug_messenger;
            void destroy_debug_util_messenger_ext(
                    VkInstance instance, 
                    VkDebugUtilsMessengerEXT p_debug_messenger,
                    const VkAllocationCallbacks* p_allocator
                    );

                              //// VULKAN ////
            VkInstance                                  instance;
            VkPhysicalDevice    physical_device = VK_NULL_HANDLE;
            VkDevice                                      device;
            VkQueue                               graphics_queue;
            VkQueue                                present_queue;
            VkSurfaceKHR                                 surface;
            const std::vector<const char*>  device_extensions = {
                                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };
                              // SWAP CHAIN //
            VkSwapchainKHR                            swap_chain;
            std::vector<VkImage>               swap_chain_images;
            VkFormat                       swap_chain_img_format;
            VkExtent2D                         swap_chain_extent;
            std::vector<VkImageView>        swap_chain_img_views;
            std::vector<VkFramebuffer>   swap_chain_framebuffers;
                              // PIPELINE //
            VkRenderPass                             render_pass;
            VkPipelineLayout                     pipeline_layout;
            VkPipeline                         graphics_pipeline;
                            // RENDER PASS //
            VkCommandPool                           command_pool;
            VkCommandBuffer                       command_buffer;
            VkSemaphore                  img_available_semaphore;
            VkSemaphore                render_finished_semaphore;
            VkFence                              in_flight_fence;
    };
}
