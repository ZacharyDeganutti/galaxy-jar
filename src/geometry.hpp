#ifndef GEOMETRY_H_
#define GEOMETRY_H_
#include <vector>
#include <string>
#include "glmvk.hpp"
#include "vk_types.hpp"

namespace geometry {
    struct TexturedVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texture_coordinate;
    };

    struct Material {
    };
    
    struct Piece {
        std::vector<uint32_t> position_indices;
        std::vector<uint32_t> normal_indices;
        std::vector<uint32_t> texture_coordinate_indices;
        int32_t material_index;
    };

    struct Model {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texture_coordinates;
        std::vector<Piece> pieces;
        std::vector<Material> materials;
    };

    struct GpuModel {
        std::vector<vk_types::GpuMeshBuffers> vertex_attributes;
    };

    // Accepts a file name, and a path to search for the file and corresponding material as arguments
    // May throw an exception on failure
    Model load_model(std::string file_name, std::string base_path);
}
#endif