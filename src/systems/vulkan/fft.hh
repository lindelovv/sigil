
#pragma once

#include "vulkan.hh"

namespace sigil::renderer {

    //struct FFT {
    //    struct {
    //        struct {
    //            vk::Image       handle;
    //            vk::ImageView   view;
    //            vma::Allocation alloc;
    //            vk::Extent3D    extent;
    //            vk::Format      format;
    //        } pingpong_img;
    //        vk::Extent2D extent;
    //        DescriptorData butterfly_descriptor;
    //        DescriptorData inversion_descriptor;
    //    } _dy;
    //    struct {
    //        struct {
    //            vk::Image       handle;
    //            vk::ImageView   view;
    //            vma::Allocation alloc;
    //            vk::Extent3D    extent;
    //            vk::Format      format;
    //        } pingpong_img;
    //        vk::Extent2D extent;
    //        DescriptorData butterfly_descriptor;
    //        DescriptorData inversion_descriptor;
    //    } _dx;
    //    struct {
    //        struct {
    //            vk::Image       handle;
    //            vk::ImageView   view;
    //            vma::Allocation alloc;
    //            vk::Extent3D    extent;
    //            vk::Format      format;
    //        } pingpong_img;
    //        vk::Extent2D extent;
    //        DescriptorData butterfly_descriptor;
    //        DescriptorData inversion_descriptor;
    //    } _dz;
    //    struct HorzPushConstants {

    //    };
    //    struct VertPushConstants {

    //    };
    //    struct InvrPushConstants {

    //    };
    //} inline fft;

    //_____________________________________
    struct FFT {
        vk::Format format = vk::Format::eR32G32B32A32Sfloat;

        struct TwiddleFactors {
            struct PushConstants {
                u32 N;
            };
            struct UniformBufferObject {
                u32 N;
            } _ubo;

            AllocatedImage alloc_img;

            vk::Pipeline pipeline;
            vk::PipelineLayout pipeline_layout;
            vk::DescriptorSet descriptor;
            vk::DescriptorSetLayout descriptor_layout;
            DescriptorAllocator descriptor_alloc;
            vk::Buffer buffer;
            vk::Semaphore signal_semaphore;
            vk::SubmitInfo2 submit_info;
            f32 t;

            vk::Format format = vk::Format::eR32G32B32A32Sfloat;
            
            void init(u32 N) {
                u32 log_2_n = std::log(N) / log(2);
                alloc_img = create_img(vk::Extent3D { log_2_n, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);

                AllocatedBuffer data_buffer = create_buffer(sizeof(TwiddleFactors::UniformBufferObject), vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eCpuToGpu);

                _deletion_queue.push_function([=] {
                    destroy_buffer(data_buffer);
                });

                TwiddleFactors::UniformBufferObject* uniform_data = (TwiddleFactors::UniformBufferObject*)data_buffer.info.pMappedData;
                *uniform_data = _ubo;

                std::vector<std::pair<vk::DescriptorType, f32>> sizes {
                    { vk::DescriptorType::eStorageImage,  1 },
                    { vk::DescriptorType::eStorageBuffer, 1 },
                };
                descriptor_alloc.init(10, sizes);
                descriptor_layout = DescriptorLayoutBuilder {}
                    .add_binding(0, vk::DescriptorType::eStorageImage)
                    .add_binding(1, vk::DescriptorType::eStorageBuffer)
                    .build(vk::ShaderStageFlagBits::eCompute);
                descriptor = descriptor_alloc.allocate(descriptor_layout);

                DescriptorWriter {} 
                    .write_img(0, alloc_img.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                    .write_buffer(1, data_buffer.handle, sizeof(TwiddleFactors::UniformBufferObject), 0, vk::DescriptorType::eStorageBuffer)
                    .update_set(descriptor);

                vk::PushConstantRange push_const {
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                    .offset = 0,
                    .size = sizeof(TwiddleFactors::PushConstants),
                };

                pipeline_layout = device.createPipelineLayout(
                    vk::PipelineLayoutCreateInfo {
                        .setLayoutCount = 1,
                        .pSetLayouts = &descriptor_layout,
                        .pushConstantRangeCount = 1,
                        .pPushConstantRanges = &push_const,
                    }
                ).value;

                vk::ShaderModule twiddle_factors = load_shader_module("res/shaders/fft/twiddle_factors.comp.spv").value;
                vk::PipelineShaderStageCreateInfo stage_info {
                    .stage  = vk::ShaderStageFlagBits::eCompute,
                    .module = twiddle_factors,
                    .pName  = "main",
                };
                vk::ComputePipelineCreateInfo pipe_info {
                    .stage = stage_info,
                    .layout = pipeline_layout,
                };

                pipeline = device.createComputePipelines(nullptr, pipe_info).value.front();

                immediate_submit([=](vk::CommandBuffer cmd) {
                    transition_img(cmd, alloc_img.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
                    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, descriptor, nullptr);
                    PushConstants pc {
                        .N = N,
                    };
                    cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(TwiddleFactors::PushConstants), &pc);
                    cmd.dispatch(std::ceil(N / 16.f), std::ceil(N / 16.f), 1);
                });
            }
        } twiddle_factors;

