#include <vulkan/vulkan.h>

namespace vk_image {
    void transition_image(const VkCommandBuffer cmd, const VkImage image, const VkImageLayout starting_layout, const VkImageLayout ending_layout);
    void blit_image_to_image(const VkCommandBuffer cmd, const VkImage source, const VkImage destination, VkExtent2D source_extent, VkExtent2D destination_extent);
    VkImageSubresourceRange make_subresource_range(const VkImageAspectFlags aspect_mask);
}