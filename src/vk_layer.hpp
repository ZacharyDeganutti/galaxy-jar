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
#include "glmvk.hpp"

namespace vk_layer
{
    struct Buffers {
        vk_types::PersistentUniformBuffer<glm::mat4> modelview_ubo;
        vk_types::PersistentUniformBuffer<glm::vec4> brightness_ubo;
    };

    struct Pipelines {
        vk_types::Pipeline graphics;
        vk_types::Pipeline compute;
    };

    struct DrawState {
        bool not_first_frame;
        uint8_t buf_num;
        uint64_t frame_num;
        Buffers buffers;
    };

    struct RenderResources {
        Pipelines pipelines;
        Buffers buffers;
    };

    RenderResources build_render_resources(vk_types::Context& context, vk_types::CleanupProcedures& lifetime);
    void immediate_submit(const vk_types::Context& res, std::function<void(VkCommandBuffer cmd)>&& function);
    DrawState draw(const vk_types::Context& res, const Pipelines& pipelines, const std::vector<vk_types::GpuMeshBuffers>& buffers, const DrawState& state);
    void cleanup(vk_types::Context& resources, vk_types::CleanupProcedures& cleanup_procedures);
} // namespace vk_layer

#endif // VK_LAYER_H_