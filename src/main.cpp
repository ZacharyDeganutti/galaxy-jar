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
    vk_types::Context context = vk_init::init(required_device_extensions, glfw_extensions, window);
    
    // Load other resources
    geometry::Model dummy_model = geometry::load_obj_model("sponza.obj", "../../../assets/sponza/");
    std::vector<vk_types::GpuMeshBuffers> mesh_resources = vk_buffer::create_mesh_buffers(context, dummy_model);
    vk_layer::RenderResources render_resources = vk_layer::build_render_resources(context, context.cleanup_procedures);

    vk_layer::DrawState draw_state = {
        .not_first_frame = false,
        .buf_num = 0,
        .frame_num = 0,
        .buffers = render_resources.buffers,
    };
    
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        draw_state = vk_layer::draw(context, render_resources.pipelines, mesh_resources, draw_state);
    }

    vk_layer::cleanup(context, context.cleanup_procedures);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}