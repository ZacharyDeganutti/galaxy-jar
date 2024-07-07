#ifndef VK_LAYER_H_
#define VK_LAYER_H_

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

namespace vk_layer
{
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

    struct Resources {
        VkInstance instance;
        VkPhysicalDevice gpu;
        VkDevice device;
        VkSurfaceKHR surface;
        Swapchain swapchain;
        std::vector<Command> command;
    };

    Resources init(const std::vector<const char*>& required_device_extensions, const std::vector<const char*>& glfw_extensions, GLFWwindow* window);
    void cleanup(Resources& resources);
} // namespace vk_layer

#endif // VK_LAYER_H_