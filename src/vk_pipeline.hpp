#ifndef VK_PIPELINE_H_
#define VK_PIPELINE_H_

#include "vk_types.hpp"
#include "vk_buffer.hpp"
#include <functional>

namespace vk_pipeline {
    // Creates a shader stage info structure for the given shader stage and module.
    VkPipelineShaderStageCreateInfo make_shader_stage_info(VkShaderStageFlagBits stage_flags, VkShaderModule module);

    // Creates a compute pipeline with the specified compute shader
    vk_types::Pipeline init_compute_pipeline(const VkDevice device, const VkPipelineLayout compute_pipeline_layout, const VkShaderModule shader_module, vk_types::CleanupProcedures& cleanup_procedures);

    // Creates a pipeline layout with the specified descriptor set layouts
    VkPipelineLayout init_pipeline_layout(const VkDevice device, const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts, const VkPushConstantRange pc_range, vk_types::CleanupProcedures& cleanup_procedures);
    VkPipelineLayout init_pipeline_layout(const VkDevice device, const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts, vk_types::CleanupProcedures& cleanup_procedures);
    // Creates a shader module from the given SPIR-V file.
    VkShaderModule init_shader_module(const VkDevice device, const char *file_path, vk_types::CleanupProcedures& cleanup_procedures);

    class GraphicsPipelineBuilder {
        private:
        VkDevice device;
        vk_types::CleanupProcedures* cleanup_procedures;
        VkPipelineLayout pipeline_layout;
        VkShaderModule vertex_shader;
        VkShaderModule fragment_shader;
        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
        std::vector<VkDynamicState> default_dynamic_state;
        VkFormat default_target_format;
        VkPipelineColorBlendAttachmentState default_blend_attachment_state;

        VkPipelineViewportStateCreateInfo viewport_info;
        VkPipelineColorBlendStateCreateInfo color_blend_info;
        VkPipelineVertexInputStateCreateInfo vertex_input_info;
        VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
        VkPipelineRasterizationStateCreateInfo rasterization_info;
        VkPipelineMultisampleStateCreateInfo multisampling_info;
        VkPipelineRenderingCreateInfo rendering_info;
        VkPipelineDepthStencilStateCreateInfo depth_stencil_info;
        VkPipelineDynamicStateCreateInfo dynamic_info;

        public:
        GraphicsPipelineBuilder(const VkDevice device, 
                                const VkPipelineLayout pipeline_layout, 
                                const VkShaderModule vert_shader_module, 
                                const VkShaderModule frag_shader_module, 
                                const VkFormat default_target_format,
                                const VkFormat default_depth_format,
                                vk_types::CleanupProcedures& cleanup_procedures);

        void override(VkPipelineViewportStateCreateInfo& viewport_info);
        void override(VkPipelineColorBlendStateCreateInfo& color_info);
        void override(VkPipelineVertexInputStateCreateInfo& vertex_input_info);
        void override(VkPipelineInputAssemblyStateCreateInfo& input_assembly_info);
        void override(VkPipelineRasterizationStateCreateInfo& rasterization_info);
        void override(VkPipelineMultisampleStateCreateInfo& multisampling_info);
        void override(VkPipelineRenderingCreateInfo& rendering_info);
        void override(VkPipelineDepthStencilStateCreateInfo& depth_stencil_info);
        void override(VkPipelineDynamicStateCreateInfo& dynamic_info);
        vk_types::Pipeline build();
    };
}

#endif // VK_PIPELINE_H_
