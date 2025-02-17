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
            const std::vector<VkDescriptorType> descriptor_types = { static_cast<VkDescriptorType>(vk_descriptors::DescriptorType::UniformBuffer) };
            auto descriptor_layout = vk_descriptors::init_descriptor_layout(vk_context.device, VK_SHADER_STAGE_ALL_GRAPHICS, descriptor_types, vk_context.cleanup_procedures);
            uniform.reserve(buffer_count);
            for (size_t index = 0; index < buffer_count; ++index) {
                vk_descriptors::DescriptorAllocator descriptor_allocator = {};
                auto ubo = vk_buffer::create_persistent_mapped_uniform_buffer<T>(vk_context);
                auto buffer_descriptors = vk_descriptors::init_buffer_descriptors(vk_context.device, ubo.buffer_resource.buffer, vk_descriptors::DescriptorType::UniformBuffer, descriptor_layout, descriptor_allocator, lifetime);
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
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec4 sun_direction;
    };

    struct SkyboxUniforms {
        glm::mat4 cam_rotation;
    };

    struct Texture {
        vk_types::AllocatedImage allocated_image;
        VkDescriptorSet descriptor;
        VkDescriptorSetLayout layout;
    };

    struct GridPassPushConstants {
        uint32_t grid_storage_index;
    };

    struct SpacePassPushConstants {
        uint32_t diffuse_texture_index;
        uint32_t specular_texture_index;
        uint32_t normal_texture_index;
    };

    struct SkyboxPassPushConstants {
        uint32_t skybox_texture_index;
    };

    struct ComposePassPushConstants {
        uint32_t grid_sampled_index;
        uint32_t grid_sampler_index;
        uint32_t space_index;
        uint32_t space_depth_index;
        uint32_t jar_mask_index;
        uint32_t jar_mask_depth_index;
        uint32_t compose_storage_index;
    };

    struct RenderTargets {
        uint32_t grid_storage_index;
        uint32_t grid_sampled_index;
        uint32_t grid_sampler_index;
        vk_types::AllocatedImage grid;
        uint32_t space_index;
        vk_types::AllocatedImage space;
        uint32_t space_depth_index;
        vk_types::AllocatedImage space_depth;
        uint32_t jar_mask_index;
        vk_types::AllocatedImage jar_mask;
        uint32_t jar_mask_depth_index;
        vk_types::AllocatedImage jar_mask_depth;
        uint32_t compose_storage_index;
        vk_types::AllocatedImage compose_storage;
    };

    struct DescriptorSetLayouts {
        std::vector<VkDescriptorSetLayout>& grid;
        std::vector<VkDescriptorSetLayout>& skybox;
        std::vector<VkDescriptorSetLayout>& jar_cutaway_mask;
        std::vector<VkDescriptorSetLayout>& graphics;
        std::vector<VkDescriptorSetLayout>& compose;
    };

    struct Pipelines {
        vk_types::Pipeline grid;
        vk_types::Pipeline skybox;
        vk_types::Pipeline jar_cutaway_mask;
        vk_types::Pipeline space;
        vk_types::Pipeline compose;
    };

    struct DrawState {
        uint8_t buf_num;
        uint64_t frame_num;
        uint64_t frame_in_flight;
        BufferedUniform<GlobalUniforms> main_dynamic_uniforms;
        BufferedUniform<SkyboxUniforms> skybox_dynamic_uniforms;
    };

    // Registers the skybox texture with the mega descriptor set as a combined sampler image, returns the descriptor index
    uint32_t upload_skybox(vk_types::Context& context, const vk_image::HostImage& skybox_image, vk_types::CleanupProcedures& lifetime);
    Pipelines build_pipelines(vk_types::Context& context, const DescriptorSetLayouts& descriptor_layouts, vk_types::CleanupProcedures& lifetime);
    BufferedUniform<GlobalUniforms> build_global_uniforms(vk_types::Context& context, const size_t buffer_count, vk_types::CleanupProcedures& lifetime);
    BufferedUniform<SkyboxUniforms> build_skybox_uniforms(vk_types::Context& context, const size_t buffer_count, vk_types::CleanupProcedures& lifetime);
    RenderTargets build_render_targets(vk_types::Context& context, vk_types::CleanupProcedures& lifetime);
    Drawable make_drawable(vk_types::Context& context, const geometry::HostModel& model_data);
    void immediate_submit(const vk_types::Context& res, std::function<void(VkCommandBuffer cmd)>&& function);

    DrawState draw( const vk_types::Context& res,
                    const Pipelines& pipelines, 
                    const RenderTargets& render_targets, 
                    const std::vector<Drawable>& drawables, 
                    const std::vector<Drawable>& masking_jars, 
                    const Drawable& skybox, 
                    const uint32_t skybox_texture_index, 
                    const DrawState& state);

    void cleanup(vk_types::Context& resources, vk_types::CleanupProcedures& cleanup_procedures);

    template <class T>
    VkPushConstantRange push_constant_range(VkShaderStageFlagBits stage) {
        VkPushConstantRange range = {};
        range.stageFlags = stage;
        range.offset = 0;
        range.size = static_cast<uint32_t>(sizeof(T));

        return range;
    }
    
} // namespace vk_layer

#endif // VK_LAYER_H_