        struct H0K {
            struct PushConstants {
                u32 N;
                u32 L;
                f32 amplitude;
                f32 wind_speed;
                glm::vec2 wind_dir;
                f32 capillar_sf;
            };
            struct UniformBufferObject {
                f32 t;
            };

            AllocatedImage h0k;
            AllocatedImage h0_minus_k;

            vk::Pipeline pipeline;
            vk::PipelineLayout pipeline_layout;
            vk::DescriptorSet descriptor;
            vk::DescriptorSetLayout descriptor_layout;
            DescriptorAllocator descriptor_alloc;
            vk::Buffer buffer;
            vk::CommandPool pool;
            vk::CommandBuffer cmd;
            vk::Semaphore signal_semaphore;
            vk::SubmitInfo2 submit_info;
            f32 t;

            vk::Format format = vk::Format::eR32G32B32A32Sfloat;
            
            void init(u32 N, u32 L, f32 amplitude, glm::vec2 wind_dir, f32 wind_speed, f32 capillar_sf) {

                h0k = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
                h0_minus_k = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);

                std::vector<std::pair<vk::DescriptorType, f32>> sizes {
                    { vk::DescriptorType::eStorageImage,  1 },
                };
                descriptor_alloc.init(10, sizes);
                descriptor_layout = DescriptorLayoutBuilder {}
                    .add_binding(0, vk::DescriptorType::eStorageImage)
                    .add_binding(1, vk::DescriptorType::eStorageImage)
                    .build(vk::ShaderStageFlagBits::eCompute);
                descriptor = descriptor_alloc.allocate(descriptor_layout);

                DescriptorWriter {} 
                    .write_img(0, h0k.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                    .write_img(1, h0_minus_k.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                    .update_set(descriptor);

                vk::PushConstantRange push_const {
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                    .offset = 0,
                    .size = sizeof(H0K::PushConstants),
                };

                pipeline_layout = device.createPipelineLayout(
                        vk::PipelineLayoutCreateInfo {
                            .setLayoutCount = 1,
                            .pSetLayouts = &descriptor_layout,
                            .pushConstantRangeCount = 1,
                            .pPushConstantRanges = &push_const,
                        }
                ).value;

                vk::ShaderModule h0k_module = load_shader_module("res/shaders/fft/h0k.comp.spv").value;
                vk::PipelineShaderStageCreateInfo stage_info {
                    .stage  = vk::ShaderStageFlagBits::eCompute,
                    .module = h0k_module,
                    .pName  = "main",
                };
                vk::ComputePipelineCreateInfo pipe_info {
                    .stage = stage_info,
                    .layout = pipeline_layout,
                };

                pipeline  = device.createComputePipelines(nullptr, pipe_info).value.front();

                immediate_submit([=](vk::CommandBuffer cmd) {
                    transition_img(cmd, h0k.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                    transition_img(cmd, h0_minus_k.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

                    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
                    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, descriptor, nullptr);
                    PushConstants pc {
                        .N = N,
                        .L = L,
                        .amplitude = amplitude,
                        .wind_speed = wind_speed,
                        .wind_dir = wind_dir,
                        .capillar_sf = capillar_sf,
                    };
                    cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(H0K::PushConstants), &pc);
                    cmd.dispatch(std::ceil(N / 16.f), std::ceil(N / 16.f), 1);
                });
            }
        } h0k;

