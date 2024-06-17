#ifndef VK_LAYER_H_
#define VK_LAYER_H_

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace vk_layer
{
    typedef struct Resources {
        VkInstance instance;
        VkPhysicalDevice gpu;
        VkDevice device;
        VkSurfaceKHR surface;
    } Resources;

    Resources init(int extension_count, const char** extension_names, GLFWwindow* window);
    void cleanup(Resources& resources);
} // namespace vk_layer

#endif // VK_LAYER_H_