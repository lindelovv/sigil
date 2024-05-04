#pragma once
#include "vulkan.hh"
#include "descriptors.hh"

namespace sigil::renderer {

    const u8 FRAME_OVERLAP = 2;

    //_____________________________________
    struct FrameData {
        struct { vk::CommandPool   pool;
                 vk::CommandBuffer buffer;
        } cmd;
        vk::Fence     fence;
        vk::Semaphore render_semaphore;
        vk::Semaphore swap_semaphore;
        DescriptorAllocator descriptors;
        DeletionQueue deletion_queue;
    };

    inline std::vector<FrameData> frames { FRAME_OVERLAP };
    inline u32 current_frame = 0;

} // sigil::renderer

