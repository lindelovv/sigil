#pragma once

#include "sigil.hh"
#include "glfw.hh"

#include <GLFW/glfw3.h>
#include <assimp/mesh.h>
#include <fstream>
#include <memory>

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "vk_mem_alloc.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "assimp/Importer.hpp"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace sigil::renderer {

#define VK_CHECK(x) { vk::Result e = (vk::Result)x; if((VkResult)e) {       \
    std::cout << "__VK_CHECK failed__ "                                      \
    << vk::to_string(e) << " @ " << __FILE_NAME__ << "_" << __LINE__ << "\n"; \
} }
#define SIGIL_V VK_MAKE_VERSION(version::major, version::minor, version::patch)

    const vk::ApplicationInfo engine_info = {
        .pApplicationName                       = "sigil",
        .applicationVersion                     = SIGIL_V,
        .pEngineName                            = "sigil",
        .engineVersion                          = SIGIL_V,
        .apiVersion                             = VK_API_VERSION_1_3,
    };

    const std::vector<const char*> layers            = { "VK_LAYER_KHRONOS_validation", };
    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_SHADER_CLOCK_EXTENSION_NAME,
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
            std::cerr << "\n>> " << p_callback_data->pMessage << "\n";
            return false;
        }
    } inline debug;

    //_____________________________________
    inline vk::Instance         instance;
    inline vk::SurfaceKHR       surface;
    inline vk::PhysicalDevice   phys_device;
    inline vk::Device           device;

    //_____________________________________
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

    //_____________________________________
    struct {
        vk::Queue handle;
        u32       family;
    } inline graphics_queue;

    //_____________________________________
    struct AllocatedImage {
        struct {
            vk::Image       handle;
            vk::ImageView   view;
            vma::Allocation alloc;
            vk::Extent3D    extent;
            vk::Format      format;
        } img;
        vk::Extent2D extent;
    };
    inline AllocatedImage _draw;
    inline AllocatedImage _depth;

    //_____________________________________
    static vma::Allocator alloc;

    //_____________________________________
    struct {
        vk::DescriptorPool pool;
        struct {
            vk::DescriptorSet handle;
            vk::DescriptorSetLayout layout;
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
        } set;
    } inline descriptor;

    //_____________________________________
    struct {
        vk::Pipeline handle;
        vk::PipelineLayout layout;
    } inline compute_pipeline;

    struct {
        vk::Pipeline handle;
        u32 index;
        vk::PipelineLayout layout;
        vk::RenderPass pass;
        u32 subpass;
        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
        vk::DynamicState state[2] = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };
    } inline graphics_pipeline;

    //_____________________________________
    struct {
        vk::Fence         fence;
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

    struct AllocBuffer {
        vk::Buffer handle;
        vma::Allocation allocation;
        vma::AllocationInfo info;
    };

    struct Vertex {
        glm::vec3 position;
        float uv_x;
        glm::vec3 normal;
        float uv_y;
        glm::vec4 color;
    };

    struct GPUMeshBuffer {
        AllocBuffer index_buffer;
        AllocBuffer vertex_buffer;
        vk::DeviceAddress vertex_buffer_address;
    };

    struct GPUDrawPushConstants {
        glm::mat4 world_matrix;
        vk::DeviceAddress vertex_buffer;
    };

    struct GeoSurface {
        u32 start_index;
        u32 count;
    };

    struct Mesh {
        std::string name;
        std::vector<GeoSurface> surfaces;
        GPUMeshBuffer mesh_buffer;
    };

    inline std::vector<Mesh> meshes;

    //_____________________________________
    inline static glm::vec3 world_up = glm::vec3(0.f, 0.f, -1.f);

    struct {
        struct {
            glm::vec3 position = glm::vec3( 1, 0, 0 );
            glm::vec3 rotation = glm::vec3( 0, 0, 0 );
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
    } inline camera;

    //_____________________________________
    namespace ui {

        inline void draw(vk::CommandBuffer cmd, vk::ImageView img_view) {

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ComputeEffect& selected = bg_effects[current_bg_effect];

            ImGui::Begin("Sigil", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(fmt::format("GPU: {}", phys_device.getProperties().deviceName.data()).c_str());
                ImGui::TextUnformatted(fmt::format("sigil   {}", sigil::version::as_string).c_str());
                ImGui::SetWindowPos(ImVec2(0, swapchain.extent.height - ImGui::GetWindowSize().y));
            }
            ImGui::End();

            ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoTitleBar   //| ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(
                    fmt::format(" Camera position:\n\tx: {:.3f}\n\ty: {:.3f}\n\tz: {:.3f}",
                    camera.transform.position.x, camera.transform.position.y, camera.transform.position.z).c_str()
                );
                ImGui::TextUnformatted(
                    fmt::format(" Yaw:   {:.2f}\n Pitch: {:.2f}",
                    camera.yaw, camera.pitch).c_str()
                );
                ImGui::TextUnformatted(
                    fmt::format(" Mouse position:\n\tx: {:.0f}\n\ty: {:.0f}",
                    input::mouse_position.x, input::mouse_position.y ).c_str()
                );
                ImGui::TextUnformatted(
                    fmt::format(" Mouse offset:\n\tx: {:.0f}\n\ty: {:.0f}",
                    input::get_mouse_movement().x, input::get_mouse_movement().y).c_str()
                );

                ImGui::TextUnformatted(fmt::format("Selected effect: {}", selected.name).c_str());
                ImGui::SliderInt("Effect Index", &current_bg_effect, 0, bg_effects.size() - 1);
                ImGui::InputFloat4("data1", (float*)& selected.data.data1);
                ImGui::InputFloat4("data2", (float*)& selected.data.data2);
                ImGui::InputFloat4("data3", (float*)& selected.data.data3);
                ImGui::InputFloat4("data4", (float*)& selected.data.data4);

                ImGui::SetWindowSize(ImVec2(108, 182));
                ImGui::SetWindowPos(ImVec2(8, 8));
            }
            ImGui::End();

            ImGui::Begin("Framerate", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(fmt::format(" FPS: {:.0f}", time::fps).c_str());
                ImGui::TextUnformatted(fmt::format(" ms: {:.2f}", time::ms).c_str());
                ImGui::SetWindowSize(ImVec2(82, 64));
                ImGui::SetWindowPos(ImVec2(swapchain.extent.width - ImGui::GetWindowSize().x, 8));
            }
            ImGui::End();
            ImGui::Render();

            vk::RenderingAttachmentInfo attach_info {
                .imageView = img_view,
                .imageLayout = vk::ImageLayout::eGeneral,
                .loadOp = vk::AttachmentLoadOp::eLoad,
                .storeOp = vk::AttachmentStoreOp::eStore,
            };
            vk::RenderingInfo render_info {
                .renderArea = vk::Rect2D { vk::Offset2D { 0, 0 }, swapchain.extent },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &attach_info,
                .pDepthAttachment = nullptr,
                .pStencilAttachment = nullptr,
            };
            cmd.beginRendering(&render_info);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
            cmd.endRendering();
        }
    } // ui

    //_____________________________________
    inline void build_swapchain() {
        int w, h;
        glfwGetFramebufferSize(window::handle, &w, &h);

        auto capabilities = phys_device.getSurfaceCapabilitiesKHR(surface).value;
        swapchain.handle = device.createSwapchainKHR(
            vk::SwapchainCreateInfoKHR {
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
        for( auto image : swapchain.images ) {
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
        _draw.img = {
            .extent = vk::Extent3D { .width = (u32)w, .height = (u32)h, .depth = 1 },
            .format = vk::Format::eR16G16B16A16Sfloat,
        };
        std::tie( _draw.img.handle, _draw.img.alloc ) = alloc.createImage(
            vk::ImageCreateInfo {
                .imageType = vk::ImageType::e2D,
                .format = _draw.img.format,
                .extent = _draw.img.extent,
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
        _draw.img.view = device.createImageView(
            vk::ImageViewCreateInfo {
                .image      = _draw.img.handle,
                .viewType   = vk::ImageViewType::e2D,
                .format     = _draw.img.format,
                .subresourceRange {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
        },  }   ).value;

        _depth.img = {
            .extent = _draw.img.extent,
            .format = vk::Format::eD32Sfloat,
        };
        std::tie( _depth.img.handle, _depth.img.alloc ) = alloc.createImage(
            vk::ImageCreateInfo {
                .imageType = vk::ImageType::e2D,
                .format = _depth.img.format,
                .extent = _depth.img.extent,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            },
            vma::AllocationCreateInfo {
                .usage = vma::MemoryUsage::eGpuOnly,
                .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
            }
        ).value;
        _depth.img.view = device.createImageView(
            vk::ImageViewCreateInfo {
                .image      = _depth.img.handle,
                .viewType   = vk::ImageViewType::e2D,
                .format     = _depth.img.format,
                .subresourceRange {
                    .aspectMask     = vk::ImageAspectFlagBits::eDepth,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
        },  }   ).value;
    }

    //_____________________________________
    inline void rebuild_swapchain() {
        VK_CHECK(graphics_queue.handle.waitIdle());

        for( auto image : swapchain.images ) {
            device.destroyImage(image);
        }
        for( auto img_view : swapchain.img_views ) {
            device.destroyImageView(img_view);
        }
        device.destroySwapchainKHR(swapchain.handle);

        build_swapchain();
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
            vk::CommandBufferAllocateInfo cmd_buffer_info {
                .commandPool        = frames.at(i).cmd.pool,
                .level              = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            VK_CHECK(device.allocateCommandBuffers(
                &cmd_buffer_info,
                &frames.at(i).cmd.buffer
            ));
        }
        VK_CHECK(device.createCommandPool(&cmd_pool_info, nullptr, &immediate.pool));
        vk::CommandBufferAllocateInfo cmd_buffer_info {
            .commandPool        = immediate.pool,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };
        VK_CHECK(device.allocateCommandBuffers(&cmd_buffer_info, &immediate.cmd));
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
        VK_CHECK(device.createFence(&fence_info, nullptr, &immediate.fence));
    }

    //_____________________________________
    inline void build_descriptors() {
        std::vector<vk::DescriptorPoolSize> sizes;
        sizes.push_back(vk::DescriptorPoolSize {
            .type = vk::DescriptorType::eStorageImage,
            .descriptorCount = (u32)(1 * 10),
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

        descriptor.set.handle = device.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo {
                .descriptorPool = descriptor.pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &descriptor.set.layout,
            }
        ).value.front();

        vk::DescriptorImageInfo img_info {
            .imageView = _draw.img.view,
            .imageLayout = vk::ImageLayout::eGeneral,
        };
        vk::WriteDescriptorSet draw_img_write {
            .dstSet = descriptor.set.handle,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageImage,
            .pImageInfo = &img_info,
        };
        device.updateDescriptorSets(1, &draw_img_write, 0, nullptr);
    }

    //_____________________________________
    inline void immediate_submit(fn<void(vk::CommandBuffer cmd)>&& fn) {
        VK_CHECK(device.resetFences(1, &immediate.fence));
        VK_CHECK(immediate.cmd.reset());
        VK_CHECK(immediate.cmd.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, }));
        {
            fn(immediate.cmd);
        }
        VK_CHECK(immediate.cmd.end());
        vk::CommandBufferSubmitInfo cmd_info {
            .commandBuffer = immediate.cmd,
            .deviceMask    = 0,
        };
        vk::SubmitInfo2 submit {
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmd_info,
        };
        VK_CHECK(graphics_queue.handle.submit2(1, &submit, immediate.fence));
        VK_CHECK(device.waitForFences(immediate.fence, true, UINT64_MAX));
    }

    //_____________________________________
    inline AllocBuffer create_buffer(size_t size, vk::BufferUsageFlags usage, vma::MemoryUsage mem_usage) {
        vk::BufferCreateInfo buffer_info {
            .size = size,
            .usage = usage,
        };
        vma::AllocationCreateInfo alloc_info {
            .flags = vma::AllocationCreateFlagBits::eMapped,
            .usage = mem_usage,
        };
        AllocBuffer buffer;
        VK_CHECK(alloc.createBuffer(&buffer_info, &alloc_info, &buffer.handle, &buffer.allocation, &buffer.info));
        return buffer;
    }

    inline void destroy_buffer(const AllocBuffer& buffer) {
        alloc.destroyBuffer(buffer.handle, buffer.allocation);
    }

    inline GPUMeshBuffer upload_mesh(std::span<u32> indices, std::span<Vertex> vertices) {
        const size_t vertex_buffer_size = vertices.size() * sizeof(Vertex);
        const size_t index_buffer_size = indices.size() * sizeof(u32);

        auto vertex_buffer = create_buffer(vertex_buffer_size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eGpuOnly);

        GPUMeshBuffer mesh_buffer {
            .index_buffer = create_buffer(index_buffer_size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vma::MemoryUsage::eGpuOnly),
            .vertex_buffer = vertex_buffer,
            .vertex_buffer_address = device.getBufferAddress(
                vk::BufferDeviceAddressInfo {
                    .buffer = vertex_buffer.handle,
                }
            ),
        };
        AllocBuffer staging_buffer = create_buffer(vertex_buffer_size + index_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuOnly);
        void* data = staging_buffer.info.pMappedData;
        memcpy(data, vertices.data(), vertex_buffer_size);
        memcpy((char*)data + vertex_buffer_size, indices.data(), index_buffer_size);
        immediate_submit([&](vk::CommandBuffer cmd) {
            vk::BufferCopy vertex_copy {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertex_buffer_size,
            };
            cmd.copyBuffer(staging_buffer.handle, mesh_buffer.vertex_buffer.handle, 1, &vertex_copy);
            vk::BufferCopy index_copy {
                .srcOffset = vertex_buffer_size,
                .dstOffset = 0,
                .size = index_buffer_size,
            };
            cmd.copyBuffer(staging_buffer.handle, mesh_buffer.index_buffer.handle, 1, &index_copy);
        });
        destroy_buffer(staging_buffer);
        return mesh_buffer;
    }

    //_____________________________________
    inline vk::ResultValue<vk::ShaderModule> load_shader_module(const char* path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if( !file.is_open() ) {
            throw std::runtime_error("\tError: Failed to open file at:" + std::string(path));
        }
        size_t size = file.tellg();
        std::vector<char> buffer(size);
        file.seekg(0);
        file.read((char*)buffer.data(), size);
        file.close();
        return device.createShaderModule(vk::ShaderModuleCreateInfo {
            .codeSize = buffer.size(),
            .pCode = (const u32*)buffer.data(),
        });
    }

    inline std::optional<std::vector<std::shared_ptr<Mesh>>> load_model(const char* path) {
        std::cout << "\n>> Loading gltf file at: " << path << "\n";

        std::vector<std::shared_ptr<Mesh>> new_meshes;
        Assimp::Importer importer;
        if( auto file = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs) ) {
            std::vector<u32> indices;
            std::vector<Vertex> vertices;
            for( uint32_t i = 0; i < file->mNumMeshes; i++ ) {
                Mesh mesh {
                    .name = "none",
                };
                indices.clear();
                vertices.clear();

                for( u32 j = 0; j < file->mNumMeshes; j++ ) {
                    aiMesh* submesh = file->mMeshes[j];
                    GeoSurface surface;

                    surface.start_index = (u32)indices.size();
                    for( u32 f = 0; f < submesh->mNumFaces; f++ ) {
                        aiFace& face = submesh->mFaces[f];
                        assert(face.mNumIndices == 3);
                        indices.push_back(face.mIndices[0]);
                        indices.push_back(face.mIndices[1]);
                        indices.push_back(face.mIndices[2]);
                    }
                    surface.count = (u32)indices.size();

                    std::cout << submesh->GetNumColorChannels() << "\n";
                    for( uint32_t j = 0; j <= submesh->mNumVertices; j++ ) {
                        vertices.push_back(Vertex {
                            .position = {
                                submesh->mVertices[j].x,
                                submesh->mVertices[j].y,
                                submesh->mVertices[j].z,
                            },
                            .uv_x = submesh->mTextureCoords[0][j].x,
                            .normal = {
                                submesh->mNormals[j].x,
                                submesh->mNormals[j].y,
                                submesh->mNormals[j].z,
                            },
                            .uv_y = submesh->mTextureCoords[0][j].y,
                            .color = {
                                1.f,//submesh->mColors[0][j]->r,
                                1.f,//submesh->mColors[0][j]->g,
                                1.f,//submesh->mColors[0][j]->b,
                                1.f,//submesh->mColors[0][j]->a,
                            },
                        });
                    }
                    mesh.surfaces.push_back(surface);
                }
                mesh.mesh_buffer = upload_mesh(indices, vertices);
                new_meshes.push_back(std::make_shared<Mesh>(mesh));

                ////_____________________________________
                //// Vertex buffer creation
                //{
                //    vk::DeviceSize buffer_size = sizeof(mesh.vertices.pos[0]) * mesh.vertices.pos.size();

                //    vk::Buffer staging_buffer;
                //    vk::DeviceMemory staging_buffer_memory;
                //    create_buffer(
                //        buffer_size,
                //        vk::BufferUsageFlagBits::eTransferSrc,
                //        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                //        staging_buffer,
                //        staging_buffer_memory
                //    );
                //    void* data = device.mapMemory(staging_buffer_memory, 0, buffer_size).value;
                //    {
                //        memcpy(data, mesh.vertices.pos.data(), (size_t) buffer_size);
                //    }
                //    device.unmapMemory(staging_buffer_memory);

                //    create_buffer(
                //        buffer_size,
                //        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                //        vk::MemoryPropertyFlagBits::eDeviceLocal,
                //        mesh.vertices.buffer,
                //        mesh.vertices.memory
                //    );
                //    copy_buffer(staging_buffer, mesh.vertices.buffer, buffer_size);
                //    device.destroyBuffer(staging_buffer);
                //    device.freeMemory(staging_buffer_memory);
                //}

                ////_____________________________________
                //// Index buffer creation
                //{
                //    vk::DeviceSize buffer_size = sizeof(mesh.indices.unique[0]) * mesh.indices.unique.size();

                //    vk::Buffer staging_buffer;
                //    vk::DeviceMemory staging_buffer_memory;
                //    create_buffer(
                //        buffer_size,
                //        vk::BufferUsageFlagBits::eTransferSrc,
                //        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                //        staging_buffer,
                //        staging_buffer_memory
                //    );
                //    void* data = device.mapMemory(staging_buffer_memory, 0, buffer_size).value;
                //    {
                //        memcpy(data, mesh.indices.unique.data(), (size_t) buffer_size);
                //    }
                //    device.unmapMemory(staging_buffer_memory);

                //    create_buffer(
                //        buffer_size,
                //        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                //        vk::MemoryPropertyFlagBits::eDeviceLocal,
                //        mesh.indices.buffer,
                //        mesh.indices.memory
                //    );
                //    copy_buffer(staging_buffer, mesh.indices.buffer, buffer_size);
                //    device.destroyBuffer(staging_buffer);
                //    device.freeMemory(staging_buffer_memory);
                //}
                //model.meshes.push_back(mesh);
            }
            //models.push_back(model);
        } else {
            std::cout << "Error:\n>>\tFailed to load model.\n";
            throw importer.GetException();
        }
        return new_meshes;
    }

    //_____________________________________
    inline void build_compute_pipeline() {

        vk::PushConstantRange push_const {
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset = 0,
            .size = sizeof(ComputePushConstants),
        };

        compute_pipeline.layout = device.createPipelineLayout(
                vk::PipelineLayoutCreateInfo {
                    .setLayoutCount = 1,
                    .pSetLayouts = &descriptor.set.layout,
                    .pushConstantRangeCount = 1,
                    .pPushConstantRanges = &push_const,
                }
        ).value;

        auto gradient_s = load_shader_module("res/shaders/gradient.comp.spv").value;
        auto sky_s = load_shader_module("res/shaders/sky.comp.spv").value;

        ComputeEffect gradient_effect {
            .name = "gradient",
            .pipeline_layout = compute_pipeline.layout,
            .data = {
                { glm::vec4(1, 0, 0, 1) },
                { glm::vec4(0, 0, 1, 1) },
            },
        };
        vk::PipelineShaderStageCreateInfo stage_info {
            .stage  = vk::ShaderStageFlagBits::eCompute,
            .module = gradient_s,
            .pName  = "main",
        };
        vk::ComputePipelineCreateInfo pipe_info {
            .stage = stage_info,
            .layout = compute_pipeline.layout,
        };

        //vk::PipelineCache cache = device.createPipelineCache(vk::PipelineCacheCreateInfo{}).value;
        compute_pipeline.handle  = device.createComputePipelines(nullptr, pipe_info).value.front();
        gradient_effect.pipeline = device.createComputePipelines(nullptr, pipe_info).value.front();
        
        stage_info.module = sky_s;

        ComputeEffect sky_effect {
            .name = "sky",
            .pipeline_layout = compute_pipeline.layout,
            .data = {
                { glm::vec4(0.1, 0.2, 0.4, 0.97) },
            },
        };

        sky_effect.pipeline = device.createComputePipelines(nullptr, pipe_info).value.front();

        bg_effects.push_back(sky_effect);
        bg_effects.push_back(gradient_effect);

        device.destroyShaderModule(sky_s);
        device.destroyShaderModule(gradient_s);
    }

    //_____________________________________
    inline void build_graphics_pipeline() {
        vk::PipelineViewportStateCreateInfo viewport_state {
            .viewportCount = 1,
            .scissorCount = 1,
        };
        vk::PipelineColorBlendAttachmentState attach_state {
            .blendEnable = false,
            .colorWriteMask = vk::ColorComponentFlagBits::eR 
                             | vk::ColorComponentFlagBits::eG 
                             | vk::ColorComponentFlagBits::eB 
                             | vk::ColorComponentFlagBits::eA,
        };
        vk::PipelineColorBlendStateCreateInfo blend_state {
            .logicOpEnable = false,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &attach_state,
        };
        vk::PipelineDepthStencilStateCreateInfo depth_state {
            .depthTestEnable = false,
            .depthWriteEnable = false,
            .depthCompareOp = vk::CompareOp::eNever,
            .depthBoundsTestEnable = false,
            .stencilTestEnable = false,
            .front = {},
            .back = {},
            .minDepthBounds = 0.f,
            .maxDepthBounds = 1.f,
        };
        vk::PipelineDynamicStateCreateInfo dyn_state {
            .dynamicStateCount = 2,
            .pDynamicStates = &graphics_pipeline.state[0],
        };
        vk::PipelineRasterizationStateCreateInfo rasterization_state {
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eClockwise,
            .lineWidth = 1.f,
        };
        vk::PipelineInputAssemblyStateCreateInfo assembly_state {
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = false,
        };
        vk::PipelineMultisampleStateCreateInfo multisample_state {
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = false,
            .minSampleShading = 1.f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = false,
            .alphaToOneEnable = false,
        };
        auto vertex_shader   = load_shader_module("res/shaders/mesh.vert.spv").value;
        auto fragment_shader = load_shader_module("res/shaders/solid.frag.spv").value;
        vk::PushConstantRange buffer_range {
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(GPUDrawPushConstants),
        };
        graphics_pipeline.layout = device.createPipelineLayout(
            vk::PipelineLayoutCreateInfo {
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &buffer_range,
            })
            .value;
        graphics_pipeline.shader_stages.push_back(
            vk::PipelineShaderStageCreateInfo {
            .stage  = vk::ShaderStageFlagBits::eVertex,
            .module = vertex_shader,
            .pName  = "main",
        });
        graphics_pipeline.shader_stages.push_back(
            vk::PipelineShaderStageCreateInfo {
            .stage  = vk::ShaderStageFlagBits::eFragment,
            .module = fragment_shader,
            .pName  = "main",
        });
        vk::Format color_attachment = vk::Format::eR16G16B16A16Sfloat;
        vk::PipelineRenderingCreateInfo render_info {
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &color_attachment,
            .depthAttachmentFormat = _depth.img.format,
        };
        vk::GraphicsPipelineCreateInfo graphics_pipeline_info {
            .pNext = &render_info,
            .stageCount = (u32)graphics_pipeline.shader_stages.size(),
            .pStages = graphics_pipeline.shader_stages.data(),
            .pInputAssemblyState = &assembly_state,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState = &multisample_state,
            .pDepthStencilState = &depth_state,
            .pColorBlendState = &blend_state,
            .pDynamicState = &dyn_state,
            .layout = graphics_pipeline.layout,
        };
        graphics_pipeline.handle = device
            .createGraphicsPipelines(nullptr, graphics_pipeline_info)
            .value
            .front();

        device.destroyShaderModule(vertex_shader);
        device.destroyShaderModule(fragment_shader);
    }

    inline void setup_imgui() {
        vk::DescriptorPoolSize pool_sizes[] {
            { vk::DescriptorType::eSampler, 1000 },
            { vk::DescriptorType::eSampledImage, 1000 },
            { vk::DescriptorType::eStorageImage, 1000 },
            { vk::DescriptorType::eUniformTexelBuffer, 1000 },
            { vk::DescriptorType::eStorageTexelBuffer, 1000 },
            { vk::DescriptorType::eUniformBuffer, 1000 },
            { vk::DescriptorType::eStorageBuffer, 1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment, 1000 },
        };
        vk::DescriptorPoolCreateInfo pool_info {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 1000,
            .poolSizeCount = (u32)std::size(pool_sizes),
            .pPoolSizes = pool_sizes,
        };
        vk::DescriptorPool imgui_pool;
        VK_CHECK(device.createDescriptorPool(&pool_info, nullptr, &imgui_pool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window::handle, true);
        ImGui_ImplVulkan_InitInfo init_info {
            .Instance           = static_cast<VkInstance>(instance),
            .PhysicalDevice     = static_cast<VkPhysicalDevice>(phys_device),
            .Device             = static_cast<VkDevice>(device),
            .Queue              = static_cast<VkQueue>(graphics_queue.handle),
            .DescriptorPool     = static_cast<VkDescriptorPool>(imgui_pool),
            .RenderPass         = VK_NULL_HANDLE,
            .MinImageCount      = 3,
            .ImageCount         = 3,
            .MSAASamples        = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1),
            .UseDynamicRendering = true,
            .PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo {
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &swapchain.img_format,
            },
        };
        ImGui_ImplVulkan_Init(&init_info);

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.FontDefault = io.Fonts->AddFontFromFileTTF("res/fonts/NotoSansMono-Regular.ttf", 12.f);
        io.Fonts->Build();

        //immediate_submit([&](vk::CommandBuffer cmd){ ImGui_ImplVulkan_CreateFontsTexture(); });

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 8.f;
        style.FrameRounding = 8.f;
        style.ScrollbarRounding = 4.f;

        style.Colors[ImGuiCol_Text] = ImVec4(.6f, .6f, .6f, 1.f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.f, 0.f, 0.f, .2f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.f, 0.f, 0.f, 0.f);
    }

    //_____________________________________
    inline void init() {
        input::bind(GLFW_KEY_W,
            key_callback {
                .press   = [&]{ camera.request_movement.forward = 1; },
                .release = [&]{ camera.request_movement.forward = 0; }
        }   );
        input::bind(GLFW_KEY_S,
            key_callback {
                .press   = [&]{ camera.request_movement.back = 1; },
                .release = [&]{ camera.request_movement.back = 0; }
        }   );
        input::bind(GLFW_KEY_A,
            key_callback {
                .press   = [&]{ camera.request_movement.left = 1; },
                .release = [&]{ camera.request_movement.left = 0; }
        }   );
        input::bind(GLFW_KEY_D,
            key_callback {
                .press   = [&]{ camera.request_movement.right = 1; },
                .release = [&]{ camera.request_movement.right = 0; }
        }   );
        input::bind(GLFW_KEY_Q,
            key_callback {
                .press   = [&]{ camera.request_movement.down = 1; },
                .release = [&]{ camera.request_movement.down = 0; }
        }   );
        input::bind(GLFW_KEY_E,
            key_callback {
                .press   = [&]{ camera.request_movement.up = 1; },
                .release = [&]{ camera.request_movement.up = 0; }
        }   );
        input::bind(GLFW_KEY_LEFT_SHIFT,
            key_callback {
                .press   = [&]{ camera.movement_speed = 2.f; },
                .release = [&]{ camera.movement_speed = 1.f; }
        }   );
        input::bind(GLFW_MOUSE_BUTTON_2,
            key_callback {
                .press   = [&]{
                    glfwSetInputMode(window::handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    camera.follow_mouse = true;
                },
                .release = [&]{
                    glfwSetInputMode(window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    camera.follow_mouse = false;
                }
        }   );
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
        vk::DeviceQueueCreateInfo queue_info {
            .queueFamilyIndex   = graphics_queue.family,
            .queueCount         = 1,
            .pQueuePriorities   = &prio,
        };

        vk::PhysicalDeviceDynamicRenderingFeatures dyn_features {
            .dynamicRendering = true,
        };
        vk::PhysicalDeviceSynchronization2Features sync_features {
            .pNext = &dyn_features,
            .synchronization2 = true,
        };
        vk::PhysicalDeviceDescriptorIndexingFeatures desc_index_features {
            .pNext = &sync_features,
        };
        vk::PhysicalDeviceBufferDeviceAddressFeatures buffer_device_features {
            .pNext = &desc_index_features,
        };
        vk::PhysicalDeviceFeatures2 device_features_2 {
            .pNext = &buffer_device_features,
        };
        phys_device.getFeatures2(&device_features_2);
        device = phys_device.createDevice(
            vk::DeviceCreateInfo{
                .pNext                   = &device_features_2,
                .queueCreateInfoCount    = 1,
                .pQueueCreateInfos       = &queue_info,
                .enabledLayerCount       = (u32)layers.size(),
                .ppEnabledLayerNames     = layers.data(),
                .enabledExtensionCount   = (u32)device_extensions.size(),
                .ppEnabledExtensionNames =  device_extensions.data(),
                //.pEnabledFeatures        = &device_features,
            },
            nullptr
        ).value;
        //VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

        graphics_queue.family = get_queue_family(vk::QueueFlagBits::eGraphics);
        graphics_queue.handle = device.getQueue(graphics_queue.family, 0);

        vma::AllocatorCreateInfo alloc_info {
            .flags = vma::AllocatorCreateFlagBits::eBufferDeviceAddress,
            .physicalDevice = phys_device,
            .device = device,
            .instance = instance,
        };
        alloc = vma::createAllocator(alloc_info).value;

        build_swapchain();

        build_command_structures();
        build_sync_structures();
        build_descriptors();
        build_compute_pipeline();
        build_graphics_pipeline();
        setup_imgui();

        //Vertex rect_vertices[4] {
        //    Vertex {
        //        .position = { .5, -.5, 0 },
        //        .color    = { 0, 1, 1, 1},
        //    },
        //    Vertex {
        //        .position = { .5, .5, 0 },
        //        .color    = { 0, 1, 1, 1 },
        //    },
        //    Vertex {
        //        .position = { -.5, -.5, 0 },
        //        .color    = { 0, 1, 1, 1 },
        //    },
        //    Vertex {
        //        .position = { -.5, .5, 0 },
        //        .color    = { 0, 1, 1, 1 },
        //    },
        //};
        //u32 rect_indices[6] {
        //    0, 1, 2,
        //    2, 1, 3,
        //};
        //auto rect = upload_mesh(rect_indices, rect_vertices);
        auto loaded_meshes = load_model("res/models/SciFiHelmet.gltf");
        if( !loaded_meshes.has_value() ) {
            std::cout << "\nError:\n>>\tFailed to load meshes.\n";
        }
        for( auto& mesh : loaded_meshes.value()) {
            meshes.push_back(*mesh);
        }
    }

    //_____________________________________
    inline void transition_img(vk::CommandBuffer cmd_buffer, vk::Image img, vk::ImageLayout from, vk::ImageLayout to) {
        vk::ImageMemoryBarrier2 img_barrier {
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
    inline void draw_geometry(vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo attach_info {
            .imageView = _draw.img.view,
            .imageLayout = vk::ImageLayout::eGeneral,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };
        vk::RenderingInfo render_info {
            .renderArea = {
                .offset = vk::Offset2D {
                    .x = 0,
                    .y = 0,
                },
                .extent = _draw.extent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attach_info,
            .pDepthAttachment = nullptr,
            .pStencilAttachment = nullptr,
        };
        cmd.beginRendering(&render_info);

        vk::Viewport viewport {
            .x = 0,
            .y = 0,
            .width = (float)_draw.extent.width,
            .height = (float)_draw.extent.height,
            .minDepth = 0.f,
            .maxDepth = 1.f,
        };
        cmd.setViewport(0, 1, &viewport);

        vk::Rect2D scissor {
            .offset {
                .x = 0,
                .y = 0,
            },
            .extent {
                .width = _draw.extent.width,
                .height = _draw.extent.height,
            },
        };
        cmd.setScissor(0, 1, &scissor);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline.handle);

        GPUDrawPushConstants push_constants {
            .world_matrix = camera.get_view(),//glm::mat4 { 1 },
            .vertex_buffer = meshes[0].mesh_buffer.vertex_buffer_address,
        };
        cmd.pushConstants(graphics_pipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(GPUDrawPushConstants), &push_constants);
        cmd.bindIndexBuffer(meshes[0].mesh_buffer.index_buffer.handle, 0, vk::IndexType::eUint32);
        cmd.drawIndexed(meshes[0].surfaces[0].count, 1, meshes[0].surfaces[0].start_index, 0, 0);
        cmd.endRendering();
    };
    
    //_____________________________________
    inline void tick() {
        auto frame = frames.at(current_frame % FRAME_OVERLAP);

        VK_CHECK(device.waitForFences(1, &frame.fence, true, UINT64_MAX));

        auto img = device.acquireNextImageKHR(swapchain.handle, UINT64_MAX, frame.swap_semaphore);
        //if( img.result == vk::Result::eErrorOutOfDateKHR ) {
        //    rebuild_swapchain();
        //}
        camera.update(time::delta_time);
        u32 img_index = img.value;
        vk::Image swap_img = swapchain.images.at(img_index);

        VK_CHECK(device.resetFences(1, &frame.fence));
        frame.cmd.buffer.reset();

        _draw.extent.width  = _draw.img.extent.width;
        _draw.extent.height = _draw.img.extent.height;
        VK_CHECK(frame.cmd.buffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, }));
        {
            transition_img(frame.cmd.buffer, _draw.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

            frame.cmd.buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline.handle);
            frame.cmd.buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline.layout, 0, descriptor.set.handle, nullptr);
            ComputePushConstants push_const {
                .data1 = glm::vec4(1, 0, 0, 1),
                .data2 = glm::vec4(0, 0, 1, 1),
            };
            frame.cmd.buffer.pushConstants(compute_pipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstants), &push_const);
            frame.cmd.buffer.dispatch(std::ceil(_draw.extent.width / 16.f), std::ceil(_draw.extent.height / 16.f), 1);

            ComputeEffect& effect = bg_effects[current_bg_effect];

            frame.cmd.buffer.bindPipeline(vk::PipelineBindPoint::eCompute, effect.pipeline);
            frame.cmd.buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline.layout, 0, descriptor.set.handle, nullptr);
            frame.cmd.buffer.pushConstants(compute_pipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstants), &effect.data);
            frame.cmd.buffer.dispatch(std::ceil(_draw.extent.width / 16.f), std::ceil(_draw.extent.height / 16.f), 1);
            draw_geometry(frame.cmd.buffer);

            transition_img(frame.cmd.buffer, _draw.img.handle, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
            transition_img(frame.cmd.buffer, swap_img, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

            vk::ImageBlit2 blit_region {
                .srcSubresource = vk::ImageSubresourceLayers {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .srcOffsets = {{
                    vk::Offset3D { 0, 0, 0 },
                    vk::Offset3D { (int) _draw.extent.width, (int) _draw.extent.height, 1 },
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
            frame.cmd.buffer.blitImage2(
                vk::BlitImageInfo2 {
                    .srcImage = _draw.img.handle,
                    .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
                    .dstImage = swap_img,
                    .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                    .regionCount = 1,
                    .pRegions = &blit_region,
                    .filter = vk::Filter::eLinear,
            }   );

            transition_img(frame.cmd.buffer, swap_img, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);

            ui::draw(frame.cmd.buffer, swapchain.img_views[img_index]);

            transition_img(frame.cmd.buffer, swap_img, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
        }
        VK_CHECK(frame.cmd.buffer.end());

        vk::CommandBufferSubmitInfo cmd_info {
            .commandBuffer = frame.cmd.buffer,
            .deviceMask    = 0,
        };
        vk::SemaphoreSubmitInfo wait_info {
            .semaphore   = frame.swap_semaphore,
            .value       = 1,
            .stageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .deviceIndex = 0,
        };
        vk::SemaphoreSubmitInfo signal_info {
            .semaphore   = frame.render_semaphore,
            .value       = 1,
            .stageMask   = vk::PipelineStageFlagBits2::eAllGraphics,
            .deviceIndex = 0,
        };
        vk::SubmitInfo2 submit {
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &wait_info,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmd_info,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signal_info,
        };
        VK_CHECK(graphics_queue.handle.submit2(submit, frame.fence));

        vk::PresentInfoKHR present_info {
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

        for( auto& mesh : meshes ) {
            alloc.destroyBuffer(mesh.mesh_buffer.index_buffer.handle, mesh.mesh_buffer.index_buffer.allocation);
            alloc.destroyBuffer(mesh.mesh_buffer.vertex_buffer.handle, mesh.mesh_buffer.vertex_buffer.allocation);
        }
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        device.destroyPipelineLayout(compute_pipeline.layout);
        device.destroyPipeline(compute_pipeline.handle);

        device.destroyPipelineLayout(graphics_pipeline.layout);
        device.destroyPipeline(graphics_pipeline.handle);

        device.destroySwapchainKHR(swapchain.handle);
        for( auto view : swapchain.img_views ) {
            device.destroyImageView(view);
        }
        device.destroyImageView(_draw.img.view);
        alloc.destroyImage(_draw.img.handle, _draw.img.alloc);
        alloc.destroyImage(_depth.img.handle, _depth.img.alloc);
        device.destroyDescriptorPool(descriptor.pool);
        device.destroyDescriptorSetLayout(descriptor.set.layout);
        for( auto frame : frames ) {
            device.destroyCommandPool(frame.cmd.pool);
            device.destroyFence(frame.fence);
            device.destroySemaphore(frame.swap_semaphore);
            device.destroySemaphore(frame.render_semaphore);
        }
        device.destroyCommandPool(immediate.pool);
        device.destroyFence(immediate.fence);
        alloc.destroy();
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

