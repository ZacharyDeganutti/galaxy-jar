#include "vk_pipeline.hpp"
#include <array>
#include <fstream>
#include <ios>

namespace vk_pipeline {

    VkPipelineShaderStageCreateInfo make_shader_stage_info(VkShaderStageFlagBits stage_flags, VkShaderModule module) {
        // Then create the pipeline (this is somewhat specific to pipeline type)
        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.pNext = nullptr;
        stage_info.stage = stage_flags;
        stage_info.module = module;
        stage_info.pName = "main";

        return stage_info;
    }

    vk_types::Pipeline init_graphics_pipeline(const VkDevice device, const VkPipelineLayout pipeline_layout, const VkFormat& target_format, const VkFormat& depth_format, const VkShaderModule vert_shader_module, const VkShaderModule frag_shader_module, const std::vector<VkDescriptorSet> descriptor_sets, vk_types::CleanupProcedures& cleanup_procedures) {
        /// Basic viewport setup
        VkPipelineViewportStateCreateInfo viewport_info = {};
        viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_info.pNext = nullptr;
        viewport_info.viewportCount = 1;
        viewport_info.scissorCount = 1;

        /// Basic blending info, no blending for now
        VkPipelineColorBlendAttachmentState blend_attachment_state = {};
        blend_attachment_state.blendEnable = VK_FALSE;
        blend_attachment_state.colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend_info = {};
        blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend_info.pNext = nullptr;
        blend_info.logicOpEnable = VK_FALSE;
        blend_info.logicOp = VK_LOGIC_OP_COPY;
        blend_info.attachmentCount = 1;
        blend_info.pAttachments = &blend_attachment_state;

        /// VertexInputStateCreateInfo is to be configured for non-interleaved positions, normals, and texture coordinates.
        const std::array<VkVertexInputBindingDescription, 3> bindings = {{
            {
                0,                          // binding
                sizeof(glm::vec3),          // stride
                VK_VERTEX_INPUT_RATE_VERTEX // inputRate
            },
            {
                1,                          // binding
                sizeof(glm::vec3),          // stride
                VK_VERTEX_INPUT_RATE_VERTEX // inputRate
            },
            {
                2,
                sizeof(glm::vec2),
                VK_VERTEX_INPUT_RATE_VERTEX
            }
        }};

        const std::array<VkVertexInputAttributeDescription, 3> attributes {{
            {
                0,                          // location
                bindings[0].binding,        // binding
                VK_FORMAT_R32G32B32_SFLOAT, // format
                0                           // offset
            },
            {
                1,                          // location
                bindings[1].binding,        // binding
                VK_FORMAT_R32G32B32_SFLOAT, // format
                0                           // offset
            },
            {
                2,                          // location
                bindings[2].binding,        // binding
                VK_FORMAT_R32G32_SFLOAT,    // format
                0                           // offset
            }
        }};
        VkPipelineVertexInputStateCreateInfo vertex_input_state_info = {};
        vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state_info.pNext = nullptr;
        vertex_input_state_info.pVertexAttributeDescriptions = attributes.data();
        vertex_input_state_info.vertexAttributeDescriptionCount = attributes.size();
        vertex_input_state_info.pVertexBindingDescriptions = bindings.data();
        vertex_input_state_info.vertexBindingDescriptionCount = bindings.size();

        /// Input Assembly configured to draw triangles
        VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.pNext = nullptr;
        input_assembly_info.primitiveRestartEnable = VK_FALSE;
        // Simple but wasteful list of separate triangles
        input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        /// Rasterization configuration
        VkPipelineRasterizationStateCreateInfo rasterization_info = {};
        rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_info.pNext = nullptr;
        // Set up polygons
        rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_info.lineWidth = 1.0f;
        // Culling and winding order
        rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

        /// Multisampling configuration (AA, etc)
        VkPipelineMultisampleStateCreateInfo multisampling_info = {};
        multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling_info.pNext = nullptr;
        multisampling_info.sampleShadingEnable = VK_FALSE;
        multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_info.minSampleShading = 1.0f;
        multisampling_info.pSampleMask = nullptr;
        multisampling_info.alphaToCoverageEnable = VK_FALSE;
        multisampling_info.alphaToOneEnable = VK_FALSE;

        /// Set up rendering
        VkPipelineRenderingCreateInfo rendering_info = {};
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.pNext = nullptr;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachmentFormats = &target_format;
        rendering_info.depthAttachmentFormat = depth_format;

        /// Configure depth
        VkPipelineDepthStencilStateCreateInfo depth_info = {};
        depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_info.pNext = nullptr;
        depth_info.depthTestEnable = VK_TRUE;
        depth_info.depthWriteEnable = VK_TRUE;
        depth_info.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_info.depthBoundsTestEnable = VK_FALSE;
        depth_info.stencilTestEnable = VK_FALSE;
        depth_info.front = {};
        depth_info.back = {};
        depth_info.minDepthBounds = 0.0f;
        depth_info.maxDepthBounds = 1.0f;

        /// Dynamic state info
        std::vector<VkDynamicState> state = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_info = {};
        dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_info.pNext = nullptr;
        dynamic_info.pDynamicStates = state.data();
        dynamic_info.dynamicStateCount = static_cast<uint32_t>(state.size());

        std::vector<VkPipelineShaderStageCreateInfo> shader_stage_infos = {
            make_shader_stage_info(VK_SHADER_STAGE_VERTEX_BIT, vert_shader_module),
            make_shader_stage_info(VK_SHADER_STAGE_FRAGMENT_BIT, frag_shader_module),
        };

        /// Smoosh it all into the pipeline definition, unused stages like tesselation left as 0 initialized nullptr
        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = &rendering_info;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stage_infos.size());
        pipeline_info.pStages = shader_stage_infos.data();
        pipeline_info.pVertexInputState = &vertex_input_state_info;
        pipeline_info.pInputAssemblyState = &input_assembly_info;
        pipeline_info.pViewportState = &viewport_info;
        pipeline_info.pRasterizationState = &rasterization_info;
        pipeline_info.pMultisampleState = &multisampling_info;
        pipeline_info.pColorBlendState = &blend_info;
        pipeline_info.pDepthStencilState = &depth_info;
        pipeline_info.pDynamicState = &dynamic_info;
        pipeline_info.layout = pipeline_layout;

