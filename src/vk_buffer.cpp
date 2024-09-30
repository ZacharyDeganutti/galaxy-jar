
#include "vk_buffer.hpp"
#include "vk_buffer_private.hpp"
#include "glmvk.hpp"

namespace vk_buffer {
    vk_types::AllocatedBuffer create_buffer(const VmaAllocator allocator, const size_t alloc_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage, vk_types::CleanupProcedures& cleanup_procedures) {
        // allocate buffer
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.pNext = nullptr;
        buffer_info.size = alloc_size;

        buffer_info.usage = usage;

        VmaAllocationCreateInfo vma_alloc_info = {};
        vma_alloc_info.usage = memory_usage;
        vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        vk_types::AllocatedBuffer new_buffer;

        // allocate the buffer
        if(vmaCreateBuffer(allocator, &buffer_info, &vma_alloc_info, &new_buffer.buffer, &new_buffer.allocation, &new_buffer.info) != VK_SUCCESS) {
            printf("Failed to allocate buffer of size %zu\n", alloc_size);
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([allocator, new_buffer]() {
            vmaDestroyBuffer(allocator, new_buffer.buffer, new_buffer.allocation);
        });

        return new_buffer;
    }

    std::vector<vk_types::GpuMeshBuffers> create_mesh_buffers(vk_types::Context& context, geometry::Model model, vk_types::CleanupProcedures& custom_lifetime) {
        std::vector<vk_types::GpuMeshBuffers> model_meshes;
        model_meshes.reserve(model.pieces.size());

        for (auto& piece : model.pieces) {
            vk_types::GpuVertexAttribute position_attribute = upload_vertex_attribute<glm::vec3>(context.device, context.allocator, model.positions, piece.position_indices, custom_lifetime);
            vk_types::GpuVertexAttribute normal_attribute = upload_vertex_attribute<glm::vec3>(context.device, context.allocator, model.normals, piece.normal_indices, custom_lifetime);
            vk_types::GpuVertexAttribute texture_coordinate_attribute = upload_vertex_attribute<glm::vec2>(context.device, context.allocator, model.texture_coordinates, piece.texture_coordinate_indices, custom_lifetime);

            model_meshes.push_back({position_attribute, normal_attribute, texture_coordinate_attribute});
        }
        
        return model_meshes;
    }

    std::vector<vk_types::GpuMeshBuffers> create_mesh_buffers(vk_types::Context& context, geometry::Model model) {
        return create_mesh_buffers(context, model, context.cleanup_procedures);
    }
}