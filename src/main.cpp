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
    //GLFWwindow* window = glfwCreateWindow(1600, 1200, "Galaxy Jar", nullptr, nullptr);
    GLFWwindow* window = glfwCreateWindow(3000, 2000, "Galaxy Jar", nullptr, nullptr);

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
    vk_layer::BufferedUniform<vk_layer::SkyboxUniforms> skybox_uniforms = vk_layer::build_skybox_uniforms(context, context.buffer_count, context.cleanup_procedures);
    vk_image::HostImage skybox_image = vk_image::load_rgba_cubemap("../../../assets/skybox/space-skybox.png");
    uint32_t skybox_texture_index = vk_layer::upload_skybox(context, skybox_image, context.cleanup_procedures);
    vk_layer::Drawable skybox_cube;
    {
        geometry::AxisAlignedBasis basis = {
            .x = geometry::Direction::Right,
            .y = geometry::Direction::Up,
            .z = geometry::Direction::Forward
        };
        geometry::HostModel cube_model = geometry::load_obj_model("cube.obj", "../../../assets/cube/", basis);
        skybox_cube = vk_layer::make_drawable(context, cube_model);
    }

    /// Setup for main geometry draw
    // Load other resources. We don't need the HostModel past upload so we can just dump it on the ground after upload since it can be pretty hefty.
    vk_layer::Drawable dummy_drawable;
    {
        geometry::AxisAlignedBasis blender_basis = {
            .x = geometry::Direction::Right,
            .y = geometry::Direction::Back,
            .z = geometry::Direction::Up
        };

        geometry::AxisAlignedBasis unmodified_basis = {
            .x = geometry::Direction::Right,
            .y = geometry::Direction::Up,
            .z = geometry::Direction::Forward
        };
        //geometry::HostModel dummy_model = geometry::load_obj_model("exterior.obj", "../../../assets/bistro/", unmodified_basis);
        geometry::HostModel dummy_model = geometry::load_obj_model("WATER_WORLD.obj", "../../../assets/planetoid/", unmodified_basis);
        dummy_drawable = vk_layer::make_drawable(context, dummy_model);
    }

    std::vector<vk_layer::Drawable> main_drawables = {dummy_drawable};
    std::vector<vk_layer::Drawable> masking_jars = {dummy_drawable};

    std::vector<VkDescriptorSetLayout> skybox_descriptor_set_layouts = { 
        skybox_uniforms.get_layout(),
        context.mega_descriptor_set.bundle.layout
    };

    vk_layer::BufferedUniform<vk_layer::GlobalUniforms> global_uniforms = vk_layer::build_global_uniforms(context, context.buffer_count, context.cleanup_procedures);

    std::vector<VkDescriptorSetLayout> grid_descriptor_set_layouts = {
        context.mega_descriptor_set.bundle.layout
    };

    std::vector<VkDescriptorSetLayout> graphics_descriptor_set_layouts = { 
        global_uniforms.get_layout(),
        context.mega_descriptor_set.bundle.layout,
        // TODO: All of the drawables currently have common layouts for resources, consider finding a cleaner way to model this
        main_drawables[0].transform.get_layout(),
    };

    std::vector<VkDescriptorSetLayout> jar_cutaway_mask_descriptor_set_layouts = {
        global_uniforms.get_layout(),
        masking_jars[0].transform.get_layout(),
    };

    std::vector<VkDescriptorSetLayout> compose_descriptor_set_layouts = {
        context.mega_descriptor_set.bundle.layout
    };

    vk_layer::DescriptorSetLayouts descriptor_layouts = {
        grid_descriptor_set_layouts,
        graphics_descriptor_set_layouts,
        skybox_descriptor_set_layouts,
        jar_cutaway_mask_descriptor_set_layouts,
        compose_descriptor_set_layouts
    };

    vk_layer::Pipelines pipelines = vk_layer::build_pipelines(context, descriptor_layouts, context.cleanup_procedures);

    vk_layer::RenderTargets render_target_indices = vk_layer::build_render_targets(context, context.cleanup_procedures);

    vk_layer::DrawState draw_state = {
        .buf_num = 0,
        .frame_num = 0,
        .frame_in_flight = 0,
        .main_dynamic_uniforms = global_uniforms,
        .skybox_dynamic_uniforms = skybox_uniforms
    };
    
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        draw_state = vk_layer::draw(context, pipelines, render_target_indices, main_drawables, masking_jars, skybox_cube, skybox_texture_index, draw_state);
    }

    vk_layer::cleanup(context, context.cleanup_procedures);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}