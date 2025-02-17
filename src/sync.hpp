#ifndef VK_SYNC_H_
#define VK_SYNC_H_

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

#include "vk_types.hpp"
namespace sync {
    // Hamfisted 'all commands' pipeline barriers with image memory barriers
    void transition_image(const VkCommandBuffer cmd, const VkImage image, const VkImageSubresourceRange range, const VkImageLayout starting_layout, const VkImageLayout ending_layout);
    // Same as above but assumes color subresource
    void transition_image(const VkCommandBuffer cmd, const VkImage image, const VkImageLayout starting_layout, const VkImageLayout ending_layout);
}
#endif