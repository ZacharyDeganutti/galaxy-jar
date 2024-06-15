#ifndef VK_LAYER_H_
#define VK_LAYER_H_

#include <vulkan/vulkan.h>

namespace vk_layer
{
    VkInstance init_instance(int extension_count, const char** extension_names);
    VkPhysicalDevice init_physical_device(VkInstance instance);
    VkDevice init_logical_device(VkPhysicalDevice gpu);

    void cleanup(VkInstance instance);
} // namespace vk_layer

#endif // VK_LAYER_H_