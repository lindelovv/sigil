#pragma once

#include "sigil.hh"
#include "math.hh"

#include <iostream>
#include <memory>

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "vk_mem_alloc.hpp"

namespace sigil::renderer {

// TODO: VkResult checker that can return value and result or just value
#define VK_CHECK(x) { vk::Result e = (vk::Result)x; if((VkResult)e) {  \
    std::cout << "__VK_CHECK failed__ "                                 \
    << vk::to_string(e) << " @ " << __FILE__ << "_" << __LINE__ << "\n"; \
} }
#define SIGIL_V VK_MAKE_VERSION(version::major, version::minor, version::patch)

    const vk::ApplicationInfo engine_info {
        .pApplicationName                   = "sigil",
        .applicationVersion                 = SIGIL_V,
        .pEngineName                        = "sigil",
        .engineVersion                      = SIGIL_V,
        .apiVersion                         = VK_API_VERSION_1_3,
    };

    const std::vector<const char*> _layers      { "VK_LAYER_KHRONOS_validation", };
    const std::vector<const char*> _device_exts {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    };

    //_____________________________________
    // DEBUG
    struct {
        vk::DebugUtilsMessengerEXT messenger;

        static VKAPI_ATTR VkBool32 VKAPI_CALL callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
            VkDebugUtilsMessageTypeFlagsEXT msg_type,
            const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
            void* p_user_data
        ) {
            std::cerr << ">> " << p_callback_data->pMessage << "\n"
                      << "---------------------------------------\n";
            return false;
        }
    } inline debug;

    //_____________________________________
    inline vk::Instance        instance;
    inline vk::SurfaceKHR      surface;
    inline vk::PhysicalDevice  phys_device;
    inline vk::Device          device;

    struct { vk::Queue handle;
             u32       family;
    } inline graphics_queue;

    //_____________________________________
    inline vma::Allocator      vma_allocator;

    struct AllocatedBuffer {
        vk::Buffer          handle;
        vma::Allocation     allocation;
        vma::AllocationInfo info;
    };

    //_____________________________________
    struct { f32 frametime;
             u32 triangles;
             u32 drawcalls;
             f32 scene_update_time;
             f32 mesh_draw_time;
    } inline _stats;

    //_____________________________________
    inline vk::Sampler _sampler_linear;
    inline vk::Sampler _sampler_nearest;

    //_____________________________________
    struct { vk::Pipeline       handle;
             vk::PipelineLayout layout;
    } inline compute_pipeline;

    //_____________________________________
    struct { vk::Fence         fence;
             vk::CommandBuffer cmd;
             vk::CommandPool   pool;
    } inline immediate;

    //_____________________________________
    struct ComputePushConstants {
        glm::vec4 data1;
        glm::vec4 data2;
        glm::vec4 data3;
        glm::vec4 data4;
    };

    //_____________________________________
    struct ComputeEffect {
        const char* name;
        vk::Pipeline pipeline;
        vk::PipelineLayout pipeline_layout;
        ComputePushConstants data;
    };
    inline std::vector<ComputeEffect> bg_effects;
    inline int current_bg_effect { 0 };

    //_____________________________________
    struct Vertex {
        glm::vec3 position;
        float uv_x;
        glm::vec3 normal;
        float uv_y;
        glm::vec4 color;
    };

    struct GPUMeshBuffer {
        AllocatedBuffer index_buffer;
        AllocatedBuffer vertex_buffer;
        vk::DeviceAddress vertex_buffer_address;
    };

    struct GPUDrawPushConstants {
        glm::mat4 world_matrix;
        vk::DeviceAddress vertex_buffer;
        float time;
    };

    //_____________________________________
    struct MaterialPipeline {
        vk::Pipeline       handle;
        vk::PipelineLayout layout;
    };

    struct MaterialInstance {
        MaterialPipeline* pipeline;
        vk::DescriptorSet material_set;
    } inline _default_material_instance;
    inline MaterialInstance _wave_material_instance;

    struct Bounds {
        glm::vec3 origin;
        glm::vec3 extents;
        f32 sphere_radius;
    };

    //_____________________________________
    struct GeoSurface {
        u32 start_index;
        u32 count;
        Bounds bounds;
        std::shared_ptr<struct PbrMaterial> material;
    };

    struct Mesh {
        size_t id;
        std::vector<GeoSurface> surfaces;
        GPUMeshBuffer mesh_buffer;
    };

    struct RenderObject {
        u32 count;
        u32 first;
        vk::Buffer index_buffer;
        MaterialInstance* material;
        Bounds            bounds;
        glm::mat4         transform;
        vk::DeviceAddress address;
    };

    //_____________________________________
    struct GPUSceneData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 view_proj;
        glm::vec4 ambient_color;
        glm::vec4 sun_dir;
        glm::vec4 sun_color;
    } inline _scene_data;
    inline vk::DescriptorSetLayout _scene_data_layout;

    namespace primitives {
        struct Plane {
            std::vector<Vertex> vertices;
            std::vector<u32> indices;
            std::vector<u32> uvs;
            GeoSurface surface;

            Plane() : Plane(10, 10) {};
            Plane(u32 div, f32 width);
        };
    }

    inline std::vector<Mesh> _meshes;
    inline std::vector<RenderObject> _opaque_objects;
    inline std::vector<RenderObject> _transparent_objects;

    void init();
    void setup_keybinds();
    void setup_imgui();
    void build_swapchain();
    void rebuild_swapchain();
    u32 get_queue_family(vk::QueueFlagBits type);
    void build_command_structures();
    void build_sync_structures();
    void build_descriptors();
    void immediate_submit(fn<void(vk::CommandBuffer cmd)>&& fn);
    AllocatedBuffer create_buffer(size_t size, vk::BufferUsageFlags usage, vma::MemoryUsage mem_usage);
    void destroy_buffer(const AllocatedBuffer& buffer);
    GPUMeshBuffer upload_mesh(std::span<u32> indices, std::span<Vertex> vertices);
    vk::ResultValue<vk::ShaderModule> load_shader_module(const char* path);
    std::vector<std::shared_ptr<Mesh>> load_model(const char* path);
    void build_compute_pipeline();
    void update_scene();
    bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);
    void draw_geometry(struct FrameData& frame, vk::CommandBuffer cmd);
    void tick();
    void terminate();

    namespace ui {
        inline bool show_info = false;
        void draw(vk::CommandBuffer cmd, vk::ImageView img_view);
    }

} // sigil::renderer

struct vulkan {
    vulkan() {
        using namespace sigil;
        schedule(eInit, renderer::init     );
        schedule(eTick, renderer::tick     );
        schedule(eExit, renderer::terminate);
    }
};

