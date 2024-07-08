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

    struct Queues {
        VkQueue graphics;
        VkQueue presentation;
    };

    struct Synchronization {
        VkSemaphore swapchain_semaphore;
        VkSemaphore render_semaphore;
        VkFence render_fence;
    };

    struct Resources {
        VkInstance instance;
        VkPhysicalDevice gpu;
        VkDevice device;
        VkSurfaceKHR surface;
        Swapchain swapchain;
        Queues queues;
        std::vector<Command> command;
        std::vector<Synchronization> synchronization;
        uint8_t buffer_count;
    };

    struct DrawState {
        bool not_first_frame;
        uint8_t buf_num;
        uint64_t frame_num;
    };

    Resources init(const std::vector<const char*>& required_device_extensions, const std::vector<const char*>& glfw_extensions, GLFWwindow* window);
    DrawState draw(const Resources& res, const DrawState& state);
    void cleanup(Resources& resources);
} // namespace vk_layer

#endif // VK_LAYER_H_