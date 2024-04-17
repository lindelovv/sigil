#include "vulkan.hh"

#include <span>

namespace sigil::renderer {
    //_____________________________________
    void DescriptorAllocator::init(u32 nr_sets, std::span<std::pair<vk::DescriptorType, f32>> pool_ratios) {
        ratios.clear();
        for( auto ratio : pool_ratios ) {
            ratios.push_back(ratio);
        }
        vk::DescriptorPool new_pool = create_pool(nr_sets, pool_ratios);
        sets_per_pool = nr_sets * 1.5f;
        ready_pools.push_back(new_pool);
    }

    vk::DescriptorSet DescriptorAllocator::allocate(vk::DescriptorSetLayout layout) {
        vk::DescriptorPool pool = get_pool();
        vk::DescriptorSet set;
        vk::DescriptorSetAllocateInfo allocate_info {
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
        };
        vk::Result result = device.allocateDescriptorSets(&allocate_info, &set);
        if( result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool ) {
            full_pools.push_back(pool);
            pool = get_pool();
            allocate_info.descriptorPool = pool;
            VK_CHECK(device.allocateDescriptorSets(&allocate_info, &set));
        }
        ready_pools.push_back(pool);
        return set;
    }

    void DescriptorAllocator::clear() {
        for( auto pool : ready_pools ) {
            device.resetDescriptorPool(pool);
        }
        for( auto pool : full_pools ) {
            device.resetDescriptorPool(pool);
            ready_pools.push_back(pool);
        }
        full_pools.clear();
    }

    void DescriptorAllocator::destroy() {
        for( auto pool : ready_pools ) {
            device.destroyDescriptorPool(pool);
        }
        ready_pools.clear();
        for( auto pool : full_pools ) {
            device.destroyDescriptorPool(pool);
        }
        full_pools.clear();
    }

    vk::DescriptorPool DescriptorAllocator::get_pool() {
        vk::DescriptorPool new_pool;
        if( ready_pools.size() != 0 ) {
            new_pool = ready_pools.back();
            ready_pools.pop_back();
        }
        else {
            new_pool = create_pool(sets_per_pool, ratios);
            if( sets_per_pool > 4092 ) {
                sets_per_pool = 4092;
            }
        }
        return new_pool;
    }

    vk::DescriptorPool DescriptorAllocator::create_pool(u32 set_count, std::span<std::pair<vk::DescriptorType, f32>> ratios) {
        std::vector<vk::DescriptorPoolSize> pool_sizes;
        for( auto [type, ratio] : ratios ) {
            pool_sizes.push_back(
                vk::DescriptorPoolSize {
                    .type = type,
                    .descriptorCount = (u32)ratio * set_count,
                }
            );
        }
        return device.createDescriptorPool(
                vk::DescriptorPoolCreateInfo {
                    .flags = vk::DescriptorPoolCreateFlags(),
                    .maxSets = set_count,
                    .poolSizeCount = (u32)pool_sizes.size(),
                    .pPoolSizes = pool_sizes.data(),
                },
                nullptr
            ).value;
    }

    //_____________________________________

    DescriptorWriter& DescriptorWriter::write_img(
        u32 binding,
        vk::ImageView img_view,
        vk::Sampler sampler,
        vk::ImageLayout layout,
        vk::DescriptorType type
    ) {
        vk::DescriptorImageInfo& info = img_info.emplace_back(
            vk::DescriptorImageInfo {
                .sampler = sampler,
                .imageView = img_view,
                .imageLayout = layout,
            }
        );
        writes.push_back(
            vk::WriteDescriptorSet {
                .dstSet = VK_NULL_HANDLE,
                .dstBinding = binding,
                .descriptorCount = 1,
                .descriptorType = type,
                .pImageInfo = &info,
            }
        );
        return *this;
    }

    DescriptorWriter& DescriptorWriter::write_buffer(
        u32 binding,
        vk::Buffer buffer,
        size_t size,
        size_t offset,
        vk::DescriptorType type
    ) {
        vk::DescriptorBufferInfo& info = buffer_info.emplace_back(
            vk::DescriptorBufferInfo {
                .buffer = buffer,
                .offset = offset,
                .range  = size,
            }
        );
        writes.push_back(
            vk::WriteDescriptorSet {
                .dstSet = VK_NULL_HANDLE,
                .dstBinding = binding,
                .descriptorCount = 1,
                .descriptorType = type,
                .pBufferInfo = &info,
            }
        );
        return *this;
    }

    DescriptorWriter& DescriptorWriter::update_set(vk::DescriptorSet set) {
        for( vk::WriteDescriptorSet& write : writes ) {
            write.dstSet = set;
        }
        device.updateDescriptorSets((u32)writes.size(), writes.data(), 0, nullptr);
        return *this;
    }

    DescriptorWriter& DescriptorWriter::clear() {
        img_info.clear();
        buffer_info.clear();
        writes.clear();
        return *this;
    }

    //_____________________________________

    DescriptorLayoutBuilder& DescriptorLayoutBuilder::add_binding(u32 binding, vk::DescriptorType type) {
        bindings.push_back(
            vk::DescriptorSetLayoutBinding {
                .binding = binding,
                .descriptorType = type,
                .descriptorCount = 1,
            }
        );
        return *this;
    }

    DescriptorLayoutBuilder& DescriptorLayoutBuilder::clear() { bindings.clear(); return *this; }

    vk::DescriptorSetLayout DescriptorLayoutBuilder::build(vk::ShaderStageFlags shader_stages) {
        for( auto& bind : bindings ) {
            bind.stageFlags |= shader_stages;
        }
        vk::DescriptorSetLayoutCreateInfo info {
            .flags = vk::DescriptorSetLayoutCreateFlags(),
            .bindingCount = (u32)bindings.size(),
            .pBindings = bindings.data(),
        };
        vk::DescriptorSetLayout set;
        VK_CHECK(device.createDescriptorSetLayout(&info, nullptr, &set));
        return set;
    }
} // sigil::renderer

