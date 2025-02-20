#include "vk_buffer.hpp"
#include "vk_descriptors.hpp"
#include "vk_image.hpp"
#include "vk_init.hpp"
#include "vk_layer.hpp"
#include "vk_pipeline.hpp"
#include "vk_types.hpp"
#include "sync.hpp"

#include <GLFW/glfw3.h>
#include <array>
#include <vulkan/vk_enum_string_helper.h>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <iterator>
#include <algorithm>
#include "vk_mem_alloc.h"


// Forward declarations and such
namespace vk_layer {
    namespace {
        VkCommandBufferSubmitInfo make_command_buffer_submit_info(const VkCommandBuffer cmd);
        VkSubmitInfo2 make_submit_info(const VkCommandBufferSubmitInfo& cmd, const VkSemaphoreSubmitInfo& signal_semaphore_info, const VkSemaphoreSubmitInfo& wait_semaphore_info);
        void clear_attachments(const VkCommandBuffer cmd, std::span<VkRenderingAttachmentInfo> attachments, std::span<VkExtent2D> extents);
    }
}

namespace vk_layer {
    namespace {

        VkSemaphoreSubmitInfo make_semaphore_submit_info(const VkPipelineStageFlags2 stage_mask, const VkSemaphore semaphore) {
            VkSemaphoreSubmitInfo semaphore_submit_info{};
            semaphore_submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            semaphore_submit_info.pNext = nullptr;
            semaphore_submit_info.semaphore = semaphore;
            semaphore_submit_info.stageMask = stage_mask;
            semaphore_submit_info.deviceIndex = 0;
            semaphore_submit_info.value = 1;

            return semaphore_submit_info;
        }

        VkCommandBufferSubmitInfo make_command_buffer_submit_info(const VkCommandBuffer cmd)
        {
            VkCommandBufferSubmitInfo command_buffer_submit_info{};
            command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_info.pNext = nullptr;
            command_buffer_submit_info.commandBuffer = cmd;
            command_buffer_submit_info.deviceMask = 0;

            return command_buffer_submit_info;
        }

        // Makes submit info struct. Passing in zeroed out semaphore submit info will cause those to be ignored
        VkSubmitInfo2 make_submit_info(const VkCommandBufferSubmitInfo& cmd, const VkSemaphoreSubmitInfo& signal_semaphore_info, const VkSemaphoreSubmitInfo& wait_semaphore_info) {
            VkSubmitInfo2 submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            submit_info.pNext = nullptr;

            if (wait_semaphore_info.sType != 0) {
                submit_info.waitSemaphoreInfoCount = 1;
                submit_info.pWaitSemaphoreInfos = &wait_semaphore_info;
            }

            if (signal_semaphore_info.sType != 0) {
                submit_info.signalSemaphoreInfoCount = 1;
                submit_info.pSignalSemaphoreInfos = &signal_semaphore_info;
            }

            submit_info.commandBufferInfoCount = 1;
            submit_info.pCommandBufferInfos = &cmd;

            return submit_info;
        }
        
