#ifndef VK_TYPES_H_
#define VK_TYPES_H_

#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <span>
#include <deque>
#include <functional>

namespace vk_types{
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
        GpuVertexAttribute position_buffers;
        GpuVertexAttribute normal_buffers;
        GpuVertexAttribute texture_coordinate_buffers;
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
        VkDescriptorSet descriptors;
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
        Pipeline compute_pipeline;
        Pipeline graphics_pipeline;
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