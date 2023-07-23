#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace sigil {

    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*>  device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    const int MAX_FRAMES_IN_FLIGHT = 2;

    #ifdef NDEBUG
        const bool enable_validation_layers = false;
    #else
        const bool enable_validation_layers = true;
    #endif

    const std::string MODEL_PATH = "models/model.obj";
    const std::string TEXTURE_PATH = "textures/model_texture.png";

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;

        bool is_complete() {
            return graphics_family.has_value()
                && present_family.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texture_coords;

        static VkVertexInputBindingDescription get_binding_description() {
            VkVertexInputBindingDescription binding_description {};
            binding_description.binding = 0;
            binding_description.stride = sizeof(Vertex);
            binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return binding_description;
        }

        static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions {};
            attribute_descriptions[0].binding = 0;
            attribute_descriptions[0].location = 0;
            attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attribute_descriptions[0].offset = offsetof(Vertex, pos);

            attribute_descriptions[1].binding = 0;
            attribute_descriptions[1].location = 1;
            attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attribute_descriptions[1].offset = offsetof(Vertex, color);

            attribute_descriptions[2].binding = 0;
            attribute_descriptions[2].location = 2;
            attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attribute_descriptions[2].offset = offsetof(Vertex, texture_coords);

            return attribute_descriptions;
        }

        bool operator==(const Vertex& other) const {
            return pos == other.pos
                && color == other.color
                && texture_coords == other.texture_coords;
        }
    };

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    class Renderer {
        public:
            void init();
            void terminate();
            void draw();
            static void resize_callback(
                    GLFWwindow* window,
                    int width,
                    int height
                );

        private:
            void create_instance();
            void create_surface();
            void select_physical_device();
            void create_logical_device();
            void create_swap_chain();
            void cleanup_swap_chain();
            void recreate_swap_chain();
            VkImageView create_img_view(
                    VkImage image,
                    VkFormat format,
                    VkImageAspectFlags aspect_flags,
                    uint32_t mip_levels
                );
            void create_img_views();
            void print_extensions();
            std::vector<const char*> get_required_extensions();
            bool is_device_suitable(
                    VkPhysicalDevice physical_device
                );
            int score_device_suitability(
                    VkPhysicalDevice physical_device
                );
            bool check_device_extension_support(
                    VkPhysicalDevice physical_device
                );
            QueueFamilyIndices find_queue_families(
                    VkPhysicalDevice physical_device
                );
            SwapChainSupportDetails query_swap_chain_support(
                    VkPhysicalDevice physical_device
                );
            VkSurfaceFormatKHR choose_swap_surface_format(
                    const std::vector<VkSurfaceFormatKHR>&
                );
            VkPresentModeKHR choose_swap_present_mode(
                    const std::vector<VkPresentModeKHR>&
                );
            VkExtent2D choose_swap_extent(
                    const VkSurfaceCapabilitiesKHR&
                );
            VkShaderModule create_shader_module(
                    const std::vector<char>&
                );
            static std::vector<char> read_file(
                    const std::string& path
                );
            void create_buffer(
                    VkDeviceSize,
                    VkBufferUsageFlags,
                    VkMemoryPropertyFlags,
                    VkBuffer&,
                    VkDeviceMemory&
                );
            void copy_buffer(
                    VkBuffer,
                    VkBuffer,
                    VkDeviceSize
                );
            void create_vertex_buffer();
            void create_index_buffer();
            void create_uniform_buffers();
            void update_uniform_buffer(
                    uint32_t current_image
                );
            uint32_t find_memory_type(
                    uint32_t,
                    VkMemoryPropertyFlags
                );
            void create_descriptor_set_layout();
            void create_descriptor_pool();
            void create_descriptor_sets();
            void create_img(
                    uint32_t width,
                    uint32_t height,
                    uint32_t mip_levels,
                    VkSampleCountFlagBits num_samples,
                    VkFormat format,
                    VkImageTiling tiling,
                    VkImageUsageFlags usage,
                    VkMemoryPropertyFlags properties,
                    VkImage& image,
                    VkDeviceMemory& image_memory
                );
            void create_texture_img();
            void create_texture_img_view();
            void create_texture_sampler();
            void create_graphics_pipeline();
            void create_render_pass();
            void create_framebuffers();
            void create_command_pool();
            void create_command_buffers();
            void record_command_buffer(
                    VkCommandBuffer,
                    uint32_t
                );
            void create_sync_objects();
            VkCommandBuffer begin_single_time_commands();
            void end_single_time_commands(
                    VkCommandBuffer command_buffer
                );
            void transition_img_layout(
                    VkImage image,
                    VkFormat format,
                    VkImageLayout old_layout,
                    VkImageLayout new_layout,
                    uint32_t mip_levels
                );
            void copy_buffer_to_img(
                    VkBuffer,
                    VkImage,
                    uint32_t,
                    uint32_t
                );
            void create_depth_resources();
            VkFormat find_supported_format(
                    const std::vector<VkFormat>& candidates,
                    VkImageTiling tiling,
                    VkFormatFeatureFlags features
                );
            VkFormat find_depth_format();
            bool has_stencil_component(VkFormat format);
            void load_model();
            void generate_mipmaps(
                    VkImage image,
                    VkFormat image_format,
                    int32_t t_width,
                    int32_t t_height,
                    uint32_t mip_levels
                );
            VkSampleCountFlagBits get_max_usable_sample_count();
            void create_color_resources();

                //// VALIDATION LAYERS ////
            VkResult create_debug_util_messenger_ext(
                    VkInstance instance, 
                    const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
                    const VkAllocationCallbacks* p_allocator,
                    VkDebugUtilsMessengerEXT* p_debug_messenger
                );
            void setup_debug_messenger();
            void populate_debug_messenger_create_info(
                    VkDebugUtilsMessengerCreateInfoEXT& create_info
                );
            bool check_validation_layer_support();
            void destroy_debug_util_messenger_ext(
                    VkInstance instance, 
                    VkDebugUtilsMessengerEXT p_debug_messenger,
                    const VkAllocationCallbacks* p_allocator
                );
            static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
                    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                    VkDebugUtilsMessageTypeFlagsEXT msg_type,
                    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                    void* p_user_data
                );

                   //// VULKAN ////
            VkInstance instance;
            VkPhysicalDevice physical_device = VK_NULL_HANDLE;
            VkDevice device;
            VkQueue graphics_queue;
            VkQueue present_queue;
            VkSurfaceKHR surface;
            VkDebugUtilsMessengerEXT debug_messenger;
                  // SWAP CHAIN //
            VkSwapchainKHR swap_chain;
            std::vector<VkImage> swap_chain_images;
            VkFormat swap_chain_img_format;
            VkExtent2D swap_chain_extent;
            std::vector<VkImageView> swap_chain_img_views;
            std::vector<VkFramebuffer> swap_chain_framebuffers;
                   // PIPELINE //
            VkRenderPass render_pass;
            VkDescriptorSetLayout descriptor_set_layout;
            VkDescriptorPool descriptor_pool;
            std::vector<VkDescriptorSet> descriptor_sets;
            VkPipelineLayout pipeline_layout;
            VkPipeline graphics_pipeline;
               // VERTEX & INDICES //
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            VkBuffer vertex_buffer;
            VkDeviceMemory vertex_buffer_memory;
            VkBuffer index_buffer;
            VkDeviceMemory index_buffer_memory;
            std::vector<VkBuffer> uniform_buffers;
            std::vector<VkDeviceMemory> uniform_buffers_memory;
            std::vector<void*> uniform_buffers_mapped;
            uint32_t mip_levels;
            VkImage texture_image;
            VkDeviceMemory texture_image_memory;
            VkImageView texture_image_view;
            VkSampler texture_sampler;
            VkImage color_img;
            VkDeviceMemory color_img_memory;
            VkImageView color_img_view;
                   // DEPTH //
            VkImage depth_img;
            VkDeviceMemory depth_img_memory;
            VkImageView depth_img_view;
                // RENDER PASS //
            VkCommandPool command_pool;
            std::vector<VkCommandBuffer> command_buffers;
            std::vector<VkSemaphore> img_available_semaphores;
            std::vector<VkSemaphore> render_finished_semaphores;
            std::vector<VkFence> in_flight_fences;
            bool framebuffer_resized = false;
            uint32_t current_frame = 0;
            VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    };
}

namespace std {
    template<> struct hash<sigil::Vertex> {
        size_t operator()(sigil::Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                    (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                    (hash<glm::vec2>()(vertex.texture_coords) << 1);
        }
    };
}
