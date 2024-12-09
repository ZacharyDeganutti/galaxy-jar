#ifndef GEOMETRY_H_
#define GEOMETRY_H_
#include <vector>
#include <string>
#include <optional>
#include "glmvk.hpp"
#include "vk_descriptors.hpp"
#include "vk_image.hpp"
#include "vk_types.hpp"

namespace geometry {
    enum class Direction {
        Left,
        Right,
        Up,
        Down,
        Forward,
        Back
    };

    struct AxisAlignedBasis {
        Direction x;
        Direction y;
        Direction z;
    };

    struct TexturedVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texture_coordinate;
    };

    struct MaterialProperties {
        glm::vec4 diffuse;
    };

    struct Piece {
        std::vector<uint32_t> indices;
        int32_t material_index;
    };

    struct IndexedVertexData {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texture_coordinates;
        std::vector<Piece> pieces;
    };

    struct HostModel {
        AxisAlignedBasis basis;
        IndexedVertexData vertex_attributes;
        std::vector<MaterialProperties> materials;
        std::vector<std::optional<vk_image::HostImageRgba>> diffuse_textures;
    };

    struct GpuModel {
        std::vector<vk_types::GpuMeshBuffers> vertex_buffers;
        std::vector<vk_types::PersistentUniformBuffer<MaterialProperties>> material_buffers;
        // std::vector<vk_types::AllocatedImage> diffuse_textures;
        VkDescriptorSetLayout texture_layout;
        std::vector<VkDescriptorSet> diffuse_texture_descriptors;
    };

    // Accepts a file name, and a path to search for the file and corresponding material as arguments
    // May throw an exception on failure
    HostModel load_obj_model(std::string file_name, std::string base_path, AxisAlignedBasis coordinate_system);

    GpuModel upload_model(vk_types::Context& context, const HostModel& host_model);

    glm::mat4 make_x_right_y_up_z_forward_transform(AxisAlignedBasis original_basis);
}
#endif