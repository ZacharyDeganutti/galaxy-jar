#include "vk_buffer.hpp"

#ifndef VK_BUFFER_PRIVATE_H
#define VK_BUFFER_PRIVATE_H
namespace vk_buffer {
    vk_types::AllocatedBuffer create_buffer(const VmaAllocator allocator, const size_t alloc_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage, vk_types::CleanupProcedures& cleanup_procedures);

    template <typename T>
    vk_types::GpuVertexAttribute upload_vertex_attribute(const VkDevice device, const VmaAllocator allocator, const std::vector<T>& attribute_data, std::vector<uint32_t>& indices, vk_types::CleanupProcedures& cleanup_procedures) {
        const size_t vertex_buffer_size = attribute_data.size() * sizeof(T);
        const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

        vk_types::GpuVertexAttribute new_attribute = {};

        //create vertex buffer
        new_attribute.vertex_buffer = create_buffer(
            allocator,
            vertex_buffer_size, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
            VMA_MEMORY_USAGE_GPU_ONLY,
            cleanup_procedures);

        //find the address of the vertex buffer
        VkBufferDeviceAddressInfo device_address_info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = new_attribute.vertex_buffer.buffer };
        new_attribute.vertex_buffer_address = vkGetBufferDeviceAddress(device, &device_address_info);

        //create index buffer
        new_attribute.index_buffer = create_buffer(
            allocator,
            index_buffer_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            cleanup_procedures);
        
        return new_attribute;
    }
}
#endif