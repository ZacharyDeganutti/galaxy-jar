#ifndef VK_PIPELINE_H_
#define VK_PIPELINE_H_

#include "vk_types.hpp"
#include "vk_buffer.hpp"

namespace vk_pipeline {
    // Creates a shader stage info structure for the given shader stage and module.
    VkPipelineShaderStageCreateInfo make_shader_stage_info(VkShaderStageFlagBits stage_flags, VkShaderModule module);

    // Creates a graphics pipeline with the specified vertex and fragment shaders
    vk_types::Pipeline init_graphics_pipeline(const VkDevice device, const VkPipelineLayout pipeline_layout, const VkFormat& target_format, const VkFormat& depth_format, const VkShaderModule vert_shader_module, const VkShaderModule frag_shader_module, const VkDescriptorSet descriptor_set, vk_types::CleanupProcedures& cleanup_procedures);

    // Creates a compute pipeline with the specified compute shader
    vk_types::Pipeline init_compute_pipeline(const VkDevice device, const VkPipelineLayout compute_pipeline_layout, const VkShaderModule shader_module, const VkDescriptorSet descriptor_set, vk_types::CleanupProcedures& cleanup_procedures);

    // Creates a pipeline layout with the specified descriptor set layouts
    VkPipelineLayout init_pipeline_layout(const VkDevice device, const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts, vk_types::CleanupProcedures& cleanup_procedures);

    // Creates a shader module from the given SPIR-V file.
    VkShaderModule init_shader_module(const VkDevice device, const char *file_path, vk_types::CleanupProcedures& cleanup_procedures);

}

#endif // VK_PIPELINE_H_
