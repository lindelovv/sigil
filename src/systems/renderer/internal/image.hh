#pragma once
#include "vulkan.hh"
#include "descriptors.hh"

namespace sigil::renderer {

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
        DescriptorData descriptor;
    };

    inline AllocatedImage _draw;
    inline AllocatedImage _depth;
    inline AllocatedImage _error_img;
    inline AllocatedImage _white_img;
    inline AllocatedImage _albedo_img;
    inline AllocatedImage _metal_roughness_img;
    inline AllocatedImage _normal_img;
    inline AllocatedImage _emissive_img;
    inline AllocatedImage _AO_img;

    void transition_img(
        vk::CommandBuffer cmd_buffer,
        vk::Image         img,
        vk::ImageLayout   from,
        vk::ImageLayout   to
    );

    AllocatedImage create_img(
        vma::Allocator      allocator,
        vk::Extent3D        extent,
        vk::Format          format,
        vk::ImageUsageFlags usage,
        bool                mipmapped = false
    );

    AllocatedImage create_img(
        vma::Allocator      allocator,
        void*               data,
        vk::Extent3D        extent,
        vk::Format          format,
        vk::ImageUsageFlags usage,
        bool                mipmapped = false
    );

    AllocatedImage load_img(
        vma::Allocator      allocator,
        vk::Format          format,
        vk::ImageUsageFlags usage,
        const char*         path
    );

    void copy_img_to_img(
        vk::CommandBuffer cmd,
        vk::Image         src,
        vk::Image         dst,
        vk::Extent2D      src_extent,
        vk::Extent2D      dst_extent
    );

    void destroy_img(const AllocatedImage& img);

} // sigil::renderer

