#pragma once
#include "vulkan.hh"
#include "descriptors.hh"
#include "image.hh"

namespace sigil::renderer {

    //_____________________________________
    // Swapchain
    struct { vk::SwapchainKHR           handle;
             vk::Format                 img_format;
             std::vector<vk::Image>     images;
             std::vector<vk::ImageView> img_views;
             vk::Extent2D               extent;
             bool      request_resize = false;
    } inline swapchain;

    //_____________________________________
    void build_swapchain() {
        int w, h;
        glfwGetFramebufferSize(window::handle, &w, &h);

        auto capabilities = phys_device.getSurfaceCapabilitiesKHR(surface).value;
        swapchain.handle = device.createSwapchainKHR(
            vk::SwapchainCreateInfoKHR {
                .surface            = surface,
                .minImageCount      = capabilities.minImageCount + 1,
                .imageFormat        = (swapchain.img_format = vk::Format::eB8G8R8A8Unorm),
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
        std::tie( _draw.img.handle, _draw.img.alloc ) = vma_allocator.createImage(
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
        std::tie( _depth.img.handle, _depth.img.alloc ) = vma_allocator.createImage(
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
    void rebuild_swapchain() {
        VK_CHECK(graphics_queue.handle.waitIdle());

        device.destroySwapchainKHR(swapchain.handle);
        //for( auto img : swapchain.images ) {
        //    device.destroyImage(img);
        //}
        for( auto img_view : swapchain.img_views ) {
            device.destroyImageView(img_view);
        }
        swapchain.img_views.clear();
        swapchain.images.clear();

        int w, h;
        glfwGetFramebufferSize(window::handle, &w, &h);

        auto capabilities = phys_device.getSurfaceCapabilitiesKHR(surface).value;
        swapchain.handle = device.createSwapchainKHR(
            vk::SwapchainCreateInfoKHR {
                .surface            = surface,
                .minImageCount      = capabilities.minImageCount + 1,
                .imageFormat        = (swapchain.img_format = vk::Format::eB8G8R8A8Unorm),
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
        swapchain.request_resize = false;
    }

} // sigil::renderer

