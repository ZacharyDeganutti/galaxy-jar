#ifndef VK_DESCRIPTORS_H_
#define VK_DESCRIPTORS_H_

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "vk_types.hpp"

namespace vk_descriptors {

    // Enum for descriptor types
    enum class DescriptorType : uint32_t {
        UniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        StorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        SampledImage = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        StorageImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        CombinedImageSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        // Add more descriptor types as needed
    };

    struct DescriptorAllocator {
        struct PoolSizeRatio{
            VkDescriptorType type;
            float ratio;
        };

        VkDescriptorPool pool;

        void init_pool(const VkDevice device, const uint32_t maxSets, const std::span<PoolSizeRatio> pool_ratios);
        void clear_descriptors(const VkDevice device);
        void destroy_pool(const VkDevice device);

        VkDescriptorSet allocate(const VkDevice device, const VkDescriptorSetLayout layout);
    };

    // Function to initialize descriptor layout. Assumes one descriptor per type.
    VkDescriptorSetLayout init_descriptor_layout(const VkDevice device, VkShaderStageFlagBits stage, const std::vector<VkDescriptorType>& descriptor_types, vk_types::CleanupProcedures& cleanup_procedures);
    // Same but can specify descriptor counts per type. Handy for large blocks of descriptors used in descriptor indexing
    VkDescriptorSetLayout init_descriptor_layout(const VkDevice device, VkShaderStageFlagBits stage, const uint32_t descriptor_count_per_type, const std::vector<VkDescriptorType>& descriptor_types, vk_types::CleanupProcedures& cleanup_procedures);
    
    // Function to initialize image descriptors
    VkDescriptorSet init_image_descriptors(const VkDevice device, const VkImageView image_view, const VkDescriptorSetLayout descriptor_layout, DescriptorAllocator& descriptor_allocator, vk_types::CleanupProcedures& cleanup_procedures);

    // Function to initialize combine image sampler descriptors
    VkDescriptorSet init_combined_image_sampler_descriptors(const VkDevice device, const VkImageView image_view, const VkSampler sampler, const VkDescriptorSetLayout descriptor_layout, DescriptorAllocator& descriptor_allocator, vk_types::CleanupProcedures& cleanup_procedures);

    // Function to initialize buffer descriptors
    VkDescriptorSet init_buffer_descriptors(const VkDevice device, const VkBuffer buffer, DescriptorType buffer_type, const VkDescriptorSetLayout descriptor_layout, DescriptorAllocator& descriptor_allocator, vk_types::CleanupProcedures& cleanup_procedures);

    // Make a fat descriptor set for descriptor indexing
    MegaDescriptorSet init_mega_descriptor_set(const VkDevice device, DescriptorAllocator& descriptor_allocator, size_t pool_sizes, vk_types::CleanupProcedures& cleanup_procedures);

    // TODO: Generalize descriptor creation in descriptor_set_builder.
    // This will allow divorce from combined image samplers for cases where it makes sense to do so, such as textures written in compute shaders and read elsewhere
    
    // class DescriptorSetBuilder {
    //     private:
    //     const VkDevice device;
    //     const VkShaderStageFlagBits stages;
    //     DescriptorAllocator allocator;
    //     std::vector<DescriptorType> descriptor_types;
    //     std::vector<void*> handles;
    //     vk_types::CleanupProcedures& descriptor_set_lifetime;
    //     VkDescriptorSetLayout init_descriptor_set_layout();
    //     public:
    //     DescriptorSetBuilder(const VkDevice device, const VkShaderStageFlagBits stages, vk_types::CleanupProcedures& descriptor_set_lifetime);
    //     void add_storage_image(const VkImageView image_view);
    //     void add_combined_image_sampler(const VkImageView image_view, const VkSampler sampler);
    //     void add_sampled_image(const VkImageView image_view);
    //     void add_uniform_buffer(const VkBuffer buffer);
    //     void add_storage_buffer(const VkBuffer buffer);
    //     DescriptorSetBundle build();
    // };

    // class DescriptorPoolManager {
    //     private:
    //     DescriptorSetBundle descriptor_set;
    // };
}  // namespace vk_descriptors

#endif