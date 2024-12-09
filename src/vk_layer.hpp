#ifndef VK_LAYER_H_
#define VK_LAYER_H_

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <span>
#include <deque>
#include <functional>

#include "vk_types.hpp"
#include "vk_buffer.hpp"
#include "vk_descriptors.hpp"
#include "geometry.hpp"
#include "glmvk.hpp"

namespace vk_layer
{
    template <class T>
    struct BufferedUniform {
        private:
        T value;
        std::vector<vk_types::UniformInfo<T>> uniform;
        public:
        BufferedUniform() {}
        BufferedUniform(vk_types::Context& vk_context, const T initial_value, const size_t buffer_count, vk_types::CleanupProcedures& lifetime) : value(initial_value), uniform(std::vector<vk_types::UniformInfo<T>>()) {
            const std::vector<VkDescriptorType> descriptor_types = { static_cast<VkDescriptorType>(vk_descriptors::DescriptorType::Uniform) };
            auto descriptor_layout = vk_descriptors::init_descriptor_layout(vk_context.device, VK_SHADER_STAGE_ALL_GRAPHICS, descriptor_types, vk_context.cleanup_procedures);
            uniform.reserve(buffer_count);
            for (size_t index = 0; index < buffer_count; ++index) {
                vk_descriptors::DescriptorAllocator descriptor_allocator = {};
                auto ubo = vk_buffer::create_persistent_mapped_uniform_buffer<T>(vk_context);
                auto buffer_descriptors = vk_descriptors::init_buffer_descriptors(vk_context.device, ubo.buffer_resource.buffer, vk_descriptors::DescriptorType::Uniform, descriptor_layout, descriptor_allocator, lifetime);
                vk_types::UniformInfo<T> default_uniform = {
                    ubo,
                    descriptor_layout,
                    buffer_descriptors,
                };
                default_uniform.buffer.update(value);
                uniform.push_back(default_uniform);
            }
        }
    
        // Set the canonical value of the uniform. Does not update the value on the GPU.
        void set(T new_value) {
            value = new_value;
        }

        // Get the canonical value of the uniform. Does not read the value from the GPU.
        T get() const {
            return value;
        }

        // Push the current value of the uniform to the GPU. Updates the GPU buffer specified by index. Assumes index is in bounds.
        void push(size_t index) {
            uniform[index].buffer.update(value);
        }

        // Retrieve the descriptor set layout for the uniform buffer.
        VkDescriptorSetLayout get_layout() const {
            // All uniforms have the same layout, so we can just return the first one.
            return uniform[0].descriptor_set_layout;
        }

        // Retrieve the descriptor set for the uniform buffer at the specified index.
        VkDescriptorSet get_descriptor_set(size_t index) const {
            return uniform[index].descriptor_set;
        }
    };

    struct Drawable {
        geometry::GpuModel gpu_model;
        BufferedUniform<glm::mat4> transform;
    };

    struct GlobalUniforms {
        BufferedUniform<glm::mat4> view;
        BufferedUniform<glm::mat4> projection;
    };

    struct SkyboxUniforms {
        BufferedUniform<glm::mat4> cam_rotation;
    };

    struct SkyboxTexture {
        vk_types::AllocatedImage allocated_image;
        VkDescriptorSet descriptor;
        VkDescriptorSetLayout layout;
    };

    struct Pipelines {
        vk_types::Pipeline graphics;
        vk_types::Pipeline compute;
        vk_types::Pipeline skybox;
    };

    struct DrawState {
        uint8_t buf_num;
        uint64_t frame_num;
        uint64_t frame_in_flight;
        GlobalUniforms main_dynamic_uniforms;
        SkyboxUniforms skybox_dynamic_uniforms;
    };

    SkyboxTexture upload_skybox(vk_types::Context& context, const vk_image::HostImageRgba& skybox_image, vk_types::CleanupProcedures& lifetime);
    Pipelines build_pipelines(vk_types::Context& context, const std::vector<VkDescriptorSetLayout>& graphics_descriptor_layouts, const std::vector<VkDescriptorSetLayout>& skybox_descriptor_layouts, vk_types::CleanupProcedures& lifetime);
    GlobalUniforms build_global_uniforms(vk_types::Context& context, const size_t buffer_count, vk_types::CleanupProcedures& lifetime);
    SkyboxUniforms build_skybox_uniforms(vk_types::Context& context, const size_t buffer_count, vk_types::CleanupProcedures& lifetime);
    Drawable make_drawable(vk_types::Context& context, const geometry::HostModel& model_data);
    void immediate_submit(const vk_types::Context& res, std::function<void(VkCommandBuffer cmd)>&& function);
    DrawState draw(const vk_types::Context& res, const Pipelines& pipelines, const std::vector<Drawable>& drawables, const Drawable& skybox, const SkyboxTexture& skybox_texture, const DrawState& state);
    void cleanup(vk_types::Context& resources, vk_types::CleanupProcedures& cleanup_procedures);
} // namespace vk_layer

#endif // VK_LAYER_H_