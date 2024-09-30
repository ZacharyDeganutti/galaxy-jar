#define TINYOBJLOADER_IMPLEMENTATION

#include "geometry.hpp"
#include "tiny_obj_loader.h"

#include <unordered_map>

namespace geometry {

    std::vector<TexturedVertex> zip_up_obj(tinyobj::attrib_t& attributes) {
        std::vector<TexturedVertex> verts;
        verts.reserve(static_cast<size_t>(attributes.vertices.size() / 3));

        for(int index = 0; index < attributes.vertices.size(); ++index) {
            TexturedVertex vert = {};
            vert.position = glm::vec3(attributes.vertices[3 * index], attributes.vertices[(3 * index) + 1], attributes.vertices[(3 * index) + 2]);
            vert.normal = glm::vec3(attributes.normals[3 * index], attributes.normals[(3 * index) + 1], attributes.normals[(3 * index) + 2]);
            vert.texture_coordinate = glm::vec2(attributes.texcoords[2 * index], attributes.texcoords[(2 * index) + 1]);

            verts.push_back(vert);
        }

        return verts;
    }

    // Group geometry by material (?)
    std::vector<Piece> make_pieces(std::vector<tinyobj::shape_t> shapes) {
        std::unordered_map<int32_t, Piece> piece_map;

        for(auto& shape : shapes) {
            size_t face_vertex_tracker = 0;
            for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
                int32_t material_id = shape.mesh.material_ids[face];

                // Pieces are batched by material
                auto piece_find = piece_map.find(material_id);
                if (piece_find == piece_map.end()) {
                    Piece new_piece = {};
                    new_piece.material_index = material_id;
                    auto insert_result = piece_map.insert({material_id, new_piece});
                    piece_find = insert_result.first;
                }

                // Loop over vertices in the face
                size_t face_vertices_size = size_t(shape.mesh.num_face_vertices[face]);
                for (size_t vertex = 0; vertex < face_vertices_size; ++vertex) {
                    Piece& current_piece = piece_find->second;
                    // Indices of individual vertex
                    tinyobj::index_t idx = shape.mesh.indices[face_vertex_tracker + vertex];
                    current_piece.position_indices.push_back(idx.vertex_index);

                    // Check if `normal_index` of a vertex is zero or positive. negative = no normal data
                    if (idx.normal_index >= 0) {
                        current_piece.normal_indices.push_back(idx.normal_index);
                    }

                    // Check if `texcoord_index` of a vertex is zero or positive. negative = no texcoord data
                    if (idx.texcoord_index >= 0) {
                        current_piece.texture_coordinate_indices.push_back(idx.texcoord_index);
                    }
                }
                face_vertex_tracker += face_vertices_size;
            }
        }
        std::vector<Piece> pieces;
        pieces.reserve(piece_map.size());
        for (auto& piece : piece_map) {
            pieces.push_back(piece.second);
        }
        return pieces;
    }

    Model load_model(std::string file_name, std::string base_path) {
        tinyobj::attrib_t attrib = {};
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;
        std::string file_path = base_path + "/" + file_name;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, file_path.c_str(), base_path.c_str());
        if(!ret) {
            throw (err);
        }

        std::vector<glm::vec3> positions;
        positions.reserve(attrib.vertices.size() / 3);
        for(int index = 0; index < (attrib.vertices.size()/3); ++index) {
            glm::vec3 position = glm::vec3(attrib.vertices[3 * index], attrib.vertices[(3 * index) + 1], attrib.vertices[(3 * index) + 2]);
            positions.push_back(position);
        }
        
        std::vector<glm::vec3> normals;
        normals.reserve(attrib.normals.size() / 3);
        for(int index = 0; index < (attrib.normals.size()/3); ++index) {
            glm::vec3 normal = glm::vec3(attrib.normals[3 * index], attrib.normals[(3 * index) + 1], attrib.normals[(3 * index) + 2]);
            normals.push_back(normal);
        }

        std::vector<glm::vec2> texture_coordinates;
        positions.reserve(attrib.texcoords.size() / 2);
        for(int index = 0; index < (attrib.texcoords.size()/2); ++index) {
            glm::vec2 coordinate = glm::vec2(attrib.texcoords[2 * index], attrib.texcoords[(2 * index) + 1]);
            texture_coordinates.push_back(coordinate);
        }

        auto pieces = make_pieces(shapes);

        Model model = {};
        model.normals = normals;
        model.positions = positions;
        model.texture_coordinates = texture_coordinates;
        model.pieces = pieces;
        // TODO: define materials
        return model;
    }
}