        // Clears out attachments, setting color buffers to (0, 0, 0, 1), and depth buffers to 1
        void clear_attachments(const VkCommandBuffer cmd, std::span<VkRenderingAttachmentInfo> attachments, std::span<VkExtent2D> extents) {
            std::vector<VkClearRect> clear_rects;
            clear_rects.reserve(attachments.size());
            std::vector<VkClearAttachment> clear_properties;
            clear_properties.reserve(attachments.size());

            uint32_t color_attachment_count = 0;
            for (size_t attachment_index = 0; attachment_index < attachments.size(); ++attachment_index) {
                auto& attachment = attachments[attachment_index];
                bool is_color = (attachment.imageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                VkClearAttachment clear_prop = {};
                if (is_color) {
                    clear_prop.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    clear_prop.clearValue.color.float32[0] = 0.0f;
                    clear_prop.clearValue.color.float32[1] = 0.0f;
                    clear_prop.clearValue.color.float32[2] = 0.0f;
                    clear_prop.clearValue.color.float32[3] = 1.0f;
                    clear_prop.colorAttachment = color_attachment_count++;
                } else {
                    clear_prop.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    clear_prop.clearValue.depthStencil.depth = 1.0f;
                }
                clear_properties.push_back(clear_prop);

                VkRect2D rect = {};
                rect.offset.x = 0;
                rect.offset.y = 0;
                rect.extent = extents[attachment_index];

                VkClearRect clear_rect = {};
                clear_rect.baseArrayLayer = 0;
                clear_rect.layerCount = 1;
                clear_rect.rect = rect;

                clear_rects.push_back(clear_rect);
            }

            vkCmdClearAttachments(cmd, clear_properties.size(), clear_properties.data(), clear_rects.size(), clear_rects.data());
        }

        void draw_compute(  const VkCommandBuffer cmd,
                            const std::function<std::vector<VkDescriptorSet>()>& get_descriptor_sets, 
                            const std::function<void()>& set_push_constants,
                            const vk_types::Pipeline& pipeline,
                            const uint32_t dispatch_x,
                            const uint32_t dispatch_y,
                            const DrawState& state)
        {
            vkCmdBindPipeline(cmd, pipeline.bind_point, pipeline.handle);
            std::vector<VkDescriptorSet> descriptor_sets = get_descriptor_sets();
            set_push_constants();
            vkCmdBindDescriptorSets(cmd, pipeline.bind_point, pipeline.layout, 0, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
            vkCmdDispatch(cmd, dispatch_x, dispatch_y, 1);
        }

        void draw_background_skybox(const VkCommandBuffer cmd,
                                    const std::function<std::vector<VkDescriptorSet>(size_t)>& get_descriptor_sets, 
                                    const std::function<void(size_t)>& set_push_constants,
                                    const vk_types::AllocatedImage& background_target,
                                    const vk_types::Pipeline& pipeline,
                                    const Drawable& cube_model,
                                    const DrawState& state) {
            // Set up draw target attachment
            VkRenderingAttachmentInfo color_attachment = {}; 
            color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            color_attachment.pNext = nullptr;
            color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color_attachment.imageView = background_target.image_view;
            color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            // Not multisampling so set this off and leave the resolve view and layouts zeroed out
            color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
            // Not using VK_ATTACHMENT_LOAD_OP_CLEAR, so no need to set clear value either
            
            VkRenderingInfo render_info = {};
            render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            render_info.pNext = nullptr;
            // Don't think any fancy render flags are needed, leave zeroed
            render_info.viewMask = 0;
            render_info.layerCount = 1;
            render_info.pColorAttachments = &color_attachment;
            render_info.colorAttachmentCount = 1;
            render_info.pDepthAttachment = nullptr;
            render_info.renderArea.extent = background_target.image_extent;
            render_info.renderArea.offset = VkOffset2D{ 0, 0 };
            vkCmdBeginRendering(cmd, &render_info);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);

            //set dynamic viewport and scissor
            VkViewport viewport = {};
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = background_target.image_extent.width;
            viewport.height = background_target.image_extent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent = background_target.image_extent;

            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // Draw all buffers
            for (int piece = 0; piece < cube_model.gpu_model.vertex_buffers.size(); ++piece) {
                // Bind up the descriptors to match each piece
                std::vector<VkDescriptorSet> skybox_descriptor_sets = get_descriptor_sets(piece);
                vkCmdBindDescriptorSets(cmd, pipeline.bind_point, pipeline.layout, 0, skybox_descriptor_sets.size(), skybox_descriptor_sets.data(), 0, nullptr);

                // Push the push constants
                set_push_constants(piece);

                // Bind the vertex buffers and fire off an indexed draw
                const vk_types::GpuMeshBuffers& buffer_group = cube_model.gpu_model.vertex_buffers[piece];
                vkCmdBindIndexBuffer(cmd, buffer_group.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                std::array<VkBuffer, 3> buffer_handles {{
                    buffer_group.position_buffer.vertex_buffer.buffer,
                    buffer_group.normal_buffer.vertex_buffer.buffer,
                    buffer_group.texture_coordinate_buffer.vertex_buffer.buffer
                }};
                std::array<VkDeviceSize, 3> offsets {{0,0,0}};
                vkCmdBindVertexBuffers(cmd, 0, buffer_handles.size(), buffer_handles.data(), offsets.data());
                vkCmdDrawIndexed(cmd, buffer_group.index_count, 1, 0, 0, 0);
            }

            vkCmdEndRendering(cmd);
        }

        void draw_geometry( const VkCommandBuffer cmd, 
                            const std::function<std::vector<VkDescriptorSet>(size_t)>& get_descriptor_sets, 
                            const std::function<void(size_t)>& set_push_constants,
                            bool clear_color, const vk_types::AllocatedImage& draw_target, 
                            const vk_types::AllocatedImage& depth_buffer, 
                            const vk_types::Pipeline& pipeline, 
                            const Drawable& drawable, 
                            const DrawState& state ) {
            //begin a render pass  connected to our draw image

            // Set up draw target attachment
            VkRenderingAttachmentInfo color_attachment = {}; 
            color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            color_attachment.pNext = nullptr;
            color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color_attachment.imageView = draw_target.image_view;
            color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            // Not multisampling so set this off and leave the resolve view and layouts zeroed out
            color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
            // Not using VK_ATTACHMENT_LOAD_OP_CLEAR, so no need to set clear value either

            // Set up depth buffer attachment
            VkRenderingAttachmentInfo depth_attachment = {}; 
            depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depth_attachment.pNext = nullptr;
            depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depth_attachment.imageView = depth_buffer.image_view;
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            // Not multisampling so set this off and leave the resolve view and layouts zeroed out
            depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
            // Not using VK_ATTACHMENT_LOAD_OP_CLEAR, so no need to set clear value either
            
            VkRenderingInfo render_info = {};
            render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            render_info.pNext = nullptr;
            // Don't think any fancy render flags are needed, leave zeroed
            render_info.viewMask = 0;
            render_info.layerCount = 1;
            render_info.pColorAttachments = &color_attachment;
            render_info.colorAttachmentCount = 1;
            render_info.pDepthAttachment = &depth_attachment;
            render_info.renderArea.extent = draw_target.image_extent;
            render_info.renderArea.offset = VkOffset2D{ 0, 0 };

            vkCmdBeginRendering(cmd, &render_info);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);

            //set dynamic viewport and scissor
            VkViewport viewport = {};
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = draw_target.image_extent.width;
            viewport.height = draw_target.image_extent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent = draw_target.image_extent;

            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // Clear out necessary color/depth buffers
            if (clear_color) {
                std::array<VkRenderingAttachmentInfo, 2> attachments = {color_attachment, depth_attachment};
                std::array<VkExtent2D, 2> extents = {draw_target.image_extent, depth_buffer.image_extent};
                clear_attachments(cmd, attachments, extents);
            }
            else {
                std::array<VkRenderingAttachmentInfo, 1> attachments = {depth_attachment};
                std::array<VkExtent2D, 1> extents = {depth_buffer.image_extent};
                clear_attachments(cmd, attachments, extents);
            }
            
            
            // Draw all buffers
            for (int piece = 0; piece < drawable.gpu_model.vertex_buffers.size(); ++piece) {
                // Bind up the descriptors to match each piece
                std::vector<VkDescriptorSet> descriptor_sets = get_descriptor_sets(piece);
                vkCmdBindDescriptorSets(cmd, pipeline.bind_point, pipeline.layout, 0, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);

                // Push the push constants
                set_push_constants(piece);

                // Bind the vertex buffers and fire off an indexed draw
                const vk_types::GpuMeshBuffers& buffer_group = drawable.gpu_model.vertex_buffers[piece];
                vkCmdBindIndexBuffer(cmd, buffer_group.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                std::array<VkBuffer, 3> buffer_handles {{
                    buffer_group.position_buffer.vertex_buffer.buffer,
                    buffer_group.normal_buffer.vertex_buffer.buffer,
                    buffer_group.texture_coordinate_buffer.vertex_buffer.buffer
                }};
                std::array<VkDeviceSize, 3> offsets {{0,0,0}};
                vkCmdBindVertexBuffers(cmd, 0, buffer_handles.size(), buffer_handles.data(), offsets.data());
                vkCmdDrawIndexed(cmd, buffer_group.index_count, 1, 0, 0, 0);
            }

            vkCmdEndRendering(cmd);
        }

    }

    void immediate_submit(const vk_types::Context& res, std::function<void(VkCommandBuffer cmd)>&& function) {
        if(vkResetFences(res.device, 1, &res.fence_immediate) != VK_SUCCESS) {
            printf("Failed to reset fence during immediate submission\n");
            exit(EXIT_FAILURE);
        }
        if(vkResetCommandBuffer(res.command_immediate.buffer_primary, 0) != VK_SUCCESS) {
            printf("Failed to reset command buffer during immediate submission\n");
            exit(EXIT_FAILURE);
        }

        VkCommandBuffer cmd = res.command_immediate.buffer_primary;

        VkCommandBufferBeginInfo cmd_begin_info = {};
        cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_begin_info.pNext = nullptr;
        cmd_begin_info.pInheritanceInfo = nullptr;
        cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Command buffer will be used exactly once

        if(vkBeginCommandBuffer(cmd, &cmd_begin_info) != VK_SUCCESS) {
            printf("Failed to begin command buffer during immediate submission\n");
            exit(EXIT_FAILURE);
        }

        function(cmd);

        if(vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            printf("Failed to end command buffer during immediate submission\n");
            exit(EXIT_FAILURE);
        }

        VkCommandBufferSubmitInfo cmd_info = make_command_buffer_submit_info(cmd);
        VkSubmitInfo2 submit = make_submit_info(cmd_info, {}, {});

        if(vkQueueSubmit2(res.queues.graphics, 1, &submit, res.fence_immediate) != VK_SUCCESS) {
            printf("Failed to submit command buffer during immediate submission\n");
            exit(EXIT_FAILURE);
        }

        if(vkWaitForFences(res.device, 1, &res.fence_immediate, true, 9999999999) != VK_SUCCESS) {
            printf("Failed to wait on fence during immediate submission\n");
            exit(EXIT_FAILURE);
        }
    }

    uint32_t upload_skybox(vk_types::Context& context, const vk_image::HostImage& skybox_image, vk_types::CleanupProcedures& lifetime) {
        VkSampler texture_sampler = vk_image::init_linear_sampler(context);
        vk_types::AllocatedImage skybox_texture = {};
        skybox_texture = vk_image::upload_image(context, skybox_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, lifetime);

        return context.mega_descriptor_set.register_combined_image_sampler_descriptor(context.device, skybox_texture.image_view, texture_sampler); 
    }
    
    Pipelines build_pipelines(vk_types::Context& context, const DescriptorSetLayouts& descriptor_layouts, RenderTargets& render_targets, vk_types::CleanupProcedures& lifetime) {
        /// Assemble the 'default' gradient drawing compute pipeline
        vk_types::Pipeline grid_pipeline = {};
        {
            VkShaderModule gradient_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/gradient.glsl.comp.spv", lifetime);
            VkPushConstantRange grid_pc_range = push_constant_range<GridPassPushConstants>(VK_SHADER_STAGE_COMPUTE_BIT);
            VkPipelineLayout gradient_pipeline_layout = vk_pipeline::init_pipeline_layout(context.device, descriptor_layouts.grid, grid_pc_range, lifetime);
            grid_pipeline = vk_pipeline::init_compute_pipeline(context.device, gradient_pipeline_layout, gradient_shader, lifetime);
        }

        /// Assemble the pipeline to compose all of the images into the final image
        vk_types::Pipeline compose_pipeline = {};
        {
            VkShaderModule compose_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/compose.glsl.comp.spv", lifetime);
            VkPushConstantRange compose_pc_range = push_constant_range<ComposePassPushConstants>(VK_SHADER_STAGE_COMPUTE_BIT);
            VkPipelineLayout compose_pipeline_layout = vk_pipeline::init_pipeline_layout(context.device, descriptor_layouts.compose, compose_pc_range, lifetime);
            compose_pipeline = vk_pipeline::init_compute_pipeline(context.device, compose_pipeline_layout, compose_shader, lifetime);
        }

        /// Assemble the graphics pipeline
        vk_types::Pipeline space_pipeline = {};
        {
            VkShaderModule vert_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/colored_triangle.glsl.vert.spv", lifetime);
            VkShaderModule frag_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/colored_triangle.glsl.frag.spv", lifetime);
            VkPushConstantRange graphics_pc_range = push_constant_range<SpacePassPushConstants>(VK_SHADER_STAGE_FRAGMENT_BIT);
            VkPipelineLayout graphics_pipeline_layout = vk_pipeline::init_pipeline_layout(context.device, descriptor_layouts.graphics, graphics_pc_range, lifetime);
            vk_pipeline::GraphicsPipelineBuilder standard_render_pipeline_builder = vk_pipeline::GraphicsPipelineBuilder(context.device, graphics_pipeline_layout, vert_shader, frag_shader, render_targets.space.image_format, render_targets.space_depth.image_format, lifetime);
            space_pipeline = standard_render_pipeline_builder.build();
        }

        /// Assemble the skybox pipeline
        vk_types::Pipeline skybox_pipeline = {};
        {
            VkShaderModule skybox_vert_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/skybox.glsl.vert.spv", lifetime);
            VkShaderModule skybox_frag_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/skybox.glsl.frag.spv", lifetime);
            VkPushConstantRange skybox_pc_range = push_constant_range<SkyboxPassPushConstants>(VK_SHADER_STAGE_FRAGMENT_BIT);
            VkPipelineLayout skybox_pipeline_layout = vk_pipeline::init_pipeline_layout(context.device, descriptor_layouts.skybox, skybox_pc_range, lifetime);
            vk_pipeline::GraphicsPipelineBuilder skybox_render_pipeline_builder = vk_pipeline::GraphicsPipelineBuilder(context.device, skybox_pipeline_layout, skybox_vert_shader, skybox_frag_shader, render_targets.space.image_format, VK_FORMAT_UNDEFINED, lifetime);
            // Set up rasterization the same, but so that the inside of the geometry is drawn
            VkPipelineRasterizationStateCreateInfo rasterization_info = {};
            rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterization_info.pNext = nullptr;
            rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
            rasterization_info.lineWidth = 1.0f;
            rasterization_info.cullMode = VK_CULL_MODE_FRONT_BIT;
            rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            skybox_render_pipeline_builder.override(rasterization_info);
            // Disable depth testing
            VkPipelineDepthStencilStateCreateInfo depth_info = {};
            depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_info.pNext = nullptr;
            depth_info.depthTestEnable = VK_FALSE;
            depth_info.depthWriteEnable = VK_FALSE;
            depth_info.depthCompareOp = VK_COMPARE_OP_LESS;
            depth_info.depthBoundsTestEnable = VK_FALSE;
            depth_info.stencilTestEnable = VK_FALSE;
            depth_info.front = {};
            depth_info.back = {};
            depth_info.minDepthBounds = 0.0f;
            depth_info.maxDepthBounds = 1.0f;
            skybox_render_pipeline_builder.override(depth_info);
            skybox_pipeline = skybox_render_pipeline_builder.build();
        }

        /// Assemble the jar cutaway mask pipeline
        vk_types::Pipeline jar_cutaway_mask_pipeline = {};
        {
            VkShaderModule jar_cutaway_mask_vert_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/jar_cutaway_mask.glsl.vert.spv", lifetime);
            VkShaderModule jar_cutaway_mask_frag_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/jar_cutaway_mask.glsl.frag.spv", lifetime);
            VkPipelineLayout jar_cutaway_mask_pipeline_layout = vk_pipeline::init_pipeline_layout(context.device, descriptor_layouts.jar_cutaway_mask, lifetime);
            vk_pipeline::GraphicsPipelineBuilder jar_cutaway_mask_pipeline_builder = vk_pipeline::GraphicsPipelineBuilder(context.device, jar_cutaway_mask_pipeline_layout, jar_cutaway_mask_vert_shader, jar_cutaway_mask_frag_shader, render_targets.jar_mask.image_format, render_targets.jar_mask_depth.image_format, lifetime);
            // Set up rasterization so that both the inward and outward faces generate fragments
            VkPipelineRasterizationStateCreateInfo rasterization_info = {};
            rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterization_info.pNext = nullptr;
            rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
            rasterization_info.lineWidth = 1.0f;
            rasterization_info.cullMode = VK_CULL_MODE_NONE;
            rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            jar_cutaway_mask_pipeline_builder.override(rasterization_info);
            // Disable depth testing, but enable depth write. Surface depth will be referenced later on in compositing.
            VkPipelineDepthStencilStateCreateInfo depth_info = {};
            depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_info.pNext = nullptr;
            depth_info.depthTestEnable = VK_FALSE;
            depth_info.depthWriteEnable = VK_TRUE;
            depth_info.depthCompareOp = VK_COMPARE_OP_LESS;
            depth_info.depthBoundsTestEnable = VK_FALSE;
            depth_info.stencilTestEnable = VK_FALSE;
            depth_info.front = {};
            depth_info.back = {};
            depth_info.minDepthBounds = 0.0f;
            depth_info.maxDepthBounds = 1.0f;
            jar_cutaway_mask_pipeline_builder.override(depth_info);
            // Unconditional blending is desired. Can't just discard the outward fragments because those are needed to generate the depth
            VkPipelineColorBlendAttachmentState color_blend_attachment{};
            color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            color_blend_attachment.blendEnable = VK_TRUE;
            color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
            VkPipelineColorBlendStateCreateInfo color_blend_info = {};
            color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blend_info.pNext = nullptr;
            color_blend_info.logicOpEnable = VK_FALSE;
            color_blend_info.logicOp = VK_LOGIC_OP_COPY;
            color_blend_info.attachmentCount = 1;
            color_blend_info.pAttachments = &color_blend_attachment;
            jar_cutaway_mask_pipeline_builder.override(color_blend_info);
            jar_cutaway_mask_pipeline = jar_cutaway_mask_pipeline_builder.build();
        }

        Pipelines pipes =  Pipelines {
            .jar_cutaway_mask = jar_cutaway_mask_pipeline,
            .grid = grid_pipeline,
            .space = space_pipeline,
            .skybox = skybox_pipeline,
            .compose = compose_pipeline
        };

        return pipes;
    }

    BufferedUniform<GlobalUniforms> build_global_uniforms(vk_types::Context& context, const size_t buffer_count, vk_types::CleanupProcedures& lifetime) {

        glm::mat4 view = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f)), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        view = glm::translate(view, glm::vec3(0.0f, 0.0f, 2.0f));
        
