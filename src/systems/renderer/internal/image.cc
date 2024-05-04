#include "image.hh"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace sigil::renderer {

    //_____________________________________
    void transition_img(
        vk::CommandBuffer cmd_buffer,
        vk::Image         img,
        vk::ImageLayout   from,
        vk::ImageLayout   to
    ) {
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

        cmd_buffer.pipelineBarrier2(
            vk::DependencyInfo {
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &img_barrier,
            }
        );
    }

    //_____________________________________
    AllocatedImage create_img(
        vma::Allocator      allocator,
        vk::Extent3D        extent,
        vk::Format          format,
        vk::ImageUsageFlags usage,
        bool                mipmapped
    ) {
        AllocatedImage alloc {
            .img {
                .extent = extent,
                .format = format,
            },
        };

        vk::ImageCreateInfo img_info {
            .imageType   = vk::ImageType::e2D,
            .format      = format,
            .extent      = extent,
            .mipLevels   = (mipmapped) 
                             ? (u32)std::floor(std::log2(std::max(extent.width, extent.height))) + 1
                             : 1,
            .arrayLayers = 1,
            .samples     = vk::SampleCountFlagBits::e1,
            .tiling      = vk::ImageTiling::eOptimal,
            .usage       = usage,
        };

        vma::AllocationCreateInfo alloc_info {
            .usage         = vma::MemoryUsage::eGpuOnly,
            .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
        };
        VK_CHECK(allocator.createImage(&img_info, &alloc_info, &alloc.img.handle, &alloc.img.alloc, nullptr));

        vk::ImageViewCreateInfo view_info {
            .image      = alloc.img.handle,
            .viewType   = vk::ImageViewType::e2D,
            .format     = alloc.img.format,
            .subresourceRange {
                .aspectMask     = (format == vk::Format::eD32Sfloat)
                                    ? vk::ImageAspectFlagBits::eDepth
                                    : vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        VK_CHECK(device.createImageView(&view_info, nullptr, &alloc.img.view));
        _deletion_queue.push_function([=]{
            destroy_img(alloc);
            allocator.destroyImage(alloc.img.handle, alloc.img.alloc);
        });
        return alloc;
    }

    //_____________________________________
    AllocatedImage create_img(
        vma::Allocator      allocator,
        void*               data,
        vk::Extent3D        extent,
        vk::Format          format,
        vk::ImageUsageFlags usage,
        bool                mipmapped
    ) {
        size_t data_size = extent.depth * extent.width * extent.height * 4;
        AllocatedBuffer upload_buffer = create_buffer(data_size, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuToGpu);
        memcpy(upload_buffer.info.pMappedData, data, data_size);

        AllocatedImage img = create_img(
            allocator,
            extent,
            format,
            usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
            mipmapped
        );

        immediate_submit([&](vk::CommandBuffer cmd) {
            transition_img(cmd, img.img.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            vk::BufferImageCopy copy {
                .bufferOffset      = 0,
                .bufferRowLength   = 0,
                .bufferImageHeight = 0,
                .imageSubresource {
                    .aspectMask       = vk::ImageAspectFlagBits::eColor,
                    .mipLevel         = 0,
                    .baseArrayLayer   = 0,
                    .layerCount       = 1,
                },
                .imageExtent       = extent,
            };
            cmd.copyBufferToImage(upload_buffer.handle, img.img.handle, vk::ImageLayout::eTransferDstOptimal, 1, &copy);
            transition_img(cmd, img.img.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        });

        destroy_buffer(upload_buffer);
        return img;
    }

    //_____________________________________
    AllocatedImage load_img(
        vma::Allocator      allocator,
        vk::Format          format,
        vk::ImageUsageFlags usage,
        const char*         path
    ) {
        int width, height;
        stbi_uc* pixels = stbi_load(path, &width, &height, nullptr, STBI_rgb_alpha);
        if( pixels == nullptr ) {
            std::cout << "Error: Failed to load image at: " << path << "\n";
            return _error_img;
        }

        return create_img(
            allocator,
            pixels,
            vk::Extent3D { (u32)width, (u32)height, 1 },
            vk::Format::eR8G8B8A8Unorm,
            vk::ImageUsageFlagBits::eSampled
        );
    }

    //_____________________________________
    void copy_img_to_img(
        vk::CommandBuffer cmd,
        vk::Image         src,
        vk::Image         dst,
        vk::Extent2D      src_extent,
        vk::Extent2D      dst_extent
    ) {
        vk::ImageBlit2 blit_region {
            .srcSubresource = vk::ImageSubresourceLayers {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
            .srcOffsets = {{
                vk::Offset3D { 0, 0, 0 },
                vk::Offset3D { (int)src_extent.width, (int)src_extent.height, 1 },
            }},
            .dstSubresource = vk::ImageSubresourceLayers {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
            .dstOffsets = {{
                vk::Offset3D { 0, 0, 0 },
                vk::Offset3D { (int)dst_extent.width, (int)dst_extent.height, 1 },
            }},
        };

        cmd.blitImage2(
            vk::BlitImageInfo2 {
                .srcImage = src,
                .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
                .dstImage = dst,
                .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                .regionCount = 1,
                .pRegions = &blit_region,
                .filter = vk::Filter::eLinear,
            }
        );
    }

    //_____________________________________
    void destroy_img(const AllocatedImage& img) {
        device.destroyImageView(img.img.view);
        device.destroyImage(img.img.handle);
        device.destroyDescriptorSetLayout(img.descriptor.set.layout);
        device.destroyDescriptorPool(img.descriptor.pool);
    }

} // sigil::renderer

