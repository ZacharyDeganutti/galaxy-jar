#include "vk_descriptors.hpp"

namespace vk_descriptors {

    void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> pool_ratios) {
        std::vector<VkDescriptorPoolSize> pool_sizes;
        for (PoolSizeRatio ratio : pool_ratios) {
            pool_sizes.push_back(VkDescriptorPoolSize{
                .type = ratio.type,
                .descriptorCount = uint32_t(ratio.ratio * maxSets)
            });
        }

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = 0;
        pool_info.maxSets = maxSets;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();

        vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
    }

    void DescriptorAllocator::clear_descriptors(const VkDevice device)
    {
        vkResetDescriptorPool(device, pool, 0);
    }

    void DescriptorAllocator::destroy_pool(const VkDevice device)
    {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }

    VkDescriptorSet DescriptorAllocator::allocate(const VkDevice device, const VkDescriptorSetLayout layout) {
        VkDescriptorSetAllocateInfo alloc_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        alloc_info.pNext = nullptr;
        alloc_info.descriptorPool = pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &layout;

        VkDescriptorSet ds;
        if(vkAllocateDescriptorSets(device, &alloc_info, &ds) != VK_SUCCESS) {
            printf("Failure to allocate descriptor set!\n");
            exit(EXIT_FAILURE);
        };

        return ds;
    }

    // Function to initialize descriptor layout
    VkDescriptorSetLayout init_descriptor_layout(const VkDevice device, VkShaderStageFlagBits stage, const std::vector<VkDescriptorType>& descriptor_types, vk_types::CleanupProcedures& cleanup_procedures) {

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (size_t i = 0; i < descriptor_types.size(); ++i) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = static_cast<uint32_t>(i);
            binding.descriptorType = descriptor_types[i];
            binding.descriptorCount = 1;
            binding.stageFlags = stage;
            binding.pImmutableSamplers = nullptr;
            bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.pNext = nullptr;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        VkDescriptorSetLayout descriptor_layout{};
        if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &descriptor_layout) != VK_SUCCESS) {
            printf("Failed to create descriptor set layout!\n");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, descriptor_layout]() mutable {
            vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
        });

        return descriptor_layout;
    }

    // Function to initialize image descriptors
    VkDescriptorSet init_image_descriptors(const VkDevice device, const VkImageView image_view, const VkDescriptorSetLayout descriptor_layout, DescriptorAllocator& descriptor_allocator, vk_types::CleanupProcedures& cleanup_procedures) {

        // Create a descriptor pool that will hold 10 sets with 1 image each
        std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
        {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
        };

        descriptor_allocator.init_pool(device, 10, sizes);

        // Allocate a descriptor set for our draw image
        VkDescriptorSet draw_descriptors = descriptor_allocator.allocate(device, descriptor_layout);

        VkDescriptorImageInfo img_info{};
        img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        img_info.imageView = image_view;

        VkWriteDescriptorSet draw_image_write = {};
        draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        draw_image_write.pNext = nullptr;

        draw_image_write.dstBinding = 0;
        draw_image_write.dstSet = draw_descriptors;
        draw_image_write.descriptorCount = 1;
        draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        draw_image_write.pImageInfo = &img_info;

        vkUpdateDescriptorSets(device, 1, &draw_image_write, 0, nullptr);

        cleanup_procedures.add([device, descriptor_allocator]() mutable {
            descriptor_allocator.destroy_pool(device);
        });

        return draw_descriptors;
    }

    // Function to initialize combined image sampler descriptors
    VkDescriptorSet init_combined_image_sampler_descriptors(const VkDevice device, const VkImageView image_view, const VkSampler sampler, const VkDescriptorSetLayout descriptor_layout, DescriptorAllocator& descriptor_allocator, vk_types::CleanupProcedures& cleanup_procedures) {

        // Create a descriptor pool that will hold 10 sets with 1 combined image sampler each
        std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
        {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
        };

        descriptor_allocator.init_pool(device, 10, sizes);

        // Allocate a descriptor set for our draw image
        VkDescriptorSet draw_descriptors = descriptor_allocator.allocate(device, descriptor_layout);

        VkDescriptorImageInfo img_info{};
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_info.imageView = image_view;
        img_info.sampler = sampler;

        VkWriteDescriptorSet draw_image_write = {};
        draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        draw_image_write.pNext = nullptr;

        draw_image_write.dstBinding = 0;
        draw_image_write.dstSet = draw_descriptors;
        draw_image_write.descriptorCount = 1;
        draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        draw_image_write.pImageInfo = &img_info;

        vkUpdateDescriptorSets(device, 1, &draw_image_write, 0, nullptr);

        cleanup_procedures.add([device, descriptor_allocator]() mutable {
            descriptor_allocator.destroy_pool(device);
        });

        return draw_descriptors;
    }

    // Function to initialize buffer descriptors
    VkDescriptorSet init_buffer_descriptors(const VkDevice device, const VkBuffer buffer, DescriptorType buffer_type, const VkDescriptorSetLayout descriptor_layout, DescriptorAllocator& descriptor_allocator, vk_types::CleanupProcedures& cleanup_procedures) {

        VkDescriptorType vk_buffer_type = static_cast<VkDescriptorType>(buffer_type);

        // Create a descriptor pool that will hold 10 sets with 1 buffer each
        std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
        {
            {vk_buffer_type, 1}
        };

        descriptor_allocator.init_pool(device, 10, sizes);

        // Allocate a descriptor set for our buffer
        VkDescriptorSet buffer_descriptors = descriptor_allocator.allocate(device, descriptor_layout);

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = buffer;
        buffer_info.offset = 0;
        buffer_info.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet buffer_write = {};
        buffer_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        buffer_write.pNext = nullptr;

        buffer_write.dstBinding = 0;
        buffer_write.dstSet = buffer_descriptors;
        buffer_write.descriptorCount = 1;
        buffer_write.descriptorType = vk_buffer_type;
        buffer_write.pBufferInfo = &buffer_info;

        vkUpdateDescriptorSets(device, 1, &buffer_write, 0, nullptr);

        cleanup_procedures.add([device, descriptor_allocator]() mutable {
            descriptor_allocator.destroy_pool(device);
        });

        return buffer_descriptors;
    }

}  // namespace vk_descriptors