        struct HKT {
            struct PushConstants {
                u32 N;
                u32 L;
            };
            struct UniformBufferObject {
                f32 t;
            } _ubo_layout;
            vk::Pipeline pipeline;
            vk::PipelineLayout pipeline_layout;
            vk::DescriptorSet descriptor;
            vk::DescriptorSetLayout descriptor_layout;
            DescriptorAllocator descriptor_alloc;
            vk::Buffer buffer;
            vk::CommandPool pool;
            vk::CommandBuffer cmd;
            vk::Semaphore signal_semaphore;
            vk::SubmitInfo2 submit_info;
            f32 t;
            f32 t_delta;

            vk::Format format = vk::Format::eR32G32B32A32Sfloat;

            AllocatedImage dx_coeff;
            AllocatedImage dy_coeff;
            AllocatedImage dz_coeff;
            
            void init(u32 N, u32 L, f32 in_t_delta, vk::ImageView tilde_h0k_view, vk::ImageView tilde_h0_minus_k_view) {
                t_delta = in_t_delta;

                dx_coeff = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
                dy_coeff = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
                dz_coeff = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
                
                AllocatedBuffer gpu_scene_data_buffer = create_buffer(sizeof(GPUSceneData), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);

                _deletion_queue.push_function([=] {
                    destroy_buffer(gpu_scene_data_buffer);
                });

                GPUSceneData* scene_uniform_data = (GPUSceneData*)gpu_scene_data_buffer.info.pMappedData;
                *scene_uniform_data = _scene_data;

                std::vector<std::pair<vk::DescriptorType, f32>> sizes {
                    { vk::DescriptorType::eStorageImage,  1 },
                    { vk::DescriptorType::eUniformBuffer, 1 },
                };
                descriptor_alloc.init(10, sizes);
                descriptor_layout = DescriptorLayoutBuilder {}
                    .add_binding(0, vk::DescriptorType::eStorageImage)
                    .add_binding(1, vk::DescriptorType::eStorageImage)
                    .add_binding(2, vk::DescriptorType::eStorageImage)
                    .add_binding(3, vk::DescriptorType::eStorageImage)
                    .add_binding(4, vk::DescriptorType::eStorageImage)
                    .add_binding(5, vk::DescriptorType::eUniformBuffer)
                    .build(vk::ShaderStageFlagBits::eCompute);
                descriptor = descriptor_alloc.allocate(descriptor_layout);

                DescriptorWriter {} 
                    .write_img(0, dy_coeff.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                    .write_img(1, dx_coeff.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                    .write_img(2, dz_coeff.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                    .write_img(3, tilde_h0k_view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                    .write_img(4, tilde_h0_minus_k_view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                    .write_buffer(5, gpu_scene_data_buffer.handle, sizeof(GPUSceneData), 0, vk::DescriptorType::eUniformBuffer)
                    .update_set(descriptor);

                vk::PushConstantRange push_const {
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                    .offset = 0,
                    .size = sizeof(HKT::PushConstants),
                };

                pipeline_layout = device.createPipelineLayout(
                    vk::PipelineLayoutCreateInfo {
                        .setLayoutCount = 1,
                        .pSetLayouts = &descriptor_layout,
                        .pushConstantRangeCount = 1,
                        .pPushConstantRanges = &push_const,
                    }
                ).value;

                vk::ShaderModule hkt = load_shader_module("res/shaders/fft/hkt.comp.spv").value;
                vk::PipelineShaderStageCreateInfo stage_info {
                    .stage  = vk::ShaderStageFlagBits::eCompute,
                    .module = hkt,
                    .pName  = "main",
                };
                vk::ComputePipelineCreateInfo pipe_info {
                    .stage = stage_info,
                    .layout = pipeline_layout,
                };

                pipeline  = device.createComputePipelines(nullptr, pipe_info).value.front();

                immediate_submit([=](vk::CommandBuffer cmd) {
                    transition_img(cmd, dx_coeff.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                    transition_img(cmd, dy_coeff.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                    transition_img(cmd, dz_coeff.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

                    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
                    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, descriptor, nullptr);
                    PushConstants pc {
                        .N = N,
                        .L = L,
                    };
                    cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(HKT::PushConstants), &pc);
                    cmd.dispatch(std::ceil(N / 16.f), std::ceil(N / 16.f), 1);
                });
            }
        } hkt;

        struct PushConstants {
            u32 stage;
            u32 ping;
            u32 direction;
        };

        vk::Pipeline pipeline;
        vk::PipelineLayout pipeline_layout;
        vk::DescriptorSet descriptor;
        vk::DescriptorSetLayout descriptor_layout;
        DescriptorAllocator descriptor_alloc;
        vk::Buffer buffer;
        vk::CommandPool pool;
        vk::CommandBuffer cmd;
        vk::Semaphore signal_semaphore;
        vk::SubmitInfo2 submit_info;

        vk::Pipeline bufferfly_pipeline;
        vk::DescriptorSet butterfly_descriptor;

        vk::Pipeline inversion_pipeline;
        vk::DescriptorSet inversion_descriptor;

        AllocatedImage _dy_ping;
        AllocatedImage _dx_ping;
        AllocatedImage _dz_ping;

        AllocatedImage _dy;
        AllocatedImage _dx;
        AllocatedImage _dz;

        void init(u32 N, u32 L, f32 t_delta, f32 amplitude, glm::vec2 direction, f32 intensity, float capillar_sf) {
            
            _dy_ping = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
            _dx_ping = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
            _dz_ping = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
            _dy = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
            _dx = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
            _dz = create_img(vk::Extent3D { N, N, 1 }, format, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);

            twiddle_factors.init(N);
            h0k.init(N, L, amplitude, direction, intensity, capillar_sf);
            hkt.init(N, L, t_delta, h0k.h0k.img.view, h0k.h0_minus_k.img.view);

            u32 stages = std::log(N) / std::log(2);
            
            std::vector<std::pair<vk::DescriptorType, f32>> sizes {
                { vk::DescriptorType::eStorageImage,  1 },
                { vk::DescriptorType::eUniformBuffer, 1 },
            };
            descriptor_alloc.init(10, sizes);
            descriptor_layout = DescriptorLayoutBuilder {}
                .add_binding(0, vk::DescriptorType::eStorageImage)
                .add_binding(1, vk::DescriptorType::eStorageImage)
                .add_binding(2, vk::DescriptorType::eStorageImage)
                .add_binding(3, vk::DescriptorType::eStorageImage)
                .add_binding(4, vk::DescriptorType::eStorageImage)
                .add_binding(5, vk::DescriptorType::eUniformBuffer)
                .build(vk::ShaderStageFlagBits::eCompute);
            descriptor = descriptor_alloc.allocate(descriptor_layout);

            DescriptorWriter {} 
                .write_img(0, _dy.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                .write_img(1, _dx.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                .write_img(2, _dz.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
                .update_set(descriptor);

            vk::PushConstantRange push_const {
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .offset = 0,
                .size = sizeof(FFT::PushConstants),
            };

            pipeline_layout = device.createPipelineLayout(
                vk::PipelineLayoutCreateInfo {
                    .setLayoutCount = 1,
                    .pSetLayouts = &descriptor_layout,
                    .pushConstantRangeCount = 1,
                    .pPushConstantRanges = &push_const,
                }
            ).value;

            vk::ShaderModule fft = load_shader_module("res/shaders/fft/butterfly.comp.spv").value;
            vk::PipelineShaderStageCreateInfo stage_info {
                .stage  = vk::ShaderStageFlagBits::eCompute,
                .module = fft,
                .pName  = "main",
            };
            vk::ComputePipelineCreateInfo pipe_info {
                .stage = stage_info,
                .layout = pipeline_layout,
            };

            pipeline = device.createComputePipelines(nullptr, pipe_info).value.front();

            immediate_submit([=](vk::CommandBuffer cmd) {
                transition_img(cmd, _dy_ping.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                transition_img(cmd, _dx_ping.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                transition_img(cmd, _dz_ping.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

                transition_img(cmd, _dy.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                transition_img(cmd, _dx.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                transition_img(cmd, _dz.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

                cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, descriptor, nullptr);
                PushConstants pc {
                    .stage = 0,
                    .ping = 0,
                    .direction = 0,
                };
                cmd.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(FFT::PushConstants), &pc);
                cmd.dispatch(std::ceil(N / 16.f), std::ceil(N / 16.f), 1);
            });
        }

    } inline fft;

    //struct Spectrum {
    //    struct UniformBufferObject {
    //        u32 N;
    //        u32 size;
    //        glm::vec2 wind;
    //    };
    //    struct UniformBufferObject2 {
    //        u32 size;
    //        f32 choppiness;
    //    };

    //    AllocatedImage initial_spectrum;

    //    vk::Pipeline pipeline;
    //    vk::PipelineLayout pipeline_layout;
    //    vk::DescriptorSet descriptor;
    //    vk::DescriptorSetLayout descriptor_layout;
    //    DescriptorAllocator descriptor_alloc;

    //    struct {
    //        AllocatedImage phases;

    //        vk::Pipeline pipeline;
    //        vk::PipelineLayout pipeline_layout;
    //        vk::DescriptorSet descriptor;
    //        vk::DescriptorSetLayout descriptor_layout;
    //        DescriptorAllocator descriptor_alloc;
    //    } phase;
    //    struct {
    //        AllocatedImage spectrum;

    //        vk::Pipeline pipeline;
    //        vk::PipelineLayout pipeline_layout;
    //        vk::DescriptorSet descriptor;
    //        vk::DescriptorSetLayout descriptor_layout;
    //        DescriptorAllocator descriptor_alloc;
    //    } spec;
    //    void init(u32 N) {
    //        initial_spectrum = create_img(vk::Extent3D { N, N, 1 }, vk::Format::eR32Sfloat, vk::ImageUsageFlagBits::eStorage);

    //        AllocatedBuffer data_buffer = create_buffer(sizeof(Spectrum::UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);

    //        _deletion_queue.push_function([=] {
    //            destroy_buffer(data_buffer);
    //        });

    //        Spectrum::UniformBufferObject* ubo = (Spectrum::UniformBufferObject*)data_buffer.info.pMappedData;
    //        ubo->N = N;
    //        ubo->size = N * 2;
    //        float wind_angle = glm::radians(180.f);
    //        ubo->wind = glm::vec2(10 * glm::cos(wind_angle), 10 * glm::sin(wind_angle));

    //        std::vector<std::pair<vk::DescriptorType, f32>> sizes {
    //            { vk::DescriptorType::eStorageImage,  1 },
    //            { vk::DescriptorType::eUniformBuffer, 1 },
    //        };
    //        descriptor_alloc.init(10, sizes);
    //        descriptor_layout = DescriptorLayoutBuilder {}
    //            .add_binding(0, vk::DescriptorType::eStorageImage)
    //            .add_binding(1, vk::DescriptorType::eUniformBuffer)
    //            .build(vk::ShaderStageFlagBits::eCompute);
    //        descriptor = descriptor_alloc.allocate(descriptor_layout);

    //        DescriptorWriter {} 
    //            .write_img(0, initial_spectrum.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
    //            .write_buffer(1, data_buffer.handle, sizeof(Spectrum::UniformBufferObject), 0, vk::DescriptorType::eUniformBuffer)
    //            .update_set(descriptor);

    //        pipeline_layout = device.createPipelineLayout(
    //            vk::PipelineLayoutCreateInfo {
    //                .setLayoutCount = 1,
    //                .pSetLayouts = &descriptor_layout,
    //            }
    //        ).value;

    //        vk::ShaderModule module = load_shader_module("res/shaders/init_spectrum.comp.spv").value;
    //        vk::PipelineShaderStageCreateInfo stage_info {
    //            .stage  = vk::ShaderStageFlagBits::eCompute,
    //            .module = module,
    //            .pName  = "main",
    //        };
    //        vk::ComputePipelineCreateInfo pipe_info {
    //            .stage = stage_info,
    //            .layout = pipeline_layout,
    //        };

    //        pipeline = device.createComputePipelines(nullptr, pipe_info).value.front();

    //        immediate_submit([=](vk::CommandBuffer cmd) {
    //            transition_img(cmd, initial_spectrum.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    //            cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    //            cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, descriptor, nullptr);
    //            cmd.dispatch(N / 32, N / 32, 1);
    //        });

    //        spec.phases = create_img(vk::Extent3D { N, N, 1 }, vk::Format::eR32Sfloat, vk::ImageUsageFlagBits::eStorage);

    //        AllocatedBuffer data_buffer2 = create_buffer(sizeof(Spectrum::UniformBufferObject2), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);

    //        _deletion_queue.push_function([=] {
    //            destroy_buffer(data_buffer2);
    //        });

    //        Spectrum::UniformBufferObject2* ubo2 = (Spectrum::UniformBufferObject2*)data_buffer2.info.pMappedData;
    //        ubo2->size = N * 2;
    //        ubo2->choppiness = 1.f;

    //        std::vector<std::pair<vk::DescriptorType, f32>> sizes2 {
    //            { vk::DescriptorType::eStorageImage,  1 },
    //            { vk::DescriptorType::eUniformBuffer, 1 },
    //        };
    //        spec.descriptor_alloc.init(10, sizes2);
    //        spec.descriptor_layout = DescriptorLayoutBuilder {}
    //            .add_binding(0, vk::DescriptorType::eStorageImage)
    //            .add_binding(1, vk::DescriptorType::eStorageImage)
    //            .add_binding(2, vk::DescriptorType::eStorageImage)
    //            .add_binding(3, vk::DescriptorType::eUniformBuffer)
    //            .build(vk::ShaderStageFlagBits::eCompute);
    //        spec.descriptor = spec.descriptor_alloc.allocate(spec.descriptor_layout);

    //        DescriptorWriter {} 
    //            .write_img(0, spec.phases.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
    //            .write_img(1, initial_spectrum.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
    //            .write_img(2, spec.spectrum.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
    //            .write_buffer(3, data_buffer2.handle, sizeof(Spectrum::UniformBufferObject2), 0, vk::DescriptorType::eUniformBuffer)
    //            .update_set(spec.descriptor);

    //        spec.pipeline_layout = device.createPipelineLayout(
    //            vk::PipelineLayoutCreateInfo {
    //                .setLayoutCount = 1,
    //                .pSetLayouts = &spec.descriptor_layout,
    //            }
    //        ).value;

    //        vk::ShaderModule module2 = load_shader_module("res/shaders/spectrum.comp.spv").value;
    //        vk::PipelineShaderStageCreateInfo stage_info2 {
    //            .stage  = vk::ShaderStageFlagBits::eCompute,
    //            .module = module2,
    //            .pName  = "main",
    //        };
    //        vk::ComputePipelineCreateInfo pipe_info2 {
    //            .stage = stage_info2,
    //            .layout = spec.pipeline_layout,
    //        };

    //        spec.pipeline = device.createComputePipelines(nullptr, pipe_info2).value.front();

    //        immediate_submit([=](vk::CommandBuffer cmd) {
    //            transition_img(cmd, spec.phases.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    //            cmd.bindPipeline(vk::PipelineBindPoint::eCompute, spec.pipeline);
    //            cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, spec.pipeline_layout, 0, spec.descriptor, nullptr);
    //            cmd.dispatch(N / 32, N / 32, 1);
    //        });
    //        spec.spectrum = create_img(vk::Extent3D { N, N, 1 }, vk::Format::eR32G32B32A32Sfloat, vk::ImageUsageFlagBits::eStorage);

    //        AllocatedBuffer data_buffer2 = create_buffer(sizeof(Spectrum::UniformBufferObject2), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);

    //        _deletion_queue.push_function([=] {
    //            destroy_buffer(data_buffer2);
    //        });

    //        Spectrum::UniformBufferObject2* ubo2 = (Spectrum::UniformBufferObject2*)data_buffer2.info.pMappedData;
    //        ubo2->size = N * 2;
    //        ubo2->choppiness = 1.f;

    //        std::vector<std::pair<vk::DescriptorType, f32>> sizes2 {
    //            { vk::DescriptorType::eStorageImage,  1 },
    //            { vk::DescriptorType::eUniformBuffer, 1 },
    //        };
    //        spec.descriptor_alloc.init(10, sizes2);
    //        spec.descriptor_layout = DescriptorLayoutBuilder {}
    //            .add_binding(0, vk::DescriptorType::eStorageImage)
    //            .add_binding(1, vk::DescriptorType::eStorageImage)
    //            .add_binding(2, vk::DescriptorType::eStorageImage)
    //            .add_binding(3, vk::DescriptorType::eUniformBuffer)
    //            .build(vk::ShaderStageFlagBits::eCompute);
    //        spec.descriptor = spec.descriptor_alloc.allocate(spec.descriptor_layout);

    //        DescriptorWriter {} 
    //            .write_img(0, spec.phases.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
    //            .write_img(1, initial_spectrum.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
    //            .write_img(2, spec.spectrum.img.view, VK_NULL_HANDLE, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage)
    //            .write_buffer(3, data_buffer2.handle, sizeof(Spectrum::UniformBufferObject2), 0, vk::DescriptorType::eUniformBuffer)
    //            .update_set(spec.descriptor);

    //        spec.pipeline_layout = device.createPipelineLayout(
    //            vk::PipelineLayoutCreateInfo {
    //                .setLayoutCount = 1,
    //                .pSetLayouts = &spec.descriptor_layout,
    //            }
    //        ).value;

    //        vk::ShaderModule module2 = load_shader_module("res/shaders/spectrum.comp.spv").value;
    //        vk::PipelineShaderStageCreateInfo stage_info2 {
    //            .stage  = vk::ShaderStageFlagBits::eCompute,
    //            .module = module2,
    //            .pName  = "main",
    //        };
    //        vk::ComputePipelineCreateInfo pipe_info2 {
    //            .stage = stage_info2,
    //            .layout = spec.pipeline_layout,
    //        };

    //        spec.pipeline = device.createComputePipelines(nullptr, pipe_info2).value.front();

    //        immediate_submit([=](vk::CommandBuffer cmd) {
    //            transition_img(cmd, spec.phases.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    //            transition_img(cmd, spec.spectrum.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    //            cmd.bindPipeline(vk::PipelineBindPoint::eCompute, spec.pipeline);
    //            cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, spec.pipeline_layout, 0, spec.descriptor, nullptr);
    //            cmd.dispatch(N / 32, N / 32, 1);
    //        });
    //    }
    //} inline _spectrum;

} // sigil::renderer

