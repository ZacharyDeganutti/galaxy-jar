#ifndef VK_TYPES_H_
#define VK_TYPES_H_

#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <span>
#include <deque>
#include <functional>
#include <cstring>

// definitions can be found in vk_descriptors.cpp but the full declaration is needed here to realize this inside the Context type
namespace vk_descriptors {
    
    struct DescriptorSetBundle {
        VkDescriptorSet set;
        VkDescriptorSetLayout layout;
    };

    struct MegaDescriptorSet {
        private:
        uint32_t next_combined_image_sampler_index;
        uint32_t next_sampled_image_index;
        uint32_t next_sampler_index;
        uint32_t next_storage_image_index;
        uint32_t next_storage_buffer_index;
        public:
        DescriptorSetBundle bundle;

        // These register new descriptors into the descriptor set buffers. Modify the corresponding next_index field of the MegaDescriptorSet and returns the most recently assigned index for the requested type
        uint32_t register_combined_image_sampler_descriptor(const VkDevice device, const VkImageView image_view, const VkSampler sampler);
        uint32_t register_sampled_image_descriptor(const VkDevice device, const VkImageView image_view);
        uint32_t register_sampler_descriptor(const VkDevice device, const VkSampler sampler);
        uint32_t register_storage_image_descriptor(const VkDevice device, const VkImageView image_view);
        uint32_t register_storage_buffer_descriptor(const VkDevice device, const VkBuffer buffer);
    };
}

namespace vk_types {
    struct AllocatedImage {
        VkImage image;
        VkImageView image_view;
        VmaAllocation allocation;
        VkExtent2D image_extent;
        VkFormat image_format;
    };

    struct AllocatedBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info;
    };

    struct GpuVertexAttribute {
        AllocatedBuffer vertex_buffer;
        VkDeviceAddress vertex_buffer_address;
    };

    struct GpuMeshBuffers {
        AllocatedBuffer index_buffer;
        GpuVertexAttribute position_buffer;
        GpuVertexAttribute normal_buffer;
        GpuVertexAttribute texture_coordinate_buffer;
        uint32_t index_count;
    };

    template <class T>
    struct PersistentUniformBuffer {
        AllocatedBuffer buffer_resource;
        const T* buffer_view;

        // Rebuilds the PersistentUniformBuffer with the new value.
        PersistentUniformBuffer<T> update(const T& new_value) const {
            PersistentUniformBuffer<T> rebuilt_buffer = {
                buffer_resource,
                buffer_view
            };

            std::memcpy((void*)(rebuilt_buffer.buffer_view), &new_value, sizeof(T));
            return rebuilt_buffer;
        };
    };

    template <class T>
    struct UniformInfo {
        vk_types::PersistentUniformBuffer<T> buffer;
        VkDescriptorSetLayout descriptor_set_layout;
        VkDescriptorSet descriptor_set;
    };

    struct CleanupProcedures {
        private:
        std::deque<std::function<void()>> procedure_stack;

        public:
        void add(std::function<void()>&& cleanup_procedure) {
            procedure_stack.push_back(cleanup_procedure);
        }

        void cleanup() {
            while (!procedure_stack.empty()) {
                auto procedure = procedure_stack.back();
                procedure_stack.pop_back();
                procedure();
            }
        }
    };

    struct Swapchain {
        VkSwapchainKHR handle;
        VkFormat format;
        VkExtent2D extent;
        std::vector<VkImage> images;
        std::vector<VkImageView> views;
    };

    struct Command {
        VkCommandPool pool;
        VkCommandBuffer buffer_primary;
    };

    struct Queues {
        VkQueue graphics;
        VkQueue presentation;
    };

    struct Pipeline {
        VkPipeline handle;
        VkPipelineLayout layout;
        VkPipelineBindPoint bind_point;
    };

    struct Synchronization {
        VkSemaphore swapchain_semaphore;
        VkSemaphore render_semaphore;
        VkFence render_fence;
    };

    struct Context {
        CleanupProcedures cleanup_procedures;
        VkInstance instance;
        VkPhysicalDevice gpu;
        VkDevice device;
        VkSurfaceKHR surface;
        Swapchain swapchain;
        Queues queues;
        std::vector<Command> command;
        Command command_immediate;
        std::vector<Synchronization> synchronization;
        VkFence fence_immediate;
        VmaAllocator allocator;
        AllocatedImage draw_target;
        AllocatedImage depth_buffer;
        AllocatedImage jar_cutaway_mask;
        AllocatedImage jar_cutaway_mask_depth;
        vk_descriptors::MegaDescriptorSet mega_descriptor_set;
        uint8_t buffer_count;
    };

    struct DescriptorAllocator {
        struct PoolSizeRatio{
            VkDescriptorType type;
            float ratio;
        };

        VkDescriptorPool pool;

        void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> pool_ratios) {
            std::vector<VkDescriptorPoolSize> pool_sizes;
            for (PoolSizeRatio ratio : pool_ratios) {
                pool_sizes.push_back(VkDescriptorPoolSize{
                    .type = ratio.type,
                    .descriptorCount = uint32_t(ratio.ratio * maxSets)
                });
            }

            VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool_info.flags = 0;
            pool_info.maxSets = maxSets;
            pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
            pool_info.pPoolSizes = pool_sizes.data();

            vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
        }
        void clear_descriptors(VkDevice device);
        void destroy_pool(VkDevice device);

        VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
    };
}

#endif