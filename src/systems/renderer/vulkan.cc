#define VMA_IMPLEMENTATION 1003000

#include <fstream>
#include <chrono>

#include "vulkan.hh"
#include "glfw.hh"
#include "pipelines.hh"
#include "swapchain.hh"
#include "descriptors.hh"
#include "image.hh"
#include "frame.hh"
#include "camera.hh"
#include "primitives.hh"

#include "materials/wave.hh"
#include "materials/pbr.hh"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "assimp/Importer.hpp"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace sigil::renderer {

    //_____________________________________
    void init() {
        setup_keybinds();

        u32 glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        VULKAN_HPP_DEFAULT_DISPATCHER.init();

        instance = vk::createInstance(
            vk::InstanceCreateInfo {
                .pApplicationInfo        = &engine_info,
                .enabledLayerCount       = (u32)_layers.size(),
                .ppEnabledLayerNames     = _layers.data(),
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
            vk::DeviceCreateInfo {
                .pNext                   = &device_features_2,
                .queueCreateInfoCount    = 1,
                .pQueueCreateInfos       = &queue_info,
                .enabledLayerCount       = (u32)_layers.size(),
                .ppEnabledLayerNames     = _layers.data(),
                .enabledExtensionCount   = (u32)_device_exts.size(),
                .ppEnabledExtensionNames = _device_exts.data(),
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
        vma_allocator = vma::createAllocator(alloc_info).value;

        build_swapchain();

        build_command_structures();
        build_sync_structures();
        build_descriptors();
        build_compute_pipeline();
        _pbr_metallic_roughness.build_pipelines();
        _wave_material.build_pipelines();
        setup_imgui();

        u32 white = 0xFFFFFFFF;
        _white_img = create_img(vma_allocator, (void*)&white, vk::Extent3D { 16, 16, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);

        u32 black = 0x00000000;
        u32 magenta = 0x00FF00FF;
        std::array<u32, 16 * 16> pixels;
        for( int x = 0; x < 16; x++ ) {
            for( int y = 0; y < 16; y++ ) {
                pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
            }
        }
        _error_img = create_img(vma_allocator, pixels.data(), vk::Extent3D { 16, 16, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);
        _albedo_img = load_img(vma_allocator, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, "res/textures/Default_albedo.jpg");
        _metal_roughness_img = load_img(vma_allocator, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, "res/textures/Default_metalRoughness.jpg");

        vk::SamplerCreateInfo sampler {
            .magFilter = vk::Filter::eNearest,
            .minFilter = vk::Filter::eNearest,
        };
        VK_CHECK(device.createSampler(&sampler, nullptr, &_sampler_nearest));
        sampler = {
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
        };
        VK_CHECK(device.createSampler(&sampler, nullptr, &_sampler_linear));

        AllocatedBuffer pbr_material_constants = create_buffer(sizeof(PbrMetallicRoughness::MaterialConstants), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);
        PbrMetallicRoughness::MaterialConstants* pbr_scene_uniform_data = (PbrMetallicRoughness::MaterialConstants*)pbr_material_constants.info.pMappedData;
        pbr_scene_uniform_data->color_factors = glm::vec4{ 1, 1, 1, 1 };
        pbr_scene_uniform_data->metal_roughness_factors = glm::vec4 { 1, .5, 0, 0 };

        PbrMetallicRoughness::MaterialResources pbr_material_resources {
            .color_img               = _albedo_img,
            .color_sampler           = _sampler_linear,
            .metal_roughness_img     = _metal_roughness_img,
            .metal_roughness_sampler = _sampler_linear,
            .data                    = pbr_material_constants.handle,
            .offset                  = 0,
        };

        _deletion_queue.push_function([=]{
            destroy_buffer(pbr_material_constants);
        });

        _default_material_instance = _pbr_metallic_roughness.write_material(pbr_material_resources, _descriptor_allocator);

        AllocatedBuffer wave_material_constants = create_buffer(sizeof(WaveMaterial::MaterialConstants), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);
        WaveMaterial::MaterialConstants* wave_scene_uniform_data = (WaveMaterial::MaterialConstants*)wave_material_constants.info.pMappedData;
        wave_scene_uniform_data->color_factors = glm::vec4{ 1, 1, 1, 1 };

        WaveMaterial::MaterialResources wave_material_resources {
            .color_img = _error_img,
            .color_sampler = _sampler_nearest,
            .data = wave_material_constants.handle,
            .offset = 0,
        };

        _deletion_queue.push_function([=]{
            destroy_buffer(wave_material_constants);
        });

        _wave_material_instance = _wave_material.write_material(wave_material_resources, _descriptor_allocator);

        auto loaded_meshes = load_model("res/models/DamagedHelmet.gltf");
        if( loaded_meshes.empty() ) {
            std::cout << "\nError:\n>>\tFailed to load meshes.\n";
        }
        for( auto& mesh : loaded_meshes) {
            _meshes.push_back(*mesh);

            for( auto& surface : mesh->surfaces ) {
                _opaque_objects.push_back(
                    RenderObject {
                        .count        = surface.count,
                        .first        = surface.start_index,
                        .index_buffer = mesh->mesh_buffer.index_buffer.handle,
                        .material     = &_default_material_instance,//&surface.material->data,
                        .bounds       = surface.bounds,
                        .transform    = glm::mat4 { 1 },
                        .address      = mesh->mesh_buffer.vertex_buffer_address,
                    }
                );
            }
        }
        {   // Plane mesh
            auto plane = primitives::Plane(100, 10);
            auto plane_buffer = upload_mesh(plane.indices, plane.vertices);

            _opaque_objects.push_back(
                RenderObject {
                    .count = plane.surface.count,
                    .first = plane.surface.start_index,
                    .index_buffer = plane_buffer.index_buffer.handle,
                    .material = &_wave_material_instance,
                    .transform = glm::mat4 { 1 },//glm::translate(glm::mat4 { 1 }, glm::vec3(0, 0, -1)),
                    .address = plane_buffer.vertex_buffer_address,
                }
            );
        }
        std::sort(_opaque_objects.begin(), _opaque_objects.end(), [&](const auto& a, const auto& b) {
            if( a.material == b.material ) { return a.index_buffer < b.index_buffer; }
            else { return a.material < b.material; }
        });
    }

    //_____________________________________
    vk::ResultValue<vk::ShaderModule> load_shader_module(const char* path) {
        //std::cout << ">> Loading file at: " << path << "\n";

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

    //_____________________________________
    std::vector<std::shared_ptr<Mesh>> load_model(const char* path) {
        //std::cout << ">> Loading file at: " << path << "\n";

        std::vector<std::shared_ptr<Mesh>> new_meshes;
        Assimp::Importer importer;
        if( auto file = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs ) ) {
            std::vector<u32> indices;
            std::vector<Vertex> vertices;

            if( file->HasMeshes() ) {
                for( uint32_t i = 0; i < file->mNumMeshes; i++ ) {
                    Mesh mesh {
                        .id = _meshes.size(),
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

                        //std::cout << submesh->GetNumColorChannels() << "\n";
                        glm::vec3 minpos, maxpos {};
                        for( uint32_t j = 0; j <= submesh->mNumVertices; j++ ) {
                            Vertex vtx {
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
                            };
                            minpos = glm::min(minpos, vtx.position);
                            maxpos = glm::max(maxpos, vtx.position);
                            vertices.push_back(vtx);
                        }
                        surface.bounds = {
                            .origin        = (maxpos + minpos) / 2.f,
                            .extents       = (maxpos - minpos) / 2.f,
                            .sphere_radius = glm::length((maxpos - minpos) / 2.f),
                        };
                        mesh.surfaces.push_back(surface);
                    }
                    mesh.mesh_buffer = upload_mesh(indices, vertices);
                    new_meshes.push_back(std::make_shared<Mesh>(mesh));
                }
            }
            if( file->HasMaterials() ) {
            }
            else {
                std::cout << "Error:\n>>\tNo models found in file.\n";
            }
        } else {
            std::cout << "Error:\n>>\tFailed to load model.\n";
            throw importer.GetException();
        }
        return new_meshes;
    }

    //_____________________________________
    GPUMeshBuffer upload_mesh(std::span<u32> indices, std::span<Vertex> vertices) {
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
        AllocatedBuffer staging_buffer = create_buffer(vertex_buffer_size + index_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuOnly);
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
    u32 get_queue_family(vk::QueueFlagBits type) {
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
    void build_command_structures() {
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
    void build_sync_structures() {
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
    void build_descriptors() {
        std::vector<std::pair<vk::DescriptorType, f32>> sizes {
            { vk::DescriptorType::eStorageImage,  1 },
            { vk::DescriptorType::eUniformBuffer, 1 },
        };
        _descriptor_allocator.init(10, sizes);

        {
            _draw.descriptor.set.layout = DescriptorLayoutBuilder {}
                .add_binding(0, vk::DescriptorType::eStorageImage)
                .build(vk::ShaderStageFlagBits::eCompute);
        }
        {
            _single_img_layout = DescriptorLayoutBuilder {}
                .add_binding(0, vk::DescriptorType::eCombinedImageSampler)
                .build(vk::ShaderStageFlagBits::eFragment);
        }
        {
            _scene_data_layout = DescriptorLayoutBuilder {}
                .add_binding(0, vk::DescriptorType::eUniformBuffer)
                .build(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
        }

        _draw.descriptor.set.handle = _descriptor_allocator.allocate(_draw.descriptor.set.layout);

        _descriptor_writer
            .write_img(0, _draw.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
            .update_set(_draw.descriptor.set.handle);

        for( u32 i = 0; i < FRAME_OVERLAP; i++ ) {
            std::vector<std::pair<vk::DescriptorType, f32>> frame_sizes = {
                std::pair( vk::DescriptorType::eStorageImage,         3 ),
                std::pair( vk::DescriptorType::eStorageBuffer,        3 ),
                std::pair( vk::DescriptorType::eUniformBuffer,        3 ),
                std::pair( vk::DescriptorType::eCombinedImageSampler, 4 ),
            };
            frames[i].descriptors.init(1000, frame_sizes);
        }
    }

    //_____________________________________
    void immediate_submit(fn<void(vk::CommandBuffer cmd)>&& fn) {
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
    AllocatedBuffer create_buffer(size_t size, vk::BufferUsageFlags usage, vma::MemoryUsage mem_usage) {
        vk::BufferCreateInfo buffer_info {
            .size = size,
            .usage = usage,
        };
        vma::AllocationCreateInfo alloc_info {
            .flags = vma::AllocationCreateFlagBits::eMapped,
            .usage = mem_usage,
        };
        AllocatedBuffer buffer;
        VK_CHECK(vma_allocator.createBuffer(&buffer_info, &alloc_info, &buffer.handle, &buffer.allocation, &buffer.info));
        return buffer;
    }

    //_____________________________________
    void destroy_buffer(const AllocatedBuffer& buffer) {
        vma_allocator.destroyBuffer(buffer.handle, buffer.allocation);
    }

    //_____________________________________
    bool is_visible(const RenderObject& obj, const glm::mat4& viewproj) {
        constexpr std::array<glm::vec3, 8> corners {
            glm::vec3 {  1,  1,  1 },
            glm::vec3 {  1,  1, -1 },
            glm::vec3 {  1, -1,  1 },
            glm::vec3 {  1, -1, -1 },
            glm::vec3 { -1,  1,  1 },
            glm::vec3 { -1,  1, -1 },
            glm::vec3 { -1, -1,  1 },
            glm::vec3 { -1, -1, -1 },
        };
        glm::mat4 matrix = viewproj * obj.transform;
        glm::vec3 min = {  1.5f,  1.5f,  1.5f };
        glm::vec3 max = { -1.5f, -1.5f, -1.5f };
        for( auto& corner : corners ) {
            glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corner * obj.bounds.extents), 1.f);
            v.x = v.x / v.w;
            v.y = v.y / v.w;
            v.z = v.z / v.w;
            glm::min(glm::vec3 { v.x, v.y, v.z }, min);
            glm::max(glm::vec3 { v.x, v.y, v.z }, max);
        }
        if( min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || min.y < -1.f ) { return false; }
        else { return true; }
    }

    //_____________________________________
    void tick() {
        auto start = std::chrono::system_clock::now();

        if( swapchain.request_resize ) { rebuild_swapchain(); }
        FrameData& frame = frames.at(current_frame % FRAME_OVERLAP);

        VK_CHECK(device.waitForFences(1, &frame.fence, true, UINT64_MAX));
        frame.deletion_queue.flush();
        frame.descriptors.clear();

        auto img = device.acquireNextImageKHR(swapchain.handle, UINT64_MAX, frame.swap_semaphore);
        if( img.result == vk::Result::eErrorOutOfDateKHR || img.result == vk::Result::eSuboptimalKHR ) {
            swapchain.request_resize = true;
        }
        camera.update(time::delta_time);
        u32 img_index = img.value;
        vk::Image& swap_img = swapchain.images.at(img_index);

        VK_CHECK(device.resetFences(1, &frame.fence));
        frame.cmd.buffer.reset();

        _draw.extent.width  = _draw.img.extent.width;
        _draw.extent.height = _draw.img.extent.height;
        VK_CHECK(frame.cmd.buffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, }));
        {
            transition_img(frame.cmd.buffer, _draw.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

            {
                ComputeEffect& effect = bg_effects[current_bg_effect];

                frame.cmd.buffer.bindPipeline(vk::PipelineBindPoint::eCompute, effect.pipeline);
                frame.cmd.buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline.layout, 0, _draw.descriptor.set.handle, nullptr);
                frame.cmd.buffer.pushConstants(compute_pipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstants), &effect.data);
                frame.cmd.buffer.dispatch(std::ceil(_draw.extent.width / 16.f), std::ceil(_draw.extent.height / 16.f), 1);
            }

            transition_img(frame.cmd.buffer, _draw.img.handle, vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal);
            transition_img(frame.cmd.buffer, _depth.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

            update_scene();
            draw_geometry(frame, frame.cmd.buffer);

            transition_img(frame.cmd.buffer, _draw.img.handle, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
            transition_img(frame.cmd.buffer, swap_img, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

            copy_img_to_img(frame.cmd.buffer, _draw.img.handle, swap_img, _draw.extent, swapchain.extent);

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
            .waitSemaphoreInfoCount   = 1,
            .pWaitSemaphoreInfos      = &wait_info,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &cmd_info,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signal_info,
        };

        VK_CHECK(graphics_queue.handle.submit2(submit, frame.fence));

        vk::PresentInfoKHR present_info {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &frame.render_semaphore,
            .swapchainCount     = 1,
            .pSwapchains        = &swapchain.handle,
            .pImageIndices      = &img_index,
        };
        vk::Result present_result = graphics_queue.handle.presentKHR(&present_info);
        if( present_result == vk::Result::eErrorOutOfDateKHR || present_result == vk::Result::eSuboptimalKHR ) {
            swapchain.request_resize = true;
        }

        current_frame++;
        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        _stats.frametime = elapsed.count() / 1000.f;
    }

    //_____________________________________
    void terminate() {
        VK_CHECK(device.waitIdle());

        for( auto& frame : frames ) {
            frame.deletion_queue.flush();
            device.destroyCommandPool(frame.cmd.pool);
            device.destroyFence(frame.fence);
            device.destroySemaphore(frame.swap_semaphore);
            device.destroySemaphore(frame.render_semaphore);
        }
        _deletion_queue.flush();
        for( auto& mesh : _meshes ) {
            destroy_buffer(mesh.mesh_buffer.index_buffer);
            destroy_buffer(mesh.mesh_buffer.vertex_buffer);
        }
        device.destroySampler(_sampler_linear);
        device.destroySampler(_sampler_nearest);
        device.destroyPipelineLayout(compute_pipeline.layout);
        device.destroyPipeline(compute_pipeline.handle);
        device.destroySwapchainKHR(swapchain.handle);
        for( auto view : swapchain.img_views ) {
            device.destroyImageView(view);
        }
        device.destroyDescriptorPool(_draw.descriptor.pool);
        device.destroyDescriptorSetLayout(_draw.descriptor.set.layout);
        device.destroyDescriptorPool(_depth.descriptor.pool);
        device.destroyDescriptorSetLayout(_depth.descriptor.set.layout);
        device.destroyDescriptorSetLayout(_scene_data_layout);
        device.destroyCommandPool(immediate.pool);
        device.destroyFence(immediate.fence);
        _descriptor_allocator.destroy();
        vma_allocator.destroy();
        instance.destroySurfaceKHR(surface);
        device.destroy();
        instance.destroyDebugUtilsMessengerEXT(debug.messenger);
        instance.destroy();
    }

    //_____________________________________
    void setup_keybinds() {
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

        input::bind(GLFW_KEY_F,
            key_callback {
                .press   = [&]{ ui::show_info ^= 1; },
        }   );
    };

    //_____________________________________
    void build_compute_pipeline() {
        vk::PushConstantRange push_const {
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset = 0,
            .size = sizeof(ComputePushConstants),
        };

        compute_pipeline.layout = device.createPipelineLayout(
                vk::PipelineLayoutCreateInfo {
                    .setLayoutCount = 1,
                    .pSetLayouts = &_draw.descriptor.set.layout,
                    .pushConstantRangeCount = 1,
                    .pPushConstantRanges = &push_const,
                }
        ).value;

        vk::ShaderModule sky = load_shader_module("res/shaders/gradient.comp.spv").value;

        vk::PipelineShaderStageCreateInfo stage_info {
            .stage  = vk::ShaderStageFlagBits::eCompute,
            .module = sky,
            .pName  = "main",
        };
        vk::ComputePipelineCreateInfo pipe_info {
            .stage = stage_info,
            .layout = compute_pipeline.layout,
        };
        ComputeEffect sky_effect {
            .name = "sky",
            .pipeline_layout = compute_pipeline.layout,
            .data = {
                { glm::vec4(.2, .4, .8, 1) },
                { glm::vec4(.4, .4, .4, 1) },
            },
        };

        //vk::PipelineCache cache = device.createPipelineCache(vk::PipelineCacheCreateInfo{}).value;
        compute_pipeline.handle  = device.createComputePipelines(nullptr, pipe_info).value.front();

        sky_effect.pipeline = device.createComputePipelines(nullptr, pipe_info).value.front();
        bg_effects.push_back(sky_effect);
        device.destroyShaderModule(sky);
    }

    //_____________________________________
    void setup_imgui() {
        vk::DescriptorPoolSize pool_sizes[] {
            { vk::DescriptorType::eSampler,              1000 },
            { vk::DescriptorType::eSampledImage,         1000 },
            { vk::DescriptorType::eStorageImage,         1000 },
            { vk::DescriptorType::eUniformTexelBuffer,   1000 },
            { vk::DescriptorType::eStorageTexelBuffer,   1000 },
            { vk::DescriptorType::eUniformBuffer,        1000 },
            { vk::DescriptorType::eStorageBuffer,        1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment,      1000 },
        };
        vk::DescriptorPoolCreateInfo pool_info {
            .flags          = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets        = 1000,
            .poolSizeCount  = (u32)std::size(pool_sizes),
            .pPoolSizes     = pool_sizes,
        };
        vk::DescriptorPool imgui_pool;
        VK_CHECK(device.createDescriptorPool(&pool_info, nullptr, &imgui_pool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window::handle, true);
        ImGui_ImplVulkan_InitInfo init_info {
            .Instance            = static_cast<VkInstance>(instance),
            .PhysicalDevice      = static_cast<VkPhysicalDevice>(phys_device),
            .Device              = static_cast<VkDevice>(device),
            .Queue               = static_cast<VkQueue>(graphics_queue.handle),
            .DescriptorPool      = static_cast<VkDescriptorPool>(imgui_pool),
            .RenderPass          = VK_NULL_HANDLE,
            .MinImageCount       = 3,
            .ImageCount          = 3,
            .MSAASamples         = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1),
            .UseDynamicRendering = true,
            .PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo {
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &swapchain.img_format,
            },
        };
        ImGui_ImplVulkan_Init(&init_info);

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.FontDefault = io.Fonts->AddFontFromFileTTF("res/fonts/NotoSansMono-Regular.ttf", 13.f);
        io.Fonts->Build();

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding    = 8.f;
        style.FrameRounding     = 8.f;
        style.ScrollbarRounding = 4.f;

        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.f, 0.f, 0.f, .4f);
        style.Colors[ImGuiCol_Text]      = ImVec4(.6f, .6f, .6f, 1.f);
        style.Colors[ImGuiCol_WindowBg]  = ImVec4(0.f, 0.f, 0.f, .33f);
        style.Colors[ImGuiCol_Border]    = ImVec4(0.f, 0.f, 0.f, 0.f);

        _deletion_queue.push_function([&]{
                // TOOD: Investigate if this is destroyed on the ImGui side now
            //device.destroyDescriptorPool(imgui_pool); 
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        });
    }

    //_____________________________________
    void update_scene() {
        _scene_data = {
            .view = camera.get_view(),
            .proj = glm::perspective(
                glm::radians(70.f),
                (f32)swapchain.extent.width/(f32)swapchain.extent.height,
                camera.clip_plane.near,
                camera.clip_plane.far
            ),
            .ambient_color = glm::vec4 { .1f },
            .sun_dir = glm::vec4 { 0, -1, 1, 1 },
            .sun_color = glm::vec4 { 1.f },
        };
        _scene_data.proj[1][1] *= -1;
    }

    //_____________________________________
    void draw_geometry(FrameData& frame, vk::CommandBuffer cmd) {
        auto start = std::chrono::system_clock::now();

        vk::RenderingAttachmentInfo attach_info {
            .imageView = _draw.img.view,
            .imageLayout = vk::ImageLayout::eGeneral,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };

        vk::RenderingAttachmentInfo depth_info {
            .imageView = _depth.img.view,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };

        vk::RenderingInfo render_info {
            .renderArea = {
                .offset  = vk::Offset2D {
                    .x    = 0,
                    .y    = 0,
                },
                .extent  = _draw.extent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attach_info,
            .pDepthAttachment = &depth_info,
            .pStencilAttachment = nullptr,
        };

        cmd.beginRendering(&render_info);

        vk::Viewport viewport {
            .x = 0,
            .y = 0,
            .width = (float)_draw.extent.width,
            .height = (float)_draw.extent.height,
            .minDepth = 1.f,
            .maxDepth = 0.f,
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

        glm::mat4 projection = glm::perspective(
            glm::radians(70.f),
            (f32)swapchain.extent.width/(f32)swapchain.extent.height,
            camera.clip_plane.near,
            camera.clip_plane.far
        );

        AllocatedBuffer gpu_scene_data_buffer = create_buffer(sizeof(GPUSceneData), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);

        frame.deletion_queue.push_function([=] {
            destroy_buffer(gpu_scene_data_buffer);
        });

        GPUSceneData* scene_uniform_data = (GPUSceneData*)gpu_scene_data_buffer.info.pMappedData;
        *scene_uniform_data = _scene_data;

        vk::DescriptorSet descriptor_set = frame.descriptors.allocate(_scene_data_layout);

        DescriptorWriter {}
            .write_buffer(0, gpu_scene_data_buffer.handle, sizeof(GPUSceneData), 0, vk::DescriptorType::eUniformBuffer)
            .update_set(descriptor_set);

        glm::mat4 view = camera.get_view();
        _stats.drawcalls = 0;
        _stats.triangles = 0;
        MaterialPipeline* last_pipeline = nullptr;
        MaterialInstance* last_material = nullptr;
        vk::Buffer last_index_buffer = VK_NULL_HANDLE;

        for( auto& render : _opaque_objects ) { // TOOD: Add transparent draws
            //if( !is_visible(render, render.transform * projection * view) ) { continue; }
            if( render.material != last_material ) {
                last_material = render.material;
                if( render.material->pipeline != last_pipeline ) {
                    last_pipeline = render.material->pipeline;
                    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, render.material->pipeline->handle);
                    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, render.material->pipeline->layout, 0, 1, &descriptor_set, 0, nullptr);
                }
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, render.material->pipeline->layout, 1, 1, &render.material->material_set, 0, nullptr);
            }

            if( render.index_buffer != last_index_buffer ) {
                last_index_buffer = render.index_buffer;
                cmd.bindIndexBuffer(render.index_buffer, 0, vk::IndexType::eUint32);
            }
            GPUDrawPushConstants push_constants {
                .world_matrix = render.transform * projection * view,
                .vertex_buffer = render.address,
                .time = (f32)glfwGetTime(),
            };
            cmd.pushConstants(render.material->pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(GPUDrawPushConstants), &push_constants);

            cmd.drawIndexed(render.count, 1, render.first, 0, 0);
            _stats.drawcalls++;
            _stats.triangles += render.count / 3;
        }
        cmd.endRendering();
        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        _stats.mesh_draw_time = elapsed.count() / 1000.f;
    };

    //_____________________________________
    namespace ui {

        void draw(vk::CommandBuffer cmd, vk::ImageView img_view) {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Sigil", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
                                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
            {
                ImGui::TextUnformatted(fmt::format("GPU: {}", phys_device.getProperties().deviceName.data()).c_str());
                ImGui::TextUnformatted(fmt::format("sigil   {}", sigil::version::as_string).c_str());
                ImGui::SetWindowPos(ImVec2(0, swapchain.extent.height - ImGui::GetWindowSize().y));
            }
            ImGui::End();

            ImGui::Begin("Toggle Info", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
                        | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            {
                ImGui::Checkbox("Show Stats", &show_info);
                ImGui::SetWindowPos(ImVec2(4, 4));
            }
            ImGui::End();

            if( show_info ) {
                ImGui::Begin("Render Info", nullptr, ImGuiWindowFlags_NoTitleBar //| ImGuiWindowFlags_NoBackground
                                              | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                                              | ImGuiWindowFlags_NoCollapse /*| ImGuiWindowFlags_NoMouseInputs */);
                {
                    ImGui::TextUnformatted(fmt::format(" FPS:         {:.0f}", time::fps).c_str());
                    ImGui::TextUnformatted(fmt::format(" frametime:   {:.2f}", _stats.frametime).c_str());
                    ImGui::TextUnformatted(fmt::format(" draw time:   {:.2f}", _stats.mesh_draw_time).c_str());
                    ImGui::TextUnformatted(fmt::format(" total ms:    {:.2f}", time::ms).c_str());
                    ImGui::TextUnformatted(fmt::format(" drawcalls:   {}", _stats.drawcalls).c_str());
                    ImGui::TextUnformatted(fmt::format(" tris:        {}", _stats.triangles).c_str());

                    ImGui::SetWindowSize(ImVec2(128, 120));
                    ImGui::SetWindowPos(ImVec2(8, 40));
                }
                ImGui::End();

                ImGui::Begin("Camera Info", nullptr, ImGuiWindowFlags_NoTitleBar   //| ImGuiWindowFlags_NoBackground
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

                    ImGui::SetWindowSize(ImVec2(128, 188));
                    ImGui::SetWindowPos(ImVec2(8, 120 + 48));
                }
                ImGui::End();
            }

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

} // sigil::renderer

