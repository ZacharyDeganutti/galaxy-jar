#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <stdio.h>

#include "vk_layer.hpp"

int main() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Galaxy Jar", nullptr, nullptr);

    // Query vulkan and glfw extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions;

    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    // Initialize vulkan
    vk_layer::Resources vulkan_resources = vk_layer::init(glfw_extension_count, glfw_extensions, window);
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    vk_layer::cleanup(vulkan_resources);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}