        VkPipeline graphics_pipeline = {};
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
            printf("Unable to create graphics pipeline\n");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, graphics_pipeline](){
            vkDestroyPipeline(device, graphics_pipeline, nullptr);
        });

        vk_types::Pipeline graphics_pipeline_bundle = {};
        graphics_pipeline_bundle.descriptors = descriptor_sets;
        graphics_pipeline_bundle.handle = graphics_pipeline;
        graphics_pipeline_bundle.layout = pipeline_layout;
        graphics_pipeline_bundle.bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

        return graphics_pipeline_bundle;
    }

    // Creates a compute pipeline with the specified compute shader
    vk_types::Pipeline init_compute_pipeline(const VkDevice device, const VkPipelineLayout compute_pipeline_layout, const VkShaderModule shader_module, const std::vector<VkDescriptorSet> descriptor_sets, vk_types::CleanupProcedures& cleanup_procedures) {
        VkPipelineShaderStageCreateInfo stage_info = make_shader_stage_info(VK_SHADER_STAGE_COMPUTE_BIT, shader_module);

        VkComputePipelineCreateInfo compute_pipeline_info{};
        compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compute_pipeline_info.pNext = nullptr;
        compute_pipeline_info.layout = compute_pipeline_layout;
        compute_pipeline_info.stage = stage_info;

        VkPipeline compute_pipeline = {};
        if(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_info, nullptr, &compute_pipeline) != VK_SUCCESS) {
            printf("Unable to create compute pipeline\n");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, compute_pipeline]() {
            vkDestroyPipeline(device, compute_pipeline, nullptr);
        });

        vk_types::Pipeline pipeline = {};
        pipeline.bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        pipeline.handle = compute_pipeline;
        pipeline.layout = compute_pipeline_layout;
        pipeline.descriptors = descriptor_sets;

        return pipeline;
    }

    // Creates a pipeline layout with the specified descriptor set layouts
    VkPipelineLayout init_pipeline_layout(const VkDevice device, const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts, vk_types::CleanupProcedures& cleanup_procedures) {
        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.pNext = nullptr;
        layout_info.pSetLayouts = descriptor_set_layouts.data();
        layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size());

        VkPipelineLayout pipeline_layout = {};
        if(vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
            printf("Unable to create compute pipeline layout\n");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, pipeline_layout]() {
            vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        });

        return pipeline_layout;
    }

    VkShaderModule init_shader_module(const VkDevice device, const char *file_path, vk_types::CleanupProcedures& cleanup_procedures) {
        // open the file. With cursor at the end
        std::ifstream file(file_path, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            printf("Unable to open shader file: %s\n", file_path);
            exit(EXIT_FAILURE);
        }

        // find what the size of the file is by looking up the location of the cursor
        // because the cursor is at the end, it gives the size directly in bytes
        size_t file_size = static_cast<size_t>(file.tellg());

        // spirv expects the buffer to be uint32, so make sure to reserve a int
        // vector big enough for the entire file
        std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

        // put file cursor at beginning
        file.seekg(0);

        // load the entire file into the buffer
        file.read(reinterpret_cast<char *>(buffer.data()), file_size);

        // now that the file is loaded into the buffer, we can close it
        file.close();

        // create a new shader module, using the buffer we loaded
        VkShaderModuleCreateInfo shader_info = {};
        shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_info.pNext = nullptr;

        // codeSize has to be in bytes, so multiply the ints in the buffer by size of
        // int to know the real size of the buffer
        shader_info.codeSize = buffer.size() * sizeof(uint32_t);
        shader_info.pCode = buffer.data();

        VkShaderModule shader_module = {};
        if (vkCreateShaderModule(device, &shader_info, nullptr, &shader_module) != VK_SUCCESS)
        {
            printf("Unable to generate shader module from file: %s\n", file_path);
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, shader_module]() {
            vkDestroyShaderModule(device, shader_module, nullptr);
        });
        return shader_module;
    }
}