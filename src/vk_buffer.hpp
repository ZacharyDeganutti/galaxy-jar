#ifndef VK_BUFFER_H_
#define VK_BUFFER_H_

#include "vk_types.hpp"
#include "geometry.hpp"

namespace vk_buffer {
    // Uploads model data to the GPU with a lifetime matching that of the context
    std::vector<vk_types::GpuMeshBuffers> create_mesh_buffers(vk_types::Context& context, geometry::Model model);

    // Uploads model data to the GPU with a custom lifetime
    std::vector<vk_types::GpuMeshBuffers> create_mesh_buffers(vk_types::Context& context, geometry::Model model, vk_types::CleanupProcedures& custom_lifetime);
}

#endif