        glm::mat4 projection = glm::transpose(glm::perspective(45.0f, 4.0f/3.0f, 1.0f, 1000.0f));
        projection = glm::transpose(projection);
        glm::mat4 vulkan_flip = glm::mat4(  1.0f, 0.0f, 0.0f, 0.0f,
                                            0.0f, -1.0f, 0.0f, 0.0f,
                                            0.0f, 0.0f, 1.0f, 0.0f,
                                            0.0f, 0.0f, 0.0f, 1.0f);
        projection = projection * vulkan_flip;

        GlobalUniforms uniform_contents = GlobalUniforms {
            view,
            projection,
            glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)
        };

        return BufferedUniform<GlobalUniforms>(context, uniform_contents, buffer_count, lifetime);
    }

    BufferedUniform<SkyboxUniforms> build_skybox_uniforms(vk_types::Context& context, const size_t buffer_count, vk_types::CleanupProcedures& lifetime) {
        glm::mat4 cam_rotation = glm::mat4(1.0f);

        SkyboxUniforms uniform_contents = SkyboxUniforms {
            cam_rotation
        };

        return BufferedUniform<SkyboxUniforms>(context, uniform_contents, buffer_count, lifetime);
    }

    RenderTargets build_render_targets(vk_types::Context& context, vk_types::CleanupProcedures& lifetime) {
        
        // Build a bunch of draw targets
        VkFormat full_color_target_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat jar_cutaway_target_format = VK_FORMAT_R16_SFLOAT;
        VkImageUsageFlags draw_target_flags = 
            VK_IMAGE_USAGE_TRANSFER_DST_BIT 
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT 
            | VK_IMAGE_USAGE_STORAGE_BIT 
            | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT;
        VkExtent2D draw_target_extent = {
            context.swapchain.extent.width,
            context.swapchain.extent.height
        };

        const uint32_t NO_MIPMAP = 1;
        vk_types::AllocatedImage compose_draw_target = vk_image::init_allocated_image(context.device, context.allocator, vk_image::Representation::Flat, full_color_target_format, draw_target_flags, NO_MIPMAP, draw_target_extent, lifetime);
        vk_types::AllocatedImage space_draw_target = vk_image::init_allocated_image(context.device, context.allocator, vk_image::Representation::Flat, full_color_target_format, draw_target_flags, NO_MIPMAP, draw_target_extent, lifetime);
        vk_types::AllocatedImage grid_draw_target = vk_image::init_allocated_image(context.device, context.allocator, vk_image::Representation::Flat, full_color_target_format, draw_target_flags, NO_MIPMAP, draw_target_extent, lifetime);
        vk_types::AllocatedImage jar_cutaway_draw_target = vk_image::init_allocated_image(context.device, context.allocator, vk_image::Representation::Flat, jar_cutaway_target_format, draw_target_flags, NO_MIPMAP, draw_target_extent, lifetime);
        
        // Allocate depth targets as well for the draw targets that write to them
        VkFormat depth_buffer_format = VK_FORMAT_D16_UNORM;
        VkImageUsageFlags depth_buffer_flags = 
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkExtent2D depth_buffer_extent = {
            context.swapchain.extent.width,
            context.swapchain.extent.height
        };

        vk_types::AllocatedImage space_depth_buffer = vk_image::init_allocated_image(context.device, context.allocator, vk_image::Representation::Flat, depth_buffer_format, depth_buffer_flags, NO_MIPMAP, depth_buffer_extent, lifetime);
        vk_types::AllocatedImage jar_cutaway_depth_buffer = vk_image::init_allocated_image(context.device, context.allocator, vk_image::Representation::Flat, depth_buffer_format, depth_buffer_flags, NO_MIPMAP, depth_buffer_extent, lifetime);

        // All sampled render targets will use the same sampler
        VkSampler linear_sampler = vk_image::init_linear_sampler(context);

        // Register all of the render targets with descriptor handles
        // The grid image is both stored and sampled, so handles are registered for both types and the sampler is registered separately
        uint32_t grid_draw_target_sampled_index = context.mega_descriptor_set.register_sampled_image_descriptor(context.device, grid_draw_target.image_view);
        uint32_t grid_draw_target_storage_index = context.mega_descriptor_set.register_storage_image_descriptor(context.device, grid_draw_target.image_view);
        uint32_t linear_sampler_index = context.mega_descriptor_set.register_sampler_descriptor(context.device, linear_sampler);
        // The compose target is the final step so it's unnecessary to have a sampled version
        uint32_t compose_draw_target_storage_index = context.mega_descriptor_set.register_storage_image_descriptor(context.device, compose_draw_target.image_view);

        // The rest are drawn to via graphics pipelines, so a simple combined image sampler for each will do.
        uint32_t space_draw_target_index = context.mega_descriptor_set.register_combined_image_sampler_descriptor(context.device, space_draw_target.image_view, linear_sampler);
        uint32_t space_depth_buffer_index = context.mega_descriptor_set.register_combined_image_sampler_descriptor(context.device, space_depth_buffer.image_view, linear_sampler);
        uint32_t jar_cutaway_draw_target_index = context.mega_descriptor_set.register_combined_image_sampler_descriptor(context.device, jar_cutaway_draw_target.image_view, linear_sampler);
        uint32_t jar_cutaway_depth_buffer_index = context.mega_descriptor_set.register_combined_image_sampler_descriptor(context.device, jar_cutaway_depth_buffer.image_view, linear_sampler);

        RenderTargets target_indices = {};
        target_indices.grid_sampled_index = grid_draw_target_sampled_index;
        target_indices.grid_sampler_index = linear_sampler_index;
        target_indices.grid_storage_index = grid_draw_target_storage_index;
        target_indices.grid = grid_draw_target;
        target_indices.compose_storage_index = compose_draw_target_storage_index;
        target_indices.compose_storage = compose_draw_target;
        target_indices.space_index = space_draw_target_index;
        target_indices.space = space_draw_target;
        target_indices.space_depth_index = space_depth_buffer_index;
        target_indices.space_depth = space_depth_buffer;
        target_indices.jar_mask_index = jar_cutaway_draw_target_index;
        target_indices.jar_mask = jar_cutaway_draw_target;
        target_indices.jar_mask_depth_index = jar_cutaway_depth_buffer_index;
        target_indices.jar_mask_depth = jar_cutaway_depth_buffer;
        
        return target_indices;
    }

    Drawable make_drawable(vk_types::Context& context, const geometry::HostModel& model_data) {
        geometry::GpuModel drawable_gpu_model = geometry::upload_model(context, model_data);
        // Set up transform so the preferred coordinate system can be used from here. Build the uniform resources with it
        glm::mat4 transform = geometry::make_x_right_y_up_z_forward_transform(model_data.basis);
        auto buffered_transform = BufferedUniform<glm::mat4>(context, transform, context.buffer_count, context.cleanup_procedures);

        Drawable drawable = {
            drawable_gpu_model,
            buffered_transform
        };
        return drawable;
    }

    DrawState draw( const vk_types::Context& vk_res, 
                    const Pipelines& pipelines, 
                    const RenderTargets& render_targets,
                    const std::vector<Drawable>& drawables, 
                    const std::vector<Drawable>& masking_jars, 
                    const Drawable& skybox, 
                    const uint32_t skybox_texture_index, 
                    const DrawState& state)
    {
        // Wait for previous frame to finish drawing (if applicable). Timeout 1s
        if (state.frame_num >= vk_res.buffer_count) {
            uint32_t wait_frame = state.frame_in_flight;
            if ((vkWaitForFences(vk_res.device, 1, &(vk_res.synchronization[wait_frame].render_fence), VK_TRUE, 1000000000)) != VK_SUCCESS) {
                printf("Unable to wait on fence for previous frame %d\n", wait_frame);
                exit(EXIT_FAILURE);
            }
            if ((vkResetFences(vk_res.device, 1, &(vk_res.synchronization[wait_frame].render_fence))) != VK_SUCCESS) {
                printf("Unable to reset fence for previous frame %d\n", wait_frame);
                exit(EXIT_FAILURE);
            }
        }
        else {
            // Reset all fences on the first run through
            // May spuriously hit this path after running for 1 billion continuous centuries :(
            if ((vkResetFences(vk_res.device, 1, &(vk_res.synchronization[state.frame_num].render_fence))) != VK_SUCCESS) {
                printf("Unable to do initial reset of fences\n");
                exit(EXIT_FAILURE);
            }
        }

        // Request image from the swapchain, 1s timeout
        uint32_t swapchain_image_index = 0;
        VkResult img_get_result = (vkAcquireNextImageKHR(vk_res.device, vk_res.swapchain.handle, 1000000000, vk_res.synchronization[state.buf_num].swapchain_semaphore, nullptr, &swapchain_image_index));
        if (img_get_result != VK_SUCCESS) {
            printf("Unable to get swapchain image for frame %d\n", state.buf_num);
            exit(EXIT_FAILURE);
        }

        /// Begin setting up command buffer and recording ///
        // Rename for ergonomics
        VkCommandBuffer cmd = vk_res.command[state.buf_num].buffer_primary;

        // Reset it so it can be recorded
        if ((vkResetCommandBuffer(cmd, 0)) != VK_SUCCESS) {
            printf("Unable to reset command buffer\n");
            exit(EXIT_FAILURE);
        }

        // Setup recording begin structure
        VkCommandBufferBeginInfo cmd_begin_info = {};
        cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_begin_info.pNext = nullptr;
        cmd_begin_info.pInheritanceInfo = nullptr;
        cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Command buffer will be used exactly once

        // Fire off recording
        if ((vkBeginCommandBuffer(cmd, &cmd_begin_info)) != VK_SUCCESS) {
            printf("Unable to begin command buffer recording\n");
            exit(EXIT_FAILURE);
        }

        // Make the draw target drawable by compute shaders
        sync::transition_image(cmd, render_targets.grid.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        auto get_grid_descriptor_sets = [&]() {
            std::vector<VkDescriptorSet> sets = {
                vk_res.mega_descriptor_set.bundle.set
            };
            return sets;
        };
        auto set_grid_push_constants = [&]() {
            GridPassPushConstants constants = {};
            constants.grid_storage_index = render_targets.grid_storage_index;
            vkCmdPushConstants(cmd, pipelines.grid.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GridPassPushConstants), &constants);
        };
        draw_compute(cmd, 
                     get_grid_descriptor_sets,
                     set_grid_push_constants,
                     pipelines.grid,
                     std::ceil(render_targets.grid.image_extent.width / 16.0),
                     std::ceil(render_targets.grid.image_extent.height / 16.0),
                     state);

        // Draw the skybox onto the target
        sync::transition_image(cmd, render_targets.space.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto get_skybox_descriptor_sets = [&](size_t piece) {
            std::vector<VkDescriptorSet> graphics_descriptor_sets = { 
                state.skybox_dynamic_uniforms.get_descriptor_set(state.frame_in_flight),
                vk_res.mega_descriptor_set.bundle.set
            };
            return graphics_descriptor_sets;
        };
        auto set_skybox_push_constants = [&](size_t piece) {
            SkyboxPassPushConstants constants = {};
            constants.skybox_texture_index = skybox_texture_index;
            vkCmdPushConstants(cmd, pipelines.skybox.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyboxPassPushConstants), &constants);
        };
        draw_background_skybox(cmd, get_skybox_descriptor_sets, set_skybox_push_constants, render_targets.space, pipelines.skybox, skybox, state);

        // Build the jar cutaway mask
        sync::transition_image(cmd, render_targets.jar_mask.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        for (auto& jar : masking_jars) {
            auto get_jar_descriptor_sets = [&](size_t piece) {
                std::vector<VkDescriptorSet> jar_descriptor_sets = { 
                    state.main_dynamic_uniforms.get_descriptor_set(state.frame_in_flight),
                    vk_res.mega_descriptor_set.bundle.set,
                    jar.transform.get_descriptor_set(state.frame_in_flight)
                };
                return jar_descriptor_sets;
            };
            bool CLEAR_COLOR = true;
            draw_geometry(cmd, get_jar_descriptor_sets, [](size_t piece){}, CLEAR_COLOR, render_targets.jar_mask, render_targets.jar_mask_depth, pipelines.jar_cutaway_mask, jar, state);
        }
        
        // Draw the space scene
        sync::transition_image(cmd, render_targets.space.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        for (auto& drawable : drawables) {
            auto get_graphics_descriptor_sets = [&](size_t piece) {
                std::vector<VkDescriptorSet> graphics_descriptor_sets = { 
                    state.main_dynamic_uniforms.get_descriptor_set(state.frame_in_flight),
                    vk_res.mega_descriptor_set.bundle.set,
                    drawable.transform.get_descriptor_set(state.frame_in_flight)
                };
                return graphics_descriptor_sets;
            };
            auto set_graphics_push_constants = [&](size_t piece) {
                SpacePassPushConstants constants = {};
                constants.diffuse_texture_index = drawable.gpu_model.diffuse_texture_indices[piece];
                constants.normal_texture_index = drawable.gpu_model.normal_texture_indices[piece];
                constants.specular_texture_index = drawable.gpu_model.specular_texture_indices[piece];

                vkCmdPushConstants(cmd, pipelines.space.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SpacePassPushConstants), &constants);
            };
            bool DO_NOT_CLEAR_COLOR = false;
            draw_geometry(cmd, get_graphics_descriptor_sets, set_graphics_push_constants, DO_NOT_CLEAR_COLOR, render_targets.space, render_targets.space_depth, pipelines.space, drawable, state);
        }

        // Compose the gbuffers together
        sync::transition_image(cmd, render_targets.compose_storage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        sync::transition_image(cmd, render_targets.space.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
        sync::transition_image(cmd, render_targets.space_depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
        sync::transition_image(cmd, render_targets.jar_mask.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
        sync::transition_image(cmd, render_targets.jar_mask_depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
        sync::transition_image(cmd, render_targets.grid.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
        auto get_compose_descriptor_sets = [&]() {
            std::vector<VkDescriptorSet> sets = {
                vk_res.mega_descriptor_set.bundle.set
            };
            return sets;
        };
        auto set_compose_push_constants = [&]() {
            ComposePassPushConstants constants = {};
            constants.compose_storage_index = render_targets.compose_storage_index;
            constants.grid_sampled_index = render_targets.grid_sampled_index;
            constants.grid_sampler_index = render_targets.grid_sampler_index;
            constants.jar_mask_depth_index = render_targets.jar_mask_depth_index;
            constants.jar_mask_index = render_targets.jar_mask_index;
            constants.space_depth_index = render_targets.space_depth_index;
            constants.space_index = render_targets.space_index;
            vkCmdPushConstants(cmd, pipelines.compose.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComposePassPushConstants), &constants);
        };
        draw_compute(cmd, 
                     get_compose_descriptor_sets,
                     set_compose_push_constants,
                     pipelines.compose,
                     std::ceil(render_targets.compose_storage.image_extent.width / 16.0),
                     std::ceil(render_targets.compose_storage.image_extent.height / 16.0),
                     state);

        // Transfer from the draw target to the swapchain
        sync::transition_image(cmd, render_targets.compose_storage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        sync::transition_image(cmd, vk_res.swapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vk_image::blit_image_to_image_no_mipmap(cmd, render_targets.compose_storage.image, vk_res.swapchain.images[swapchain_image_index], render_targets.compose_storage.image_extent, vk_res.swapchain.extent);

        // After drawing, transition the image to presentable
        sync::transition_image(cmd, vk_res.swapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        // Finalize the command buffer, making it executable
        if ((vkEndCommandBuffer(cmd)) != VK_SUCCESS) {
            printf("Unable to end command buffer recording\n");
            exit(EXIT_FAILURE);
        }

        /// Prep for queue submission ///
        VkCommandBufferSubmitInfo cmd_submit_info = make_command_buffer_submit_info(cmd);
        // Setup our semaphores.
        // We wait on the swapchain becoming ready
        VkSemaphoreSubmitInfo wait_semaphore_info = make_semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, vk_res.synchronization[state.buf_num].swapchain_semaphore);
        // We signal the render semaphore when we're done drawing
        VkSemaphoreSubmitInfo signal_semaphore_info = make_semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, vk_res.synchronization[state.buf_num].render_semaphore);
        VkSubmitInfo2 submit_info = make_submit_info(cmd_submit_info, signal_semaphore_info, wait_semaphore_info);

        // Fire the command buffer off to the queue
        if (auto res = (vkQueueSubmit2(vk_res.queues.graphics, 1, &submit_info, vk_res.synchronization[state.buf_num].render_fence)) != VK_SUCCESS) {
            printf("Unable to submit command buffer, result %d\n", res);
            exit(EXIT_FAILURE);
        }

        /// Setup presentation ///
        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.pNext = nullptr;
        present_info.pSwapchains = &vk_res.swapchain.handle;
        present_info.swapchainCount = 1;

        present_info.pWaitSemaphores = &vk_res.synchronization[state.buf_num].render_semaphore;
        present_info.waitSemaphoreCount = 1;

        present_info.pImageIndices = &swapchain_image_index;

        if(vkQueuePresentKHR(vk_res.queues.graphics, &present_info) != VK_SUCCESS) {
            printf("Unable to present image\n");
            exit(EXIT_FAILURE);
        }

        /// Update state for next frame ///
        glm::mat4 rotated_view = glm::rotate(state.main_dynamic_uniforms.get().view, glm::radians(-0.01f), glm::vec3(0.0f, 1.0f, 0.0f));
        //glm::mat4 rotated_view = state.main_dynamic_uniforms.view.get();
        //glm::vec4 rotated_sun = glm::rotate(state.main_dynamic_uniforms.sun_direction.get(), glm::radians(0.01f), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec4 rotated_sun = state.main_dynamic_uniforms.get().sun_direction;

        // Face the same direction as the main rendering camera
        glm::mat4 cam_rotation = glm::mat4x4(rotated_view[0], rotated_view[1], rotated_view[2], glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        auto next_frame_index = (state.frame_in_flight + 1) % vk_res.buffer_count;
        GlobalUniforms updated_main_data = state.main_dynamic_uniforms.get();
        updated_main_data.view = rotated_view;
        updated_main_data.sun_direction = rotated_sun;
        BufferedUniform<GlobalUniforms> new_global_uniforms = state.main_dynamic_uniforms;
        new_global_uniforms.set(updated_main_data);
        new_global_uniforms.push(next_frame_index);

        SkyboxUniforms updated_skybox_data = { cam_rotation };
        BufferedUniform<SkyboxUniforms> new_skybox_uniforms = state.skybox_dynamic_uniforms;
        new_skybox_uniforms.set(updated_skybox_data);
        new_skybox_uniforms.push(next_frame_index);

        return DrawState {
            .buf_num = static_cast<uint8_t>((state.buf_num + 1u) % vk_res.buffer_count),
            .frame_num = state.frame_num + 1,
            .frame_in_flight = next_frame_index,
            .main_dynamic_uniforms = new_global_uniforms,
            .skybox_dynamic_uniforms = new_skybox_uniforms
        };
    }

    void cleanup(vk_types::Context& resources, vk_types::CleanupProcedures& cleanup_procedures) {
        vkDeviceWaitIdle(resources.device);
        cleanup_procedures.cleanup();
    }
}
