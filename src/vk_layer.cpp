#include "vk_layer.hpp"
#include <vulkan/vk_enum_string_helper.h>

VkInstance vk_layer::init_instance(int extension_count, const char** extension_names) {
    // Create our instance
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Galaxy Jar";
    app_info.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
    app_info.apiVersion= VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = extension_count;
    instance_create_info.ppEnabledExtensionNames = extension_names;
    instance_create_info.enabledLayerCount = 0;

    VkInstance instance{};
    VkResult instance_creation_result = vkCreateInstance(&instance_create_info, nullptr, &instance);
    if (instance_creation_result != VK_SUCCESS) {
        printf("VkInstance creation failed: %s", string_VkResult(instance_creation_result));
        exit(EXIT_FAILURE);
    }
    return instance;
}

void vk_layer::cleanup(VkInstance instance) {
    vkDestroyInstance(instance, nullptr);
}