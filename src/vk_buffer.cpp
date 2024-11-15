
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

    vk_types::AllocatedBuffer upload_index_buffer(const vk_types::Context& context, const std::vector<uint32_t>& indices, vk_types::CleanupProcedures& cleanup_procedures) {
        const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

        //create index buffer
        vk_types::AllocatedBuffer index_buffer = create_buffer(
            context.allocator,
            index_buffer_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            cleanup_procedures);

        // Create a temporary staging buffer which can be used to transfer crom CPU memory to GPU memory
        vk_types::CleanupProcedures staging_buffer_lifetime = {};
        vk_types::AllocatedBuffer staging = create_buffer(
            context.allocator,
            index_buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VMA_MEMORY_USAGE_CPU_ONLY,
            staging_buffer_lifetime);

        void* data = nullptr;
        if (vmaMapMemory(context.allocator, staging.allocation, &data) != VK_SUCCESS) {
            printf("Unable to map staging buffer during vertex attribute upload\n");
            exit(EXIT_FAILURE);
        }

        // copy index buffer
        memcpy(reinterpret_cast<char*>(data), indices.data(), index_buffer_size);

        vk_layer::immediate_submit(context, [&](VkCommandBuffer cmd) {
            VkBufferCopy index_copy{ 0 };
            index_copy.dstOffset = 0;
            index_copy.srcOffset = 0;
            index_copy.size = index_buffer_size;

            vkCmdCopyBuffer(cmd, staging.buffer, index_buffer.buffer, 1, &index_copy);
        });
        vmaUnmapMemory(context.allocator, staging.allocation);
        staging_buffer_lifetime.cleanup();

        return index_buffer;
    }

    std::vector<vk_types::GpuMeshBuffers> create_mesh_buffers(vk_types::Context& context, geometry::HostModel model, vk_types::CleanupProcedures& custom_lifetime) {
        std::vector<vk_types::GpuMeshBuffers> model_meshes;
        model_meshes.reserve(model.vertex_attributes.pieces.size());

        vk_types::GpuVertexAttribute position_attribute = upload_vertex_attribute<glm::vec3>(context, model.vertex_attributes.positions, custom_lifetime);
        vk_types::GpuVertexAttribute normal_attribute = upload_vertex_attribute<glm::vec3>(context, model.vertex_attributes.normals, custom_lifetime);
        vk_types::GpuVertexAttribute texture_coordinate_attribute = upload_vertex_attribute<glm::vec2>(context, model.vertex_attributes.texture_coordinates, custom_lifetime);

        for (auto& piece : model.vertex_attributes.pieces) {
            vk_types::AllocatedBuffer index_buffer = upload_index_buffer(context, piece.indices, custom_lifetime);

            model_meshes.push_back({index_buffer, position_attribute, normal_attribute, texture_coordinate_attribute, static_cast<uint32_t>(piece.indices.size())});
        }
        
        return model_meshes;
    }

    std::vector<vk_types::GpuMeshBuffers> create_mesh_buffers(vk_types::Context& context, geometry::HostModel model) {
        return create_mesh_buffers(context, model, context.cleanup_procedures);
    }
}