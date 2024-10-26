#ifndef VK_BUFFER_H_
#define VK_BUFFER_H_

#include "vk_types.hpp"
#include "geometry.hpp"

namespace vk_buffer {
    // Generic buffer allocation function that can be used for various buffer types. Not recommended for use outside of this module.
    vk_types::AllocatedBuffer create_buffer(const VmaAllocator allocator, const size_t alloc_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage, vk_types::CleanupProcedures& cleanup_procedures);
    
    // Uploads model data to the GPU with a lifetime matching that of the context
    std::vector<vk_types::GpuMeshBuffers> create_mesh_buffers(vk_types::Context& context, geometry::Model model);

    // Uploads model data to the GPU with a custom lifetime
    std::vector<vk_types::GpuMeshBuffers> create_mesh_buffers(vk_types::Context& context, geometry::Model model, vk_types::CleanupProcedures& custom_lifetime);

    // Creates a uniform buffer with data of type T that is mapped until the provided lifetime is cleaned up
    template <class T>
    vk_types::PersistentUniformBuffer<T> create_persistent_mapped_uniform_buffer(vk_types::Context& context, vk_types::CleanupProcedures& custom_lifetime) {
        const size_t uniform_buffer_size = sizeof(T);

        vk_types::AllocatedBuffer uniforms_allocation = {};

        //create uniform buffer
        uniforms_allocation = create_buffer(
            context.allocator,
            uniform_buffer_size, 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            custom_lifetime);

        void* data = nullptr;
        if (vmaMapMemory(context.allocator, uniforms_allocation.allocation, &data) != VK_SUCCESS) {
            printf("Unable to map uniform buffer\n");
            exit(EXIT_FAILURE);
        }

        auto& allocator = context.allocator;
        auto& allocation = uniforms_allocation.allocation;
        custom_lifetime.add([allocator, allocation]() {
            vmaUnmapMemory(allocator, allocation);
        });

        vk_types::PersistentUniformBuffer<T> uniform_buffer = {
            uniforms_allocation,
            reinterpret_cast<T*>(data)
        };
        
        return uniform_buffer;
    }

    // Creates a uniform buffer with data of type T that is mapped for the lifetime of the provided context
    template <class T>
    vk_types::PersistentUniformBuffer<T> create_persistent_mapped_uniform_buffer(vk_types::Context& context) {
        return create_persistent_mapped_uniform_buffer<T>(context, context.cleanup_procedures);
    }
}

#endif