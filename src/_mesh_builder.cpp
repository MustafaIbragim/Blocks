#include "mesh_builder.h"
#include <algorithm>


uint16_t MeshBuilder::get_neighbor_block(
    int local_x, int local_y, int local_z,
    const std::array<uint16_t, BLOCKS_COUNT>& center,
    const std::array<const std::array<uint16_t, BLOCKS_COUNT>*, NEIGHBORS_COUNT>& neighbors
) {
    constexpr int SIZE = 32;
    
    if (local_x >= 0 && local_x < SIZE && 
        local_y >= 0 && local_y < SIZE && 
        local_z >= 0 && local_z < SIZE) {
        size_t idx = local_x + local_y * SIZE + local_z * SIZE * SIZE;
        return center[idx];
    }
    
    int neighbor_idx = -1;
    int nx = local_x, ny = local_y, nz = local_z;
    
    if (local_x < 0) { neighbor_idx = 0; nx = SIZE - 1; }
    else if (local_x >= SIZE) { neighbor_idx = 1; nx = 0; }
    
    if (local_y < 0) { 
        if (neighbor_idx == -1) neighbor_idx = 2; 
        else if (neighbor_idx == 0) neighbor_idx = 8;
        else if (neighbor_idx == 1) neighbor_idx = 9;
        else neighbor_idx = 2;
        ny = SIZE - 1;
    }
    else if (local_y >= SIZE) {
        if (neighbor_idx == -1) neighbor_idx = 3;
        else if (neighbor_idx == 0) neighbor_idx = 10;
        else if (neighbor_idx == 1) neighbor_idx = 11;
        else neighbor_idx = 3;
        ny = 0;
    }
    
    if (local_z < 0) {
        if (neighbor_idx == -1) neighbor_idx = 4;
        else if (neighbor_idx == 0) neighbor_idx = 12;
        else if (neighbor_idx == 1) neighbor_idx = 13;
        else if (neighbor_idx == 2) neighbor_idx = 14;
        else if (neighbor_idx == 3) neighbor_idx = 15;
        else if (neighbor_idx == 8) neighbor_idx = 18;
        else if (neighbor_idx == 9) neighbor_idx = 19;
        else if (neighbor_idx == 10) neighbor_idx = 20;
        else if (neighbor_idx == 11) neighbor_idx = 21;
        else neighbor_idx = 4;
        nz = SIZE - 1;
    }
    else if (local_z >= SIZE) {
        if (neighbor_idx == -1) neighbor_idx = 5;
        else if (neighbor_idx == 0) neighbor_idx = 16;
        else if (neighbor_idx == 1) neighbor_idx = 17;
        else if (neighbor_idx == 2) neighbor_idx = 22;
        else if (neighbor_idx == 3) neighbor_idx = 23;
        else if (neighbor_idx == 8) neighbor_idx = 24;
        else if (neighbor_idx == 9) neighbor_idx = 25;
        else if (neighbor_idx == 10) neighbor_idx = 26;
        else if (neighbor_idx == 11) neighbor_idx = 27;
        else neighbor_idx = 5;
        nz = 0;
    }
    
    if (neighbor_idx >= 0 && neighbor_idx < NEIGHBORS_COUNT && neighbors[neighbor_idx]) {
        size_t idx = nx + ny * SIZE + nz * SIZE * SIZE;
        return (*neighbors[neighbor_idx])[idx];
    }
    
    return 0;
}

std::vector<std::shared_ptr<Mesh>> MeshBuilder::build_mesh(
    const std::array<uint16_t, BLOCKS_COUNT>& center,
    const std::array<const std::array<uint16_t, BLOCKS_COUNT>*, NEIGHBORS_COUNT>& neighbors,
    const BlockManager& block_manager,
    const Vec3i& world_pos
) {
    std::unordered_map<int32_t, std::shared_ptr<Mesh>> meshes_by_layer;
    
    for (int axis = 0; axis < 3; ++axis) {
        greedy_mesh_axis(center, neighbors, block_manager, axis, world_pos.x, world_pos.y, world_pos.z, meshes_by_layer);
    }
    
    constexpr int SIZE = 32;
    for (int z = 0; z < SIZE; ++z) {
        for (int y = 0; y < SIZE; ++y) {
            for (int x = 0; x < SIZE; ++x) {
                size_t idx = x + y * SIZE + z * SIZE * SIZE;
                uint16_t block_id = center[idx];
                if (block_id == 0) continue;
                
                const Block& block = block_manager.get_block(block_id);
                if (block.inner && block.inner->vertex_count() > 0) {
                    int32_t layer_id = block.inner->layer_id;
                    
                    if (meshes_by_layer.find(layer_id) == meshes_by_layer.end()) {
                        auto mesh = std::make_shared<Mesh>();
                        mesh->layer_id = layer_id;
                        meshes_by_layer[layer_id] = mesh;
                    }
                    
                    add_block_mesh(*block.inner, 
                                  static_cast<float>(world_pos.x + x),
                                  static_cast<float>(world_pos.y + y),
                                  static_cast<float>(world_pos.z + z),
                                  *meshes_by_layer[layer_id]);
                }
            }
        }
    }
    
    std::vector<std::shared_ptr<Mesh>> result;
    result.reserve(meshes_by_layer.size());
    for (auto& pair : meshes_by_layer) {
        result.push_back(pair.second);
    }
    
    return result;
}

