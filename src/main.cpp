#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <stdio.h>

#include "vk_types.hpp"
#include "vk_init.hpp"
#include "vk_layer.hpp"
#include "vk_buffer.hpp"
#include "geometry.hpp"

int main() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1600, 1200, "Galaxy Jar", nullptr, nullptr);

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
    vk_types::Context context = vk_init::init(required_device_extensions, glfw_extensions, window);
    
    // Load other resources. We don't need the HostModel past upload so we can just dump it on the ground after upload since it can be pretty hefty.
    geometry::GpuModel dummy_gpu_model;
    {
        geometry::HostModel dummy_model = geometry::load_obj_model("cube.obj", "../../../assets/cube/");
        dummy_gpu_model = geometry::upload_model(context, dummy_model);
    }

    std::vector<geometry::GpuModel> drawables = {dummy_gpu_model};

    vk_layer::GlobalUniforms global_uniforms = vk_layer::build_global_uniforms(context, context.buffer_count, context.cleanup_procedures);
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts = { 
        global_uniforms.modelview.get_layout(),
        global_uniforms.brightness.get_layout(),
        drawables[0].texture_layout, // TODO: All of the drawables have a shared layout, consider finding a cleaner way to model this
    };

    vk_layer::Pipelines pipelines = vk_layer::build_pipelines(context, descriptor_set_layouts, context.cleanup_procedures);

    vk_layer::DrawState draw_state = {
        .buf_num = 0,
        .frame_num = 0,
        .frame_in_flight = 0,
        .buffers = global_uniforms,
    };
    
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        draw_state = vk_layer::draw(context, pipelines, drawables, draw_state);
    }

    vk_layer::cleanup(context, context.cleanup_procedures);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}