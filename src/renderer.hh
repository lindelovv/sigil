#pragma once

#include "engine.hh"
#include "system.hh"
#include "window.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_ASSERT
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace sigil {

    const vk::ApplicationInfo engine_info = {
        .pApplicationName               = "sigil",
        .applicationVersion             = VK_MAKE_VERSION(0, 0, 1),
        .pEngineName                    = "sigil",
        .engineVersion                  = VK_MAKE_VERSION(0, 0, 1),
        .apiVersion                     = VK_API_VERSION_1_3,
    };

    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation",
        //"VK_LAYER_LUNARG_api_dump",
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

    const std::string MODEL_PATH   = "models/model.obj";
    const std::string TEXTURE_PATH = "textures/model_texture.png";

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;

        bool is_complete() {
            return graphics_family.has_value()
                && present_family .has_value();
        }
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> present_modes;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 uvs;

        static vk::VertexInputBindingDescription get_binding_description() {
            vk::VertexInputBindingDescription binding_description {};
            binding_description.binding   = 0;
            binding_description.stride    = sizeof(Vertex);
            binding_description.inputRate = vk::VertexInputRate::eVertex;

            return binding_description;
        }

        static std::array<vk::VertexInputAttributeDescription, 3> get_attribute_descriptions() {
            std::array<vk::VertexInputAttributeDescription, 3> attribute_descriptions {};
            attribute_descriptions[0].binding  = 0;
            attribute_descriptions[0].location = 0;
            attribute_descriptions[0].format   = vk::Format::eR32G32B32Sfloat;
            attribute_descriptions[0].offset   = offsetof(Vertex, pos);

            attribute_descriptions[1].binding  = 0;
            attribute_descriptions[1].location = 1;
            attribute_descriptions[1].format   = vk::Format::eR32G32B32Sfloat;
            attribute_descriptions[1].offset   = offsetof(Vertex, color);

            attribute_descriptions[2].binding  = 0;
            attribute_descriptions[2].location = 2;
            attribute_descriptions[2].format   = vk::Format::eR32G32Sfloat;
            attribute_descriptions[2].offset   = offsetof(Vertex, uvs);

            return attribute_descriptions;
        }

        bool operator==(const Vertex& other) const {
            return pos   == other.pos
                && color == other.color
                && uvs   == other.uvs;
        }
    };

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    class Renderer : public System {
        public:
            virtual void init() override;
            virtual void link(Engine* engine) override;
            virtual void terminate() override;
            inline virtual bool can_tick() override { return true; };
            inline virtual void tick() override { draw(); };
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
            vk::ImageView create_img_view(
                    vk::Image image,
                    vk::Format format,
                    vk::ImageAspectFlags aspect_flags,
                    uint32_t mip_levels
                );
            void create_img_views();
            void print_extensions();
            bool is_device_suitable(vk::PhysicalDevice phys_device);
            int score_device_suitability(vk::PhysicalDevice phys_device);
            bool check_device_extension_support(vk::PhysicalDevice phys_device);
            QueueFamilyIndices find_queue_families(vk::PhysicalDevice phys_device);
            vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>&);
            vk::PresentModeKHR choose_swap_present_mode(const std::vector<vk::PresentModeKHR>&);
            vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR&);
            vk::ShaderModule create_shader_module(const std::vector<char>&);
            static std::vector<char> read_file(const std::string& path);
            void create_buffer(
                    vk::DeviceSize,
                    vk::BufferUsageFlags,
                    vk::MemoryPropertyFlags,
                    vk::Buffer&,
                    vk::DeviceMemory&
                );
            void copy_buffer(
                    vk::Buffer,
                    vk::Buffer,
                    vk::DeviceSize
                );
            void create_vertex_buffer();
            void create_index_buffer();
            void create_uniform_buffers();
            void update_uniform_buffer(uint32_t current_image);
            uint32_t find_memory_type(
                    uint32_t,
                    vk::MemoryPropertyFlags
                );
            void create_descriptor_set_layout();
            void create_descriptor_pool();
            void create_descriptor_sets();
            void create_img(
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
                    vk::CommandBuffer,
                    uint32_t
                );
            void create_sync_objects();
            vk::CommandBuffer begin_single_time_commands();
            void end_single_time_commands(vk::CommandBuffer command_buffer);
            void transition_img_layout(
                    vk::Image image,
                    vk::Format format,
                    vk::ImageLayout old_layout,
                    vk::ImageLayout new_layout,
                    uint32_t mip_levels
                );
            void copy_buffer_to_img(
                    vk::Buffer,
                    vk::Image,
                    uint32_t,
                    uint32_t
                );
            void create_depth_resources();
            vk::Format find_supported_format(
                    const std::vector<vk::Format>& candidates,
                    vk::ImageTiling tiling,
                    vk::FormatFeatureFlags features
                );
            vk::Format find_depth_format();
            bool has_stencil_component(vk::Format format);
            void load_model();
            void generate_mipmaps(
                    vk::Image image,
                    vk::Format image_format,
                    int32_t t_width,
                    int32_t t_height,
                    uint32_t mip_levels
                );
            vk::SampleCountFlagBits get_max_usable_sample_count();
            void create_color_resources();

                //// VALIDATION LAYERS ////
            VkResult create_debug_util_messenger_ext(
                    VkInstance instance, 
                    const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
                    const VkAllocationCallbacks* p_allocator,
                    VkDebugUtilsMessengerEXT* p_debug_messenger
                );
            void setup_debug_messenger();
            vk::ResultValue<std::vector<VkLayerProperties>> check_validation_layer_support();
            void destroy_debug_util_messenger_ext(
                    VkInstance instance, 
                    VkDebugUtilsMessengerEXT p_debug_messenger,
                    const VkAllocationCallbacks* p_allocator
                );
            static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
                    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                    VkDebugUtilsMessageTypeFlagsEXT msg_type,
                    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                    void* p_user_data
                );

                   //// VULKAN ////
            vk::Instance instance;
            vk::PhysicalDevice physical_device;
            vk::Device device;
            vk::Queue graphics_queue;
            vk::Queue present_queue;
            vk::SurfaceKHR surface;
            VkDebugUtilsMessengerEXT debug_messenger;
                  // SWAP CHAIN //
            vk::SwapchainKHR swap_chain;
            std::vector<vk::Image> swap_chain_images;
            vk::Format swap_chain_img_format;
            vk::Extent2D swap_chain_extent;
            std::vector<vk::ImageView> swap_chain_img_views;
            std::vector<vk::Framebuffer> swap_chain_framebuffers;
                   // PIPELINE //
            vk::RenderPass render_pass;
            vk::DescriptorSetLayout descriptor_set_layout;
            vk::DescriptorPool descriptor_pool;
            std::vector<vk::DescriptorSet> descriptor_sets;
            vk::PipelineLayout pipeline_layout;
            vk::Pipeline graphics_pipeline;
               // VERTEX & INDICES //
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            vk::Buffer vertex_buffer;
            vk::DeviceMemory vertex_buffer_memory;
            vk::Buffer index_buffer;
            vk::DeviceMemory index_buffer_memory;
            std::vector<vk::Buffer> uniform_buffers;
            std::vector<vk::DeviceMemory> uniform_buffers_memory;
            std::vector<void*> uniform_buffers_mapped;
            uint32_t mip_levels;
            vk::Image texture_image;
            vk::DeviceMemory texture_image_memory;
            vk::ImageView texture_image_view;
            vk::Sampler texture_sampler;
            vk::Image color_img;
            vk::DeviceMemory color_img_memory;
            vk::ImageView color_img_view;
                   // DEPTH //
            vk::Image depth_img;
            vk::DeviceMemory depth_img_memory;
            vk::ImageView depth_img_view;
                // RENDER PASS //
            vk::CommandPool command_pool;
            std::vector<vk::CommandBuffer> command_buffers;
            std::vector<vk::Semaphore> img_available_semaphores;
            std::vector<vk::Semaphore> render_finished_semaphores;
            std::vector<vk::Fence> in_flight_fences;
            bool framebuffer_resized = false;
            uint32_t current_frame = 0;
            vk::SampleCountFlagBits msaa_samples = vk::SampleCountFlagBits::e1;
            Window* window;
    };
    extern Renderer renderer;
}

namespace std {
    template<> struct hash<sigil::Vertex> {
        size_t operator()(sigil::Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                    (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                    (hash<glm::vec2>()(vertex.uvs)   << 1);
        }
    };
}

