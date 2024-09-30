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

namespace vk_layer
{
    struct DrawState {
        bool not_first_frame;
        uint8_t buf_num;
        uint64_t frame_num;
    };

    DrawState draw(const vk_types::Context& res, const DrawState& state);
    void cleanup(vk_types::Context& resources, vk_types::CleanupProcedures& cleanup_procedures);
} // namespace vk_layer

#endif // VK_LAYER_H_