#pragma once

#include "sigil.hh"
#include "glfw.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <math.h>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

using namespace sigil;

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

const std::string MODEL_PATH   = "models/model.obj";
const std::string TEXTURE_PATH = "textures/model_texture.png";

//____________________________________
// 
struct renderer {
    renderer() {
        sigil::add_init_fn([&]{ this->init();      });
        sigil::add_tick_fn([&]{ this->draw();      });
        sigil::add_exit_fn([&]{ this->terminate(); });
    }
    void init();
    void terminate();
    void draw();

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
    
    struct TextureData {
        vk::Image               image;
        vk::DeviceMemory        image_memory;
        vk::ImageView           image_view;
        uint32_t                mip_levels;
        vk::DescriptorSet       descriptor_set;
        vk::Sampler             sampler;
        const std::string       path;
    };
    
    struct Mesh {
        struct {
            std::vector<Vertex>     pos;
            vk::Buffer              buffer;
            vk::DeviceMemory        memory;
        } vertices;
        struct {
            std::vector<uint32_t>   unique;
            uint32_t                count;
            vk::Buffer              buffer;
            vk::DeviceMemory        memory;
        } indices;
        std::vector<TextureData>    textures;
    };
    
    struct Model {
        std::vector<Mesh> meshes;
    };
    
    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };
    
    //____________________________________
    // @TODO: implement more integrated ui instead of ImGui [low-priority]
    struct Ui {
    };

    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    vk::Queue graphics_queue;
    vk::Queue present_queue;
    vk::SurfaceKHR surface;
    vk::DebugUtilsMessengerEXT debug_messenger;
    struct {
        vk::SwapchainKHR handle;
        std::vector<vk::Image> images;
        vk::Format img_format;
        vk::Extent2D extent;
        std::vector<vk::ImageView> img_views;
        std::vector<vk::Framebuffer> framebuffers;
    } swapchain;
    vk::RenderPass render_pass;
    struct {
        vk::DescriptorPool pool;
        vk::DescriptorSetLayout uniform_set_layout;
        vk::DescriptorSetLayout texture_set_layout;
        std::vector<vk::DescriptorSet> sets;
    } descriptor;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline graphics_pipeline;
       // VERTEX & INDICES //
    //std::vector<Vertex> vertices;
    //std::vector<uint32_t> indices;

    std::vector<vk::Buffer> uniform_buffers;
    std::vector<vk::DeviceMemory> uniform_buffers_memory;
    std::vector<void*> uniform_buffers_mapped;
    std::vector<TextureData> textures {
        { .path = "textures/model_texture.png" },
        { .path = "textures/missing_texture.jpg" }
    };
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

    std::vector<Model> models;

            // WORLD //
    inline static glm::vec3 world_up = glm::vec3(0.f, 0.f, -1.f);

      // FIRST PERSON CAMERA //
    struct {
        struct {
            glm::vec3 position = glm::vec3( 0, 1.8, .6f );
            glm::vec3 rotation = glm::vec3( 0, 0,  0 );
            glm::vec3 scale    = glm::vec3( 0 );
        } transform;
        float fov =  90.f;
        float yaw = -90.f;
        float pitch = -13.f;
        glm::vec3 velocity;
        glm::vec3 forward_vector = glm::normalize(glm::vec3(
                    cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(pitch))
                ));
        glm::vec3 up_vector      = -world_up;
        glm::vec3 right_vector   = glm::cross(forward_vector, world_up);
        struct {
            float near =  0.01f;
            float far  = 256.0f;
        } clip_plane;
        struct {
            bool none;
            bool forward;
            bool right;
            bool back; 
            bool left; 
            bool up; 
            bool down; 
        } request_movement;
        float movement_speed = 1.f;
        bool follow_mouse = false;
        float mouse_sens = 12.f;

        inline void update(float delta_time) {
            if( request_movement.forward ) velocity += forward_vector;
            if( request_movement.back    ) velocity -= forward_vector;
            if( request_movement.right   ) velocity -= right_vector;
            if( request_movement.left    ) velocity += right_vector;
            if( request_movement.up      ) velocity += up_vector;
            if( request_movement.down    ) velocity -= up_vector;
            transform.position += velocity * movement_speed * delta_time;
            velocity = glm::vec3(0);
            if( follow_mouse ) {
                glm::dvec2 offset = -input::get_mouse_movement() * glm::dvec2(mouse_sens) * glm::dvec2(delta_time);
                yaw += offset.x;
                pitch = ( pitch + offset.y >  89.f ?  89.f
                        : pitch + offset.y < -89.f ? -89.f
                        : pitch + offset.y);
                forward_vector = glm::normalize(glm::vec3(
                    cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(pitch))
                ));
                right_vector = glm::cross(forward_vector, world_up);
                up_vector = glm::cross(forward_vector, right_vector);
                //transform.rotation = glm::qrot(transform.rotation, forward_vector);
            }
        }
        inline glm::mat4 get_view() {
            return glm::lookAt(transform.position,
                               transform.position + forward_vector,
                               up_vector);
        }
    } camera;

    void create_swap_chain();
    void cleanup_swap_chain();
    void recreate_swap_chain();
    vk::ImageView create_img_view(vk::Image image,
                                  vk::Format format,
                                  vk::ImageAspectFlags aspect_flags,
                                  uint32_t mip_levels
    );
    void create_img_views();
    QueueFamilyIndices find_queue_families(vk::PhysicalDevice phys_device);
    vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>&);
    vk::PresentModeKHR choose_swap_present_mode(const std::vector<vk::PresentModeKHR>&);
    vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR&);
    vk::ShaderModule create_shader_module(const std::vector<char>&);
    static std::vector<char> read_file(const std::string& path);
    void create_buffer(vk::DeviceSize,
                       vk::BufferUsageFlags,
                       vk::MemoryPropertyFlags,
                       vk::Buffer&,
                       vk::DeviceMemory&
    );
    void copy_buffer(vk::Buffer,
                     vk::Buffer,
                     vk::DeviceSize
    );
    void update_uniform_buffer(uint32_t current_image);
    uint32_t find_memory_type(uint32_t,
                              vk::MemoryPropertyFlags
    );
    void create_img(uint32_t width,
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
    void create_framebuffers();
    void record_command_buffer(vk::CommandBuffer, uint32_t);
    vk::CommandBuffer begin_single_time_commands();
    void end_single_time_commands(vk::CommandBuffer command_buffer);
    void transition_img_layout(vk::Image image,
                               vk::Format format,
                               vk::ImageLayout old_layout,
                               vk::ImageLayout new_layout,
                               uint32_t mip_levels
    );
    void copy_buffer_to_img(vk::Buffer,
                            vk::Image,
                            uint32_t,
                            uint32_t
    );
    void create_depth_resources();
    vk::Format find_supported_format(const std::vector<vk::Format>& candidates,
                                     vk::ImageTiling tiling,
                                     vk::FormatFeatureFlags features
    );
    vk::Format find_depth_format();
    bool has_stencil_component(vk::Format format);
    void load_model();
    void generate_mipmaps(vk::Image image,
                          vk::Format image_format,
                          int32_t t_width,
                          int32_t t_height,
                          uint32_t mip_levels
    );
    void create_color_resources();

        //// VALIDATION LAYERS ////
    static VKAPI_ATTR VkResult VKAPI_CALL
    create_debug_util_messenger_ext(VkInstance instance, 
                                    const VkDebugUtilsMessengerCreateInfoEXT* p_create_info,
                                    const VkAllocationCallbacks* p_allocator,
                                    VkDebugUtilsMessengerEXT* p_debug_messenger
    );
    static VKAPI_ATTR void VKAPI_CALL
    destroy_debug_util_messenger_ext(VkInstance instance, 
                                     VkDebugUtilsMessengerEXT p_debug_messenger,
                                     const VkAllocationCallbacks* p_allocator
    );
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL
    debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                   VkDebugUtilsMessageTypeFlagsEXT msg_type,
                   const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                   void* p_user_data
    );
};

namespace std {
    template<> struct hash<renderer::Vertex> {
        size_t operator()(renderer::Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                    (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                    (hash<glm::vec2>()(vertex.uvs)   << 1);
        }
    };
}

