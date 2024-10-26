#ifndef VK_BUFFER_PRIVATE_H
#define VK_BUFFER_PRIVATE_H

#include "vk_buffer.hpp"
#include "vk_layer.hpp"
#include "vk_mem_alloc.h"

namespace vk_buffer {
    vk_types::AllocatedBuffer upload_index_buffer(const vk_types::Context& context, const std::vector<uint32_t>& indices, vk_types::CleanupProcedures& cleanup_procedures);

    template <typename T>
    vk_types::GpuVertexAttribute upload_vertex_attribute(const vk_types::Context& context, const std::vector<T>& attribute_data, std::vector<uint32_t>& indices, vk_types::CleanupProcedures& cleanup_procedures) {
        const size_t vertex_buffer_size = attribute_data.size() * sizeof(T);

        vk_types::GpuVertexAttribute new_attribute = {};

        //create vertex buffer
        new_attribute.vertex_buffer = create_buffer(
            context.allocator,
            vertex_buffer_size, 
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
            VMA_MEMORY_USAGE_GPU_ONLY,
            cleanup_procedures);

        //find the address of the vertex buffer
        VkBufferDeviceAddressInfo device_address_info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = new_attribute.vertex_buffer.buffer };
        new_attribute.vertex_buffer_address = vkGetBufferDeviceAddress(context.device, &device_address_info);

        // Create a temporary staging buffer which can be used to transfer crom CPU memory to GPU memory
        vk_types::CleanupProcedures staging_buffer_lifetime = {};
        vk_types::AllocatedBuffer staging = create_buffer(
            context.allocator,
            vertex_buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VMA_MEMORY_USAGE_CPU_ONLY,
            staging_buffer_lifetime);

        void* data = nullptr; //staging.allocation->GetMappedData();
        if (vmaMapMemory(context.allocator, staging.allocation, &data) != VK_SUCCESS) {
            printf("Unable to map staging buffer during vertex attribute upload\n");
            exit(EXIT_FAILURE);
        }

        // copy vertex buffer
        memcpy(reinterpret_cast<char*>(data), attribute_data.data(), vertex_buffer_size);

        vk_layer::immediate_submit(context, [&](VkCommandBuffer cmd) {
            VkBufferCopy vertex_copy{ 0 };
            vertex_copy.dstOffset = 0;
            vertex_copy.srcOffset = 0;
            vertex_copy.size = vertex_buffer_size;

            vkCmdCopyBuffer(cmd, staging.buffer, new_attribute.vertex_buffer.buffer, 1, &vertex_copy);
        });
        vmaUnmapMemory(context.allocator, staging.allocation);
        staging_buffer_lifetime.cleanup();
        
        return new_attribute;
    }
}
#endif