#include "vk_buffer.hpp"
#include "vk_image.hpp"
#include "vk_layer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace vk_image {

    HostImageRgba load_rgba_image(const std::string& filename) {
        
        int width = 0;
        int height = 0; 
        int channels = 0;

        const int FORCE_CHANNELS = 4;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* image_data = stbi_load(filename.c_str(), &width, &height, &channels, FORCE_CHANNELS);

        if (!image_data) {
            printf("Unable to load image %s\n", filename.c_str());
            exit(EXIT_FAILURE);
        }

        std::vector<unsigned char> image_data_vec(image_data, image_data + (width * height * FORCE_CHANNELS));

        stbi_image_free(image_data);

        return HostImageRgba{
            HostImage { static_cast<uint32_t>(width), static_cast<uint32_t>(height), image_data_vec }
        };
    }

    vk_types::AllocatedImage upload_rgba_image(vk_types::Context& context, const HostImageRgba& image, VkImageLayout desired_layout) {
        return upload_rgba_image(context, image, desired_layout, context.cleanup_procedures);
    }

    vk_types::AllocatedImage upload_rgba_image(const vk_types::Context& context, const HostImageRgba& image, VkImageLayout desired_layout, vk_types::CleanupProcedures& lifetime) {
        const VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;
        const VkImageUsageFlags image_flags =  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        const VkExtent2D extent = { image.image.width, image.image.height };
        vk_types::AllocatedImage allocated_image = init_allocated_image(context.device, context.allocator, image_format, image_flags, extent, lifetime);
        
        // Create a temporary staging buffer which can be used to transfer from CPU memory to GPU memory
        vk_types::CleanupProcedures staging_buffer_lifetime = {};
        vk_types::AllocatedBuffer staging = vk_buffer::create_buffer(
            context.allocator,
            image.image.data.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VMA_MEMORY_USAGE_CPU_ONLY,
            staging_buffer_lifetime);

        void* data = nullptr;
        if (vmaMapMemory(context.allocator, staging.allocation, &data) != VK_SUCCESS) {
            printf("Unable to map staging buffer during vertex attribute upload\n");
            exit(EXIT_FAILURE);
        }

        // Fill staging buffer
        memcpy(reinterpret_cast<unsigned char*>(data), image.image.data.data(), image.image.data.size());

        vk_layer::immediate_submit(context, [&](VkCommandBuffer cmd) {
            transition_image(cmd, allocated_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;

            region.imageOffset = {0, 0, 0};
            region.imageExtent = {
                extent.width,
                extent.height,
                1
            };

            vkCmdCopyBufferToImage(cmd, staging.buffer, allocated_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            // Transition image to requested layout after upload is complete
            transition_image(cmd, allocated_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
        vmaUnmapMemory(context.allocator, staging.allocation);
        staging_buffer_lifetime.cleanup();

        return allocated_image;
    }

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

    VkSampler init_linear_sampler(vk_types::Context& context) {
        return init_linear_sampler(context, context.cleanup_procedures);
    }

    VkSampler init_linear_sampler(const vk_types::Context& context, vk_types::CleanupProcedures& lifetime) {
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = 16.0f;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = VK_LOD_CLAMP_NONE;

        VkSampler sampler;
        vkCreateSampler(context.device, &sampler_info, nullptr, &sampler);

        lifetime.add([=]() {
            vkDestroySampler(context.device, sampler, nullptr);
        });

        return sampler;
    }

    VkImageView init_image_view(const VkDevice device, const VkImage image, const VkFormat format, vk_types::CleanupProcedures& cleanup_procedures) {
        VkImageView image_view = {};

        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = image;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = (format >= VK_FORMAT_D16_UNORM) && (format <= VK_FORMAT_D32_SFLOAT_S8_UINT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &image_view_create_info, nullptr, &image_view) != VK_SUCCESS) {
            printf("Unable to create image view");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, image_view]() {
            vkDestroyImageView(device, image_view, nullptr);
        });

        return image_view;
    }

    vk_types::AllocatedImage init_allocated_image(const VkDevice device, const VmaAllocator allocator, const VkFormat format, const VkImageUsageFlags usage_flags, const VkExtent2D extent, vk_types::CleanupProcedures& cleanup_procedures) {
        // Setup image specification
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext = nullptr;

        image_info.imageType = VK_IMAGE_TYPE_2D;

        image_info.format = format;
        image_info.extent = {extent.width, extent.height, 1};

        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;

        image_info.samples = VK_SAMPLE_COUNT_1_BIT;

        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = usage_flags;

        // Setup allocation specification
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkImage image = {};
        VmaAllocation allocation = {};
        vmaCreateImage(allocator, &image_info, &alloc_info, &image, &allocation, nullptr);

        // Specify cleanup for the VkImage here because the image is created prior to its VkImageView and we want cleanup in the right order
        // since init_image_view handles its own cleanup
        cleanup_procedures.add([allocator, image, allocation]() {
            vmaDestroyImage(allocator, image, allocation);
        });

        VkImageView view = init_image_view(device, image, format, cleanup_procedures);
        vk_types::AllocatedImage allocated_image = {};
        allocated_image.allocation = allocation;
        allocated_image.image = image;
        allocated_image.image_extent = extent;
        allocated_image.image_format = format;
        allocated_image.image_view = view;

        return allocated_image;
    }
}