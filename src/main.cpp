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
    
    /// Setup for skybox background draw
    vk_layer::SkyboxUniforms skybox_uniforms = vk_layer::build_skybox_uniforms(context, context.buffer_count, context.cleanup_procedures);
    vk_image::HostImageRgba skybox_image = vk_image::load_rgba_cubemap("../../../assets/skybox/daylight.png");
    vk_layer::SkyboxTexture skybox_texture = vk_layer::upload_skybox(context, skybox_image, context.cleanup_procedures);
    geometry::GpuModel skybox_cube;
    {
        geometry::HostModel cube_model = geometry::load_obj_model("cube.obj", "../../../assets/cube/");
        skybox_cube = geometry::upload_model(context, cube_model);
    }
    std::vector<VkDescriptorSetLayout> skybox_descriptor_set_layouts = { 
        skybox_uniforms.cam_rotation.get_layout(),
        skybox_texture.layout
    };

    /// Setup for main geometry draw
    // Load other resources. We don't need the HostModel past upload so we can just dump it on the ground after upload since it can be pretty hefty.
    geometry::GpuModel dummy_gpu_model;
    {
        geometry::HostModel dummy_model = geometry::load_obj_model("house.obj", "../../../assets/rungholt/");
        dummy_gpu_model = geometry::upload_model(context, dummy_model);
    }

    std::vector<geometry::GpuModel> main_drawables = {dummy_gpu_model};
    vk_layer::GlobalUniforms global_uniforms = vk_layer::build_global_uniforms(context, context.buffer_count, context.cleanup_procedures);
    std::vector<VkDescriptorSetLayout> graphics_descriptor_set_layouts = { 
        global_uniforms.modelview.get_layout(),
        global_uniforms.brightness.get_layout(),
        main_drawables[0].texture_layout, // TODO: All of the drawables have a shared layout, consider finding a cleaner way to model this
    };

    vk_layer::Pipelines pipelines = vk_layer::build_pipelines(context, graphics_descriptor_set_layouts, skybox_descriptor_set_layouts, context.cleanup_procedures);

    vk_layer::DrawState draw_state = {
        .buf_num = 0,
        .frame_num = 0,
        .frame_in_flight = 0,
        .main_dynamic_uniforms = global_uniforms,
        .skybox_dynamic_uniforms = skybox_uniforms
    };
    
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        draw_state = vk_layer::draw(context, pipelines, main_drawables, skybox_cube, skybox_texture, draw_state);
    }

    vk_layer::cleanup(context, context.cleanup_procedures);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}