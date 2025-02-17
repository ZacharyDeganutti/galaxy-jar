#include "sync.hpp"
#include "vk_image.hpp"
namespace sync {
    void transition_image(const VkCommandBuffer cmd, const VkImage image, const VkImageSubresourceRange range, const VkImageLayout starting_layout, const VkImageLayout ending_layout) {
        VkImageSubresourceRange subresource_range = range;

        // Use an image memory barrier on the subresource
        VkImageMemoryBarrier2 image_barrier = {};
        image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        image_barrier.pNext = nullptr;
        
        image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        image_barrier.oldLayout = starting_layout;
        image_barrier.newLayout = ending_layout;

        image_barrier.subresourceRange = subresource_range;
        image_barrier.image = image;

        // Then specify the dependency info with the image barrier wired in
        VkDependencyInfo dependency_info = {};
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.pNext = nullptr;
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &image_barrier;

        // And set the pipeline barrier in place
        vkCmdPipelineBarrier2(cmd, &dependency_info);
    }

    void transition_image(const VkCommandBuffer cmd, const VkImage image, const VkImageLayout starting_layout, const VkImageLayout ending_layout) {
        VkImageSubresourceRange subresource_range = vk_image::make_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
        transition_image(cmd, image, subresource_range, starting_layout, ending_layout);
    }
}