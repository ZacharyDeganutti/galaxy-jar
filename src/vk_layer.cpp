#include "vk_buffer.hpp"
#include "vk_descriptors.hpp"
#include "vk_image.hpp"
#include "vk_init.hpp"
#include "vk_layer.hpp"
#include "vk_pipeline.hpp"
#include "vk_types.hpp"

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

        void draw_background(const VkCommandBuffer cmd, const vk_types::AllocatedImage& background_target, const vk_types::Pipeline& pipeline, const DrawState& state) {
            vkCmdBindPipeline(cmd, pipeline.bind_point, pipeline.handle);
            vkCmdBindDescriptorSets(cmd, pipeline.bind_point, pipeline.layout, 0, pipeline.core_descriptors.size(), pipeline.core_descriptors.data(), 0, nullptr);
            vkCmdDispatch(cmd, std::ceil(background_target.image_extent.width / 16.0), std::ceil(background_target.image_extent.height / 16.0), 1);
        }

        void draw_background_skybox(const VkCommandBuffer cmd, const vk_types::AllocatedImage& background_target, const vk_types::Pipeline& pipeline, const Drawable& cube_model, const SkyboxTexture& texture, const DrawState& state) {
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
                std::vector<VkDescriptorSet> skybox_descriptor_sets = {
                    state.skybox_dynamic_uniforms.cam_rotation.get_descriptor_set(state.frame_in_flight),
                    texture.descriptor,
                };
                vkCmdBindDescriptorSets(cmd, pipeline.bind_point, pipeline.layout, 0, skybox_descriptor_sets.size(), skybox_descriptor_sets.data(), 0, nullptr);

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

        void draw_geometry(const VkCommandBuffer cmd, const vk_types::AllocatedImage& draw_target, const vk_types::AllocatedImage& depth_buffer, const vk_types::Pipeline& pipeline, const Drawable& drawable, const DrawState& state) {
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

            // Clear out depth buffer
            // std::array<VkRenderingAttachmentInfo, 2> attachments = {color_attachment, depth_attachment};
            // std::array<VkExtent2D, 2> extents = {draw_target.image_extent, depth_buffer.image_extent};
            std::array<VkRenderingAttachmentInfo, 1> attachments = {depth_attachment};
            std::array<VkExtent2D, 1> extents = {depth_buffer.image_extent};
            clear_attachments(cmd, attachments, extents);
            
            // Draw all buffers
            for (int piece = 0; piece < drawable.gpu_model.vertex_buffers.size(); ++piece) {
                // Bind up the descriptors to match each piece
                std::vector<VkDescriptorSet> graphics_descriptor_sets = { 
                    state.main_dynamic_uniforms.view.get_descriptor_set(state.frame_in_flight), 
                    state.main_dynamic_uniforms.projection.get_descriptor_set(state.frame_in_flight), 
                    state.main_dynamic_uniforms.sun_direction.get_descriptor_set(state.frame_in_flight),
                    drawable.gpu_model.diffuse_texture_descriptors[piece],
                    drawable.gpu_model.normal_texture_descriptors[piece],
                    drawable.gpu_model.specular_texture_descriptors[piece],
                    drawable.transform.get_descriptor_set(state.frame_in_flight)
                };
                vkCmdBindDescriptorSets(cmd, pipeline.bind_point, pipeline.layout, 0, graphics_descriptor_sets.size(), graphics_descriptor_sets.data(), 0, nullptr);

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

    SkyboxTexture upload_skybox(vk_types::Context& context, const vk_image::HostImage& skybox_image, vk_types::CleanupProcedures& lifetime) {
        const std::vector<VkDescriptorType> texture_descriptor_types = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
        VkDescriptorSetLayout texture_layout = vk_descriptors::init_descriptor_layout(context.device, VK_SHADER_STAGE_ALL_GRAPHICS, texture_descriptor_types, context.cleanup_procedures);

        // Upload textures for all materials
        VkSampler texture_sampler = vk_image::init_linear_sampler(context);

        vk_types::AllocatedImage skybox_texture = {};
        skybox_texture = vk_image::upload_image(context, skybox_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, lifetime);
        
        vk_descriptors::DescriptorAllocator descriptor_allocator = {};
        VkDescriptorSet skybox_texture_descriptor = vk_descriptors::init_combined_image_sampler_descriptors(context.device,
            skybox_texture.image_view,
            texture_sampler,
            texture_layout,
            descriptor_allocator,
            lifetime);
        
        return SkyboxTexture { skybox_texture, skybox_texture_descriptor, texture_layout }; 
    }
    
    Pipelines build_pipelines(vk_types::Context& context, const std::vector<VkDescriptorSetLayout>& graphics_descriptor_layouts, const std::vector<VkDescriptorSetLayout>& skybox_descriptor_layouts, vk_types::CleanupProcedures& lifetime) {
        /// Setup descriptors for compute pipeline
        vk_descriptors::DescriptorAllocator descriptor_allocator = {};
        std::vector<VkDescriptorType> compute_descriptor_types = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
        VkDescriptorSetLayout compute_descriptor_layout = vk_descriptors::init_descriptor_layout(context.device, VK_SHADER_STAGE_COMPUTE_BIT, compute_descriptor_types, lifetime);
        VkDescriptorSet compute_descriptor_set = vk_descriptors::init_image_descriptors(context.device, context.draw_target.image_view, compute_descriptor_layout, descriptor_allocator, lifetime);

        /// Assemble the compute pipeline
        VkShaderModule compute_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/gradient.glsl.comp.spv", lifetime);
        std::vector<VkDescriptorSetLayout> compute_descriptor_set_layouts = { compute_descriptor_layout };
        VkPipelineLayout compute_pipeline_layout = vk_pipeline::init_pipeline_layout(context.device, compute_descriptor_set_layouts, lifetime);
        vk_types::Pipeline compute_pipeline = vk_pipeline::init_compute_pipeline(context.device, compute_pipeline_layout, compute_shader, { compute_descriptor_set }, lifetime);

        /// Assemble the graphics pipeline
        VkShaderModule vert_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/colored_triangle.glsl.vert.spv", lifetime);
        VkShaderModule frag_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/colored_triangle.glsl.frag.spv", lifetime);
        VkPipelineLayout graphics_pipeline_layout = vk_pipeline::init_pipeline_layout(context.device, graphics_descriptor_layouts, lifetime);
        vk_pipeline::GraphicsPipelineBuilder standard_render_pipeline_builder = vk_pipeline::GraphicsPipelineBuilder(context.device, graphics_pipeline_layout, vert_shader, frag_shader, context.draw_target.image_format, context.depth_buffer.image_format, lifetime);
        vk_types::Pipeline graphics_pipeline = standard_render_pipeline_builder.build();

        /// Assemble the skybox pipeline
        VkShaderModule skybox_vert_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/skybox.glsl.vert.spv", lifetime);
        VkShaderModule skybox_frag_shader = vk_pipeline::init_shader_module(context.device, "../../../src/shaders/skybox.glsl.frag.spv", lifetime);
        VkPipelineLayout skybox_pipeline_layout = vk_pipeline::init_pipeline_layout(context.device, skybox_descriptor_layouts, lifetime);
        vk_pipeline::GraphicsPipelineBuilder skybox_render_pipeline_builder = vk_pipeline::GraphicsPipelineBuilder(context.device, skybox_pipeline_layout, skybox_vert_shader, skybox_frag_shader, context.draw_target.image_format, VK_FORMAT_UNDEFINED, lifetime);
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
        vk_types::Pipeline skybox_pipeline = skybox_render_pipeline_builder.build();


        Pipelines pipes =  Pipelines {
            .graphics = graphics_pipeline,
            .compute = compute_pipeline,
            .skybox = skybox_pipeline,
        };

        return pipes;
    }

    GlobalUniforms build_global_uniforms(vk_types::Context& context, const size_t buffer_count, vk_types::CleanupProcedures& lifetime) {

        glm::mat4 view = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -2.2f, 10.0f)), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        view = glm::translate(view, glm::vec3(1.0f, 0.0f, 2.0f));
        BufferedUniform<glm::mat4> view_ubo = BufferedUniform<glm::mat4>(context, view, buffer_count, lifetime);
        
        glm::mat4 projection = glm::transpose(glm::perspective(45.0f, 4.0f/3.0f, 1.0f, 1000.0f));
        projection = glm::transpose(projection);
        glm::mat4 vulkan_flip = glm::mat4(  1.0f, 0.0f, 0.0f, 0.0f,
                                            0.0f, -1.0f, 0.0f, 0.0f,
                                            0.0f, 0.0f, 1.0f, 0.0f,
                                            0.0f, 0.0f, 0.0f, 1.0f);
        projection = projection * vulkan_flip;
        BufferedUniform<glm::mat4> projection_ubo = BufferedUniform<glm::mat4>(context, projection, buffer_count, lifetime);

        BufferedUniform<glm::vec4> sun_ubo = BufferedUniform<glm::vec4>(context, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f), buffer_count, lifetime);

        GlobalUniforms global_buffers = GlobalUniforms {
            view_ubo,
            projection_ubo,
            sun_ubo
        };

        return global_buffers;
    }

    SkyboxUniforms build_skybox_uniforms(vk_types::Context& context, const size_t buffer_count, vk_types::CleanupProcedures& lifetime) {
        glm::mat4 cam_rotation = glm::mat4(1.0f);
        BufferedUniform<glm::mat4> cam_rotation_ubo = BufferedUniform<glm::mat4>(context, cam_rotation, buffer_count, lifetime);

        SkyboxUniforms skybox_buffers = SkyboxUniforms {
            cam_rotation_ubo
        };

        return skybox_buffers;
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

    DrawState draw(const vk_types::Context& vk_res, const Pipelines& pipelines, const std::vector<Drawable>& drawables, const Drawable& skybox, const SkyboxTexture& skybox_texture, const DrawState& state) {
        // TODO: Fix this so that it actually does frames in flight. May need first-frame checks for each buffer slot.
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

        // Transition the acquired swapchain image into a drawable format
        // transition_image(cmd, vk_res.swapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        // Make the draw target drawable by compute shaders
        vk_image::transition_image(cmd, vk_res.draw_target.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        draw_background(cmd, vk_res.draw_target, pipelines.compute, state);

        vk_image::transition_image(cmd, vk_res.draw_target.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        draw_background_skybox(cmd, vk_res.draw_target, pipelines.skybox, skybox, skybox_texture, state);

        for (auto& drawable : drawables) {
            draw_geometry(cmd, vk_res.draw_target, vk_res.depth_buffer, pipelines.graphics, drawable, state);
        }

        // Transfer from the draw target to the swapchain
        vk_image::transition_image(cmd, vk_res.draw_target.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vk_image::transition_image(cmd, vk_res.swapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vk_image::blit_image_to_image_no_mipmap(cmd, vk_res.draw_target.image, vk_res.swapchain.images[swapchain_image_index], vk_res.draw_target.image_extent, vk_res.swapchain.extent);

        // After drawing, transition the image to presentable
        vk_image::transition_image(cmd, vk_res.swapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
        glm::mat4 rotated_view = glm::rotate(state.main_dynamic_uniforms.view.get(), glm::radians(0.01f), glm::vec3(0.0f, 1.0f, 0.0f));
        //glm::mat4 rotated_view = state.main_dynamic_uniforms.view.get();
        //glm::vec4 rotated_sun = glm::rotate(state.main_dynamic_uniforms.sun_direction.get(), glm::radians(0.01f), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec4 rotated_sun = state.main_dynamic_uniforms.sun_direction.get();

        // Face the same direction as the main rendering camera
        glm::mat4 cam_rotation = glm::mat4x4(rotated_view[0], rotated_view[1], rotated_view[2], glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        auto next_frame_index = (state.frame_in_flight + 1) % vk_res.buffer_count;
        GlobalUniforms updated_main_uniforms = state.main_dynamic_uniforms;
        updated_main_uniforms.view.set(rotated_view);
        updated_main_uniforms.view.push(next_frame_index);
        updated_main_uniforms.sun_direction.set(rotated_sun);
        updated_main_uniforms.sun_direction.push(next_frame_index);

        SkyboxUniforms updated_skybox_uniforms = state.skybox_dynamic_uniforms;
        updated_skybox_uniforms.cam_rotation.set(cam_rotation);
        updated_skybox_uniforms.cam_rotation.push(next_frame_index);

        return DrawState {
            .buf_num = static_cast<uint8_t>((state.buf_num + 1u) % vk_res.buffer_count),
            .frame_num = state.frame_num + 1,
            .frame_in_flight = next_frame_index,
            .main_dynamic_uniforms = updated_main_uniforms,
            .skybox_dynamic_uniforms = updated_skybox_uniforms
        };
    }

    void cleanup(vk_types::Context& resources, vk_types::CleanupProcedures& cleanup_procedures) {
        vkDeviceWaitIdle(resources.device);
        cleanup_procedures.cleanup();
    }
}
