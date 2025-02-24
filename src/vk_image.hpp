#ifndef VK_IMAGE_H_
#define VK_IMAGE_H_

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "vk_types.hpp"

namespace vk_image {
    enum class Representation {
        Flat,
        Cubemap
    };

    struct HostImage {
        uint32_t width;
        uint32_t height;
        std::vector<unsigned char> data;
        Representation representation;
    };

    struct HostImageRgba {
        HostImage image;
    };

    struct HostImageRg {
        HostImage image;
    };

    HostImage load_rg_image(const std::string& filepath);
    HostImage load_rgba_image(const std::string& filepath);
    HostImage load_rgba_cubemap(const std::string& filepath);
    // Takes a gltf standard encoded metallic workflow image and flattens it to a RG channel image 
    HostImage load_gltf_specular_image_as_rg(const std::string& filename);
    vk_types::AllocatedImage upload_image(vk_types::Context& context, const HostImage& image, VkFormat format, VkImageLayout desired_layout);
    vk_types::AllocatedImage upload_image(const vk_types::Context& context, const HostImage& image, VkFormat format, VkImageLayout desired_layout, vk_types::CleanupProcedures& lifetime);
    vk_types::AllocatedImage upload_image_mipmapped(vk_types::Context& context, const HostImage& image, VkFormat format, VkImageLayout desired_layout);
    vk_types::AllocatedImage upload_image_mipmapped(const vk_types::Context& context, const HostImage& image, VkFormat format, VkImageLayout desired_layout, vk_types::CleanupProcedures& lifetime);

    VkSampler init_linear_sampler(vk_types::Context& context);
    VkSampler init_linear_sampler(const vk_types::Context& context, vk_types::CleanupProcedures& lifetime);
    
    // Blits a source image to a destination image. Operates on a per-mipmap level basis
    void blit_image_to_image(const VkCommandBuffer cmd, const VkImage source, const VkImage destination, const VkExtent2D source_extent, const VkExtent2D destination_extent, const uint32_t source_miplevel, const uint32_t destination_miplevel);
    // Blits a source image to a destination image. Only blits the base mipmap level.
    void blit_image_to_image_no_mipmap(const VkCommandBuffer cmd, const VkImage source, const VkImage destination, VkExtent2D source_extent, VkExtent2D destination_extent);

    VkImageSubresourceRange make_subresource_range(const VkImageAspectFlags aspect_mask);
    VkImageSubresourceRange make_baselevel_subresource_range(const VkImageAspectFlags aspect_mask);
    VkImageSubresourceRange make_miplevels_subresource_range(const VkImageAspectFlags aspect_mask);
    
    // Rough around the edges general functions, prefer the higher level ones when possible.
    vk_types::AllocatedImage init_allocated_image(const VkDevice device, const VmaAllocator allocator, const Representation representation, const VkFormat format, const VkImageUsageFlags usage_flags, const uint32_t miplevels, const VkExtent2D extent, vk_types::CleanupProcedures& cleanup_procedures);
    VkImageView init_image_view(const VkDevice device, const VkImage image, const Representation representation, const VkFormat format, const uint32_t miplevels, vk_types::CleanupProcedures& cleanup_procedures);
}
#endif // VK_IMAGE_H_