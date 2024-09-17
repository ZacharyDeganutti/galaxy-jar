#include "vk_image.hpp"

namespace vk_image {
    VkImageSubresourceRange make_subresource_range(const VkImageAspectFlags aspect_mask) {
        VkImageSubresourceRange subresource_range = {};
        subresource_range.aspectMask = aspect_mask; // COLOR or DEPTH
        subresource_range.baseArrayLayer = 0;
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = VK_REMAINING_MIP_LEVELS;
        subresource_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

        return subresource_range;
    }

    void transition_image(const VkCommandBuffer cmd, const VkImage image, const VkImageLayout starting_layout, const VkImageLayout ending_layout) {
        VkImageSubresourceRange subresource_range = make_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

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

    void blit_image_to_image(const VkCommandBuffer cmd, const VkImage source, const VkImage destination, VkExtent2D source_extent, VkExtent2D destination_extent) {
        VkImageBlit2 blit_region = {};
        blit_region.pNext = nullptr;
        blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

        blit_region.srcOffsets[1].x = source_extent.width;
        blit_region.srcOffsets[1].y = source_extent.height;
        blit_region.srcOffsets[1].z = 1;

        blit_region.dstOffsets[1].x = destination_extent.width;
        blit_region.dstOffsets[1].y = destination_extent.height;
        blit_region.dstOffsets[1].z = 1;

        blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit_region.srcSubresource.baseArrayLayer = 0;
        blit_region.srcSubresource.layerCount = 1;
        blit_region.srcSubresource.mipLevel = 0;

        blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit_region.dstSubresource.baseArrayLayer = 0;
        blit_region.dstSubresource.layerCount = 1;
        blit_region.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blit_info = {};
        blit_info.pNext = nullptr;
        blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;

        blit_info.dstImage = destination;
        blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blit_info.srcImage = source;
        blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blit_info.filter = VK_FILTER_LINEAR;
        blit_info.regionCount = 1;
        blit_info.pRegions = &blit_region;

        vkCmdBlitImage2(cmd, &blit_info);
    }
}