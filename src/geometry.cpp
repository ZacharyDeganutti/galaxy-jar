#define TINYOBJLOADER_IMPLEMENTATION

#include "geometry.hpp"
#include "tiny_obj_loader.h"
#include "vk_buffer.hpp"

#include <algorithm>
#include <deque>
#include <tuple>
#include <unordered_map>

// Local declarations and such
namespace geometry {
    struct PreprocessedPiece {
        std::vector<uint32_t> position_indices;
        std::vector<uint32_t> normal_indices;
        std::vector<uint32_t> texture_coordinate_indices;
        int32_t material_index;
    };
}

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
    std::vector<PreprocessedPiece> make_pieces(std::vector<tinyobj::shape_t> shapes) {
        std::unordered_map<int32_t, PreprocessedPiece> preprocessed_piece_map;

        for(auto& shape : shapes) {
            size_t face_vertex_tracker = 0;
            for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
                int32_t material_id = shape.mesh.material_ids[face];

                // Pieces are batched by material
                auto piece_find = preprocessed_piece_map.find(material_id);
                if (piece_find == preprocessed_piece_map.end()) {
                    PreprocessedPiece new_piece = {};
                    new_piece.material_index = material_id;
                    auto insert_result = preprocessed_piece_map.insert({material_id, new_piece});
                    piece_find = insert_result.first;
                }

                // Loop over vertices in the face
                size_t face_vertices_size = size_t(shape.mesh.num_face_vertices[face]);
                for (size_t vertex = 0; vertex < face_vertices_size; ++vertex) {
                    PreprocessedPiece& current_piece = piece_find->second;
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
        std::vector<PreprocessedPiece> pieces;
        pieces.reserve(preprocessed_piece_map.size());
        for (auto& piece : preprocessed_piece_map) {
            pieces.push_back(piece.second);
        }
        return pieces;
    }

    IndexedVertexData reindex_pieces(std::vector<PreprocessedPiece>& pieces, std::vector<glm::vec3>& raw_positions, std::vector<glm::vec3>& raw_normals, std::vector<glm::vec2>& raw_texture_coordinates) {
        IndexedVertexData indexed_data = {};
        
        // We can build a map on a type with underlying floats as long as we don't modify them
        // The goal is to build a list of unique vertices and a corresponding single index buffer for them
        typedef std::tuple<glm::vec3, glm::vec3, glm::vec2> Vertex;

        // Build a hashing function for this type
        auto vertex_hash = [](const Vertex& vert) {
            glm::vec3 pos = std::get<0>(vert);
            glm::vec3 norm = std::get<1>(vert);
            glm::vec2 tex = std::get<2>(vert);

            return
                std::hash<float>()(pos.x)
                ^ (std::hash<float>()(norm.x) << 1)
                ^ (std::hash<float>()(tex.x) << 2)
                ^ (std::hash<float>()(tex.y))
                ^ (std::hash<float>()(pos.y) << 1)
                ^ (std::hash<float>()(norm.y) << 2)
                ^ (std::hash<float>()(norm.z))
                ^ (std::hash<float>()(pos.z) << 2);
        };

        // Assigned index, so we can sort the vertices after we en-settify them
        typedef size_t VertexIndex;
        // Key on vertices, and keep track of its new index
        std::unordered_map<Vertex, VertexIndex, decltype(vertex_hash)> unique_vertices;

        size_t max_pieces = pieces.size();
        auto& processed_piece_vector = indexed_data.pieces;
        processed_piece_vector.resize(max_pieces);

        // Global count of indices, increments whenever a new unique one is identified
        size_t index_tracker = 0;

        // Build up the map of vertices and populate the index buffer of each piece in the model
        for (size_t piece_idx = 0; piece_idx < max_pieces; ++piece_idx) {
            PreprocessedPiece current_piece = pieces[piece_idx];
            // All of these preprocessed index buffers should be the same length if the obj file is spec compliant. Grab the minimum length one just to be safe.
            size_t pre_index_length = std::min(std::min(current_piece.position_indices.size(), current_piece.normal_indices.size()), current_piece.texture_coordinate_indices.size());
            for(size_t pre_index = 0; pre_index < pre_index_length; ++pre_index) {
                uint32_t piece_pos_idx = current_piece.position_indices[pre_index];
                uint32_t piece_norm_idx = current_piece.normal_indices[pre_index];
                uint32_t piece_tex_idx = current_piece.texture_coordinate_indices[pre_index];
                Vertex vertex = {
                    raw_positions[piece_pos_idx],
                    raw_normals[piece_norm_idx],
                    raw_texture_coordinates[piece_tex_idx]
                };
                // Look up the vertex. If it has not been added yet, add it and assign it an index.
                std::pair<Vertex, VertexIndex> vertex_pair;
                auto found_vertex = unique_vertices.find(vertex);
                if (found_vertex == unique_vertices.end()) {
                    vertex_pair = std::pair{vertex, index_tracker};
                    unique_vertices.insert(vertex_pair);
                    ++index_tracker;
                } else {
                    vertex_pair = *found_vertex;
                }

                // Either way, add the index associated with the vertex to the processed piece's index buffer
                processed_piece_vector[piece_idx].indices.push_back(vertex_pair.second);
                // processed_piece_vector[piece_idx].material_index = current_piece.material_index;
            }
            processed_piece_vector[piece_idx].material_index = current_piece.material_index;
        }

        // Dump the unique vertices into a buffer and sort it on the associated index so that they match the order expected by the piece index buffers
        std::vector<std::pair<Vertex, VertexIndex>> vertex_index_pairs;
        vertex_index_pairs.reserve(unique_vertices.size());

        for(auto& vertex_index: unique_vertices) {
            vertex_index_pairs.push_back(vertex_index);
        }
        unique_vertices.clear();

        std::sort(vertex_index_pairs.begin(), vertex_index_pairs.end(), [](std::pair<Vertex, VertexIndex>& a, std::pair<Vertex, VertexIndex>& b){
            return a.second < b.second;
        });

        // Rip the contents of the sorted vertex list into the final cpu-side vertex buffers
        indexed_data.positions.reserve(vertex_index_pairs.size());
        indexed_data.normals.reserve(vertex_index_pairs.size());
        indexed_data.texture_coordinates.reserve(vertex_index_pairs.size());
        for (auto& vertex: vertex_index_pairs) {
            indexed_data.positions.push_back(std::get<0>(vertex.first));
            indexed_data.normals.push_back(std::get<1>(vertex.first));
            indexed_data.texture_coordinates.push_back(std::get<2>(vertex.first));
        }

        return indexed_data;
    }

    HostModel load_obj_model(std::string file_name, std::string base_path) {
        tinyobj::attrib_t attrib = {};
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;
        std::string file_path = base_path + "/" + file_name;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, file_path.c_str(), base_path.c_str());
        if(!ret) {
            throw (err);
        }

        std::vector<glm::vec3> raw_positions;
        raw_positions.reserve(attrib.vertices.size() / 3);
        for(int index = 0; index < (attrib.vertices.size()/3); ++index) {
            glm::vec3 position = glm::vec3(attrib.vertices[3 * index], attrib.vertices[(3 * index) + 1], attrib.vertices[(3 * index) + 2]);
            raw_positions.push_back(position);
        }
        
        std::vector<glm::vec3> raw_normals;
        raw_normals.reserve(attrib.normals.size() / 3);
        for(int index = 0; index < (attrib.normals.size()/3); ++index) {
            glm::vec3 normal = glm::vec3(attrib.normals[3 * index], attrib.normals[(3 * index) + 1], attrib.normals[(3 * index) + 2]);
            raw_normals.push_back(normal);
        }

        std::vector<glm::vec2> raw_texture_coordinates;
        raw_positions.reserve(attrib.texcoords.size() / 2);
        for(int index = 0; index < (attrib.texcoords.size()/2); ++index) {
            glm::vec2 coordinate = glm::vec2(attrib.texcoords[2 * index], attrib.texcoords[(2 * index) + 1]);
            raw_texture_coordinates.push_back(coordinate);
        }

        auto raw_pieces = make_pieces(shapes);
        IndexedVertexData indexed_geometry = reindex_pieces(raw_pieces, raw_positions, raw_normals, raw_texture_coordinates);

        HostModel model = {};
        model.vertex_attributes = indexed_geometry;

        // Extract material data we care about from pieces
        std::vector<std::optional<vk_image::HostImageRgba>> diffuse_textures;
        diffuse_textures.resize(indexed_geometry.pieces.size());

        std::vector<MaterialProperties> material_properties;
        material_properties.resize(indexed_geometry.pieces.size());

        for (auto& piece : indexed_geometry.pieces) {
            int32_t material_index = piece.material_index;
            auto& diffuse = materials[material_index].diffuse;
            material_properties[material_index].diffuse = glm::vec4(diffuse[0], diffuse[1], diffuse[2], 1.0f);
            std::string diffuse_texname = materials[material_index].diffuse_texname;
            if (diffuse_texname.empty()) {
                diffuse_textures[material_index] = std::nullopt;
            } else {
                diffuse_textures[material_index] = vk_image::load_rgba_image(base_path + "/" + diffuse_texname);
            }
        }

        model.materials = material_properties;
        model.diffuse_textures = diffuse_textures;

        return model;
    }

    GpuModel upload_model(vk_types::Context& context, const HostModel& host_model) {
        std::vector<vk_types::GpuMeshBuffers> mesh_resources = vk_buffer::create_mesh_buffers(context, host_model);

        const std::vector<VkDescriptorType> texture_descriptor_types = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
        VkDescriptorSetLayout texture_layout = vk_descriptors::init_descriptor_layout(context.device, VK_SHADER_STAGE_ALL_GRAPHICS, texture_descriptor_types, context.cleanup_procedures);

        // Upload textures for all materials
        VkSampler diffuse_texture_sampler = vk_image::init_linear_sampler(context);
        std::vector<VkDescriptorSet> diffuse_texture_descriptors;
        diffuse_texture_descriptors.reserve(host_model.diffuse_textures.size());
        // for (auto& diffuse_texture : host_model.diffuse_textures) {
        for (auto& piece : host_model.vertex_attributes.pieces) {
            auto& diffuse_texture = host_model.diffuse_textures[piece.material_index];
            vk_types::AllocatedImage diffuse_texture_image = {};
            if (diffuse_texture.has_value()) {
                diffuse_texture_image = vk_image::upload_rgba_image_mipmapped(context, diffuse_texture.value(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            } else {
                // TODO: Make texture fallback common so we don't need tons of random little 1 pixel image allocations.
                // Make a white texture that samples a value of 1.0 for all channels so it acts as a passthrough when multiplying against diffuse parameters.
                vk_image::HostImageRgba white_pixel_texture = vk_image::HostImageRgba{
                    .image = vk_image::HostImage {
                        .width = 1,
                        .height = 1,
                        .data = {255, 255, 255, 255}  // RGBA: Full brightness white
                    }
                };
                diffuse_texture_image = vk_image::upload_rgba_image(context, white_pixel_texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            vk_descriptors::DescriptorAllocator descriptor_allocator = {};
            VkDescriptorSet diffuse_texture_descriptor = vk_descriptors::init_combined_image_sampler_descriptors(context.device,
                diffuse_texture_image.image_view,
                diffuse_texture_sampler,
                texture_layout,
                descriptor_allocator,
                context.cleanup_procedures);
            
            diffuse_texture_descriptors.push_back(diffuse_texture_descriptor);
        }

        // Upload material properties for all materials
        std::vector<vk_types::PersistentUniformBuffer<MaterialProperties>> material_buffers;
        material_buffers.reserve(host_model.materials.size());
        for (auto& material_properties : host_model.materials) {
            vk_types::PersistentUniformBuffer<MaterialProperties> material_properties_buffer = vk_buffer::create_persistent_mapped_uniform_buffer<MaterialProperties>(context);
            material_properties_buffer.update(material_properties);
            material_buffers.push_back(material_properties_buffer);
        }

        GpuModel gpu_model = {
            mesh_resources,
            material_buffers,
            texture_layout,
            diffuse_texture_descriptors
        };

        return gpu_model;
    }
}