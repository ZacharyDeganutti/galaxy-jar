#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <stdio.h>

#include "vk_layer.hpp"

int main() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Galaxy Jar", nullptr, nullptr);

    // Query glfw extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions_data;
    glfw_extensions_data = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    std::vector<const char*> glfw_extensions(glfw_extensions_data, glfw_extensions_data+glfw_extension_count);
    
    // Dictate which extensions we need
    std::vector<const char*> required_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Initialize vulkan
    vk_layer::Resources vulkan_resources = vk_layer::init(required_device_extensions, glfw_extensions, window);
    vk_layer::DrawState draw_state = {
        .not_first_frame = false,
        .buf_num = 0,
        .frame_num = 0
    };
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        draw_state = vk_layer::draw(vulkan_resources, draw_state);
    }

    vk_layer::cleanup(vulkan_resources);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}