void MeshBuilder::greedy_mesh_axis(
    const std::array<uint16_t, BLOCKS_COUNT>& center,
    const std::array<const std::array<uint16_t, BLOCKS_COUNT>*, NEIGHBORS_COUNT>& neighbors,
    const BlockManager& block_manager,
    int axis,
    int world_x, int world_y, int world_z,
    std::unordered_map<int32_t, std::shared_ptr<Mesh>>& meshes_by_layer
) {
    constexpr int SIZE = 32;
    
    for (int dir = 0; dir < 2; ++dir) {
        int face = axis * 2 + dir;
        int opposite_face = face ^ 1;
        
        for (int z = 0; z < SIZE; ++z) {
            for (int y = 0; y < SIZE; ++y) {
                for (int x = 0; x < SIZE; ++x) {
                    size_t idx = x + y * SIZE + z * SIZE * SIZE;
                    uint16_t block_id = center[idx];
                    if (block_id == 0) continue;
                    
                    const Block& block = block_manager.get_block(block_id);
                    if (!(block.face_flags & (1 << face))) continue;
                    
                    int nx = x + (axis == 0 ? (dir == 0 ? 1 : -1) : 0);
                    int ny = y + (axis == 1 ? (dir == 0 ? 1 : -1) : 0);
                    int nz = z + (axis == 2 ? (dir == 0 ? 1 : -1) : 0);
                    
                    uint16_t neighbor_id = get_neighbor_block(nx, ny, nz, center, neighbors);
                    
                    const Block& neighbor_block = block_manager.get_block(neighbor_id);
                    if ((neighbor_block.face_flags & (1 << opposite_face)) && neighbor_block.opacity == 255) {
                        continue;
                    }
                    
                    if (!block.faces[face] || block.faces[face]->vertex_count() == 0) continue;
                    
                    int32_t layer_id = block.faces[face]->layer_id;
                    
                    if (meshes_by_layer.find(layer_id) == meshes_by_layer.end()) {
                        auto mesh = std::make_shared<Mesh>();
                        mesh->layer_id = layer_id;
                        meshes_by_layer[layer_id] = mesh;
                    }
                    
                    auto& src_mesh = *block.faces[face];
                    auto& dst_mesh = *meshes_by_layer[layer_id];
                    
                    uint32_t base_idx = static_cast<uint32_t>(dst_mesh.vertex_count());
                    
                    dst_mesh.vertices.reserve(dst_mesh.vertices.size() + src_mesh.vertices.size());
                    for (size_t i = 0; i < src_mesh.vertices.size(); i += 6) {
                        int local_int_x = static_cast<int>(std::round(src_mesh.vertices[i]));
                        int local_int_y = static_cast<int>(std::round(src_mesh.vertices[i + 1]));
                        int local_int_z = static_cast<int>(std::round(src_mesh.vertices[i + 2]));
                        
                        int world_coord_x = world_x + x + local_int_x;
                        int world_coord_y = world_y + y + local_int_y;
                        int world_coord_z = world_z + z + local_int_z;
                        
                        dst_mesh.vertices.push_back(static_cast<float>(world_coord_x));
                        dst_mesh.vertices.push_back(static_cast<float>(world_coord_y));
                        dst_mesh.vertices.push_back(static_cast<float>(world_coord_z));
                        dst_mesh.vertices.push_back(src_mesh.vertices[i + 3]);
                        dst_mesh.vertices.push_back(src_mesh.vertices[i + 4]);
                        dst_mesh.vertices.push_back(src_mesh.vertices[i + 5]);
                    }
                    
                    dst_mesh.indices.reserve(dst_mesh.indices.size() + src_mesh.indices.size());
                    for (uint32_t idx : src_mesh.indices) {
                        dst_mesh.indices.push_back(base_idx + idx);
                    }
                }
            }
        }
    }
}

void MeshBuilder::add_block_mesh(const Mesh& block_mesh, const Vec3f& offset, Mesh& final_mesh) {
    add_block_mesh(block_mesh, offset.x, offset.y, offset.z, final_mesh);
}

void MeshBuilder::add_block_mesh(const Mesh& block_mesh, float offset_x, float offset_y, float offset_z, Mesh& final_mesh) {
    uint32_t base_idx = static_cast<uint32_t>(final_mesh.vertex_count());
    
    final_mesh.vertices.reserve(final_mesh.vertices.size() + block_mesh.vertices.size());
    for (size_t i = 0; i < block_mesh.vertices.size(); i += 6) {
        int local_int_x = static_cast<int>(std::round(block_mesh.vertices[i]));
        int local_int_y = static_cast<int>(std::round(block_mesh.vertices[i + 1]));
        int local_int_z = static_cast<int>(std::round(block_mesh.vertices[i + 2]));
        
        int world_coord_x = static_cast<int>(offset_x) + local_int_x;
        int world_coord_y = static_cast<int>(offset_y) + local_int_y;
        int world_coord_z = static_cast<int>(offset_z) + local_int_z;
        
        final_mesh.vertices.push_back(static_cast<float>(world_coord_x));
        final_mesh.vertices.push_back(static_cast<float>(world_coord_y));
        final_mesh.vertices.push_back(static_cast<float>(world_coord_z));
        final_mesh.vertices.push_back(block_mesh.vertices[i + 3]);
        final_mesh.vertices.push_back(block_mesh.vertices[i + 4]);
        final_mesh.vertices.push_back(block_mesh.vertices[i + 5]);
    }
    
    final_mesh.indices.reserve(final_mesh.indices.size() + block_mesh.indices.size());
    for (uint32_t idx : block_mesh.indices) {
        final_mesh.indices.push_back(base_idx + idx);
    }
}


