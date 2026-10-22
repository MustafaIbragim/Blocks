#include "chunk_mesher.h"
#include <cstdint>


uint64_t ChunkMesher::pack_block_pos(int32_t x, int32_t y, int32_t z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x) & 0x00FFFFFF) << 40) |
           (static_cast<uint64_t>(static_cast<uint32_t>(y) & 0x0000FFFF) << 24) |
           (static_cast<uint64_t>(static_cast<uint32_t>(z) & 0x00FFFFFF));
}

bool ChunkMesher::is_neighbor_cube(
    const Chunk& chunk,
    int32_t x, int32_t y, int32_t z,
    const BlockManager& block_mgr
) {
    if (y < 0 || y >= Chunk::HEIGHT) return false;
    
    if (x >= 0 && x < Chunk::SIZE_XZ && z >= 0 && z < Chunk::SIZE_XZ) {
        uint32_t global_index = Chunk::coords_to_global_index(
            static_cast<uint8_t>(x),
            static_cast<uint16_t>(y),
            static_cast<uint8_t>(z)
        );
        return chunk.is_standard_cube(global_index);
    }
    
    const Chunk* neighbor = nullptr;
    int32_t local_x = x, local_z = z;
    
    if (x < 0) {
        neighbor = chunk.get_neighbor(ChunkNeighborDir::NegX);
        local_x += Chunk::SIZE_XZ;
    } else if (x >= Chunk::SIZE_XZ) {
        neighbor = chunk.get_neighbor(ChunkNeighborDir::PosX);
        local_x -= Chunk::SIZE_XZ;
    }
    
    if (z < 0) {
        neighbor = chunk.get_neighbor(ChunkNeighborDir::NegZ);
        local_z += Chunk::SIZE_XZ;
    } else if (z >= Chunk::SIZE_XZ) {
        neighbor = chunk.get_neighbor(ChunkNeighborDir::PosZ);
        local_z -= Chunk::SIZE_XZ;
    }
    
    if (neighbor) {
        uint32_t global_index = Chunk::coords_to_global_index(
            static_cast<uint8_t>(local_x),
            static_cast<uint16_t>(y),
            static_cast<uint8_t>(local_z)
        );
        return neighbor->is_standard_cube(global_index);
    }
    
    return false;
}

uint8_t ChunkMesher::get_vertex_light(const Chunk& chunk, int32_t vx, int32_t vy, int32_t vz) {
    uint32_t total = 0;
    int count = 0;
    
    int32_t offsets[8][3] = {
        { 0,  0,  0}, {-1,  0,  0}, { 0, -1,  0}, { 0,  0, -1},
        {-1, -1,  0}, {-1,  0, -1}, { 0, -1, -1}, {-1, -1, -1}
    };
    
    for (auto& off : offsets) {
        int32_t x = vx + off[0];
        int32_t y = vy + off[1];
        int32_t z = vz + off[2];
        
        if (y >= 0 && y < Chunk::HEIGHT && x >= 0 && x < Chunk::SIZE_XZ && z >= 0 && z < Chunk::SIZE_XZ) {
            uint32_t idx = Chunk::coords_to_global_index(
                static_cast<uint8_t>(x), 
                static_cast<uint16_t>(y), 
                static_cast<uint8_t>(z)
            );
            total += chunk.light_data[idx];
            count++;
        }
    }
    
    return count > 0 ? static_cast<uint8_t>(total / count) : 0;
}

void ChunkMesher::add_face(
    std::shared_ptr<Mesh>& mesh,
    const Chunk& chunk,
    uint8_t x, uint16_t y, uint8_t z,
    uint8_t face_dir,
    uint16_t layer,
    uint8_t anim_data
) {
    // Вычисляем мировое смещение чанка
    float world_offset_x = static_cast<float>(chunk.chunk_xz.x * Chunk::SIZE_XZ);
    float world_offset_z = static_cast<float>(chunk.chunk_xz.y * Chunk::SIZE_XZ);
    
    // Добавляем смещение к локальным координатам
    float fx = static_cast<float>(x) + world_offset_x;
    float fy = static_cast<float>(y);
    float fz = static_cast<float>(z) + world_offset_z;
    
    int32_t ix = static_cast<int32_t>(x);
    int32_t iy = static_cast<int32_t>(y);
    int32_t iz = static_cast<int32_t>(z);
    
    uint32_t i0, i1, i2, i3;
    uint8_t l0, l1, l2, l3; // Свет для каждой вершины
    
    switch (face_dir) {
        case 0: // -X
            l0 = get_vertex_light(chunk, x, y, z + 1);
            l1 = get_vertex_light(chunk, x, y + 1, z + 1);
            l2 = get_vertex_light(chunk, x, y + 1, z);
            l3 = get_vertex_light(chunk, x, y, z);
            i0 = mesh->add_vertex(fx, fy, fz + 1.0f, 0.0f, 0.0f, layer, anim_data, l0);
            i1 = mesh->add_vertex(fx, fy + 1.0f, fz + 1.0f, 0.0f, 1.0f, layer, anim_data, l1);
            i2 = mesh->add_vertex(fx, fy + 1.0f, fz, 1.0f, 1.0f, layer, anim_data, l2);
            i3 = mesh->add_vertex(fx, fy, fz, 1.0f, 0.0f, layer, anim_data, l3);
            break;
        case 1: // +X
            l0 = get_vertex_light(chunk, x + 1, y, z);
            l1 = get_vertex_light(chunk, x + 1, y + 1, z);
            l2 = get_vertex_light(chunk, x + 1, y + 1, z + 1);
            l3 = get_vertex_light(chunk, x + 1, y, z + 1);
            i0 = mesh->add_vertex(fx + 1.0f, fy, fz, 0.0f, 0.0f, layer, anim_data, l0);
            i1 = mesh->add_vertex(fx + 1.0f, fy + 1.0f, fz, 0.0f, 1.0f, layer, anim_data, l1);
            i2 = mesh->add_vertex(fx + 1.0f, fy + 1.0f, fz + 1.0f, 1.0f, 1.0f, layer, anim_data, l2);
            i3 = mesh->add_vertex(fx + 1.0f, fy, fz + 1.0f, 1.0f, 0.0f, layer, anim_data, l3);
            break;
        case 2: // -Y
            l0 = get_vertex_light(chunk, x, y, z);
            l1 = get_vertex_light(chunk, x + 1, y, z);
            l2 = get_vertex_light(chunk, x + 1, y, z + 1);
            l3 = get_vertex_light(chunk, x, y, z + 1);
            i0 = mesh->add_vertex(fx, fy, fz, 0.0f, 0.0f, layer, anim_data, l0);
            i1 = mesh->add_vertex(fx + 1.0f, fy, fz, 1.0f, 0.0f, layer, anim_data, l1);
            i2 = mesh->add_vertex(fx + 1.0f, fy, fz + 1.0f, 1.0f, 1.0f, layer, anim_data, l2);
            i3 = mesh->add_vertex(fx, fy, fz + 1.0f, 0.0f, 1.0f, layer, anim_data, l3);
            break;
        case 3: // +Y
            l0 = get_vertex_light(chunk, x, y + 1, z);
            l1 = get_vertex_light(chunk, x, y + 1, z + 1);
            l2 = get_vertex_light(chunk, x + 1, y + 1, z + 1);
            l3 = get_vertex_light(chunk, x + 1, y + 1, z);
            i0 = mesh->add_vertex(fx, fy + 1.0f, fz, 0.0f, 0.0f, layer, anim_data, l0);
            i1 = mesh->add_vertex(fx, fy + 1.0f, fz + 1.0f, 0.0f, 1.0f, layer, anim_data, l1);
            i2 = mesh->add_vertex(fx + 1.0f, fy + 1.0f, fz + 1.0f, 1.0f, 1.0f, layer, anim_data, l2);
            i3 = mesh->add_vertex(fx + 1.0f, fy + 1.0f, fz, 1.0f, 0.0f, layer, anim_data, l3);
            break;
        case 4: // -Z
            l0 = get_vertex_light(chunk, x, y, z);
            l1 = get_vertex_light(chunk, x, y + 1, z);
            l2 = get_vertex_light(chunk, x + 1, y + 1, z);
            l3 = get_vertex_light(chunk, x + 1, y, z);
            i0 = mesh->add_vertex(fx, fy, fz, 0.0f, 0.0f, layer, anim_data, l0);
            i1 = mesh->add_vertex(fx, fy + 1.0f, fz, 0.0f, 1.0f, layer, anim_data, l1);
            i2 = mesh->add_vertex(fx + 1.0f, fy + 1.0f, fz, 1.0f, 1.0f, layer, anim_data, l2);
            i3 = mesh->add_vertex(fx + 1.0f, fy, fz, 1.0f, 0.0f, layer, anim_data, l3);
            break;
        case 5: // +Z (задняя грань)
            l0 = get_vertex_light(chunk, ix + 1, iy, iz + 1);
            l1 = get_vertex_light(chunk, ix + 1, iy + 1, iz + 1);
            l2 = get_vertex_light(chunk, ix, iy + 1, iz + 1);
            l3 = get_vertex_light(chunk, ix, iy, iz + 1);
            i0 = mesh->add_vertex(fx + 1.0f, fy, fz + 1.0f, 0.0f, 0.0f, layer, anim_data, l0);
            i1 = mesh->add_vertex(fx + 1.0f, fy + 1.0f, fz + 1.0f, 0.0f, 1.0f, layer, anim_data, l1);
            i2 = mesh->add_vertex(fx, fy + 1.0f, fz + 1.0f, 1.0f, 1.0f, layer, anim_data, l2);
            i3 = mesh->add_vertex(fx, fy, fz + 1.0f, 1.0f, 0.0f, layer, anim_data, l3);
            break;
        default:
            return;
    }
    
    mesh->add_triangle(i0, i1, i2);
    mesh->add_triangle(i0, i2, i3);
}


void ChunkMesher::build_cube_faces(
    std::shared_ptr<Mesh>& mesh,
    const Chunk& chunk,
    uint8_t x, uint16_t y, uint8_t z,
    const Block& block,
    const BlockManager& block_mgr,
    const TextureBlocksManager& tex_blocks_mgr
) {
    int32_t ix = static_cast<int32_t>(x);
    int32_t iy = static_cast<int32_t>(y);
    int32_t iz = static_cast<int32_t>(z);
    
    if (!is_neighbor_cube(chunk, ix - 1, iy, iz, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(block.face_textures[0]);
        add_face(mesh, chunk, x, y, z, 0, layer, anim_data);
    }
    
    if (!is_neighbor_cube(chunk, ix + 1, iy, iz, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(block.face_textures[1]);
        add_face(mesh, chunk, x, y, z, 1, layer, anim_data);
    }
    
    if (!is_neighbor_cube(chunk, ix, iy - 1, iz, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(block.face_textures[2]);
        add_face(mesh, chunk, x, y, z, 2, layer, anim_data);
    }
    
    if (!is_neighbor_cube(chunk, ix, iy + 1, iz, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(block.face_textures[3]);
        add_face(mesh, chunk, x, y, z, 3, layer, anim_data);
    }
    
    if (!is_neighbor_cube(chunk, ix, iy, iz - 1, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(block.face_textures[4]);
        add_face(mesh, chunk, x, y, z, 4, layer, anim_data);
    }
    
    if (!is_neighbor_cube(chunk, ix, iy, iz + 1, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(block.face_textures[5]);
        add_face(mesh, chunk, x, y, z, 5, layer, anim_data);
    }
}


void ChunkMesher::append_inner_mesh(
    std::shared_ptr<Mesh>& mesh,
    const Chunk& chunk,
    const Mesh& inner,
    uint8_t x, uint16_t y, uint8_t z
) {
    uint32_t base_vertex = mesh->vertex_count();
    
    float fx = static_cast<float>(x) + static_cast<float>(chunk.chunk_xz.x * Chunk::SIZE_XZ);
    float fy = static_cast<float>(y);
    float fz = static_cast<float>(z) + static_cast<float>(chunk.chunk_xz.y * Chunk::SIZE_XZ);
    
    for (const auto& v : inner.vertices) {
        mesh->add_vertex(
            v.x + fx, v.y + fy, v.z + fz,
            v.u, v.v,
            v.layer, v.anim_data,
            v.block_light, v.sky_light
        );
    }
    
    for (uint32_t idx : inner.indices) {
        mesh->indices.push_back(base_vertex + idx);
    }
}


std::shared_ptr<Mesh> ChunkMesher::build_mesh(
    const Chunk& chunk,
    const BlockManager& block_mgr,
    const TextureBlocksManager& tex_blocks_mgr
) {
    auto mesh = std::make_shared<Mesh>();
    
    for (uint8_t nugget_idx = 0; nugget_idx < Chunk::NUGGETS_COUNT; ++nugget_idx) {
        const auto& nugget = chunk.nuggets[nugget_idx];
        
        for (uint16_t local_index = 0; local_index < ChunkNugget::BLOCKS_COUNT; ++local_index) {
            uint16_t block_id = nugget.block_ids[local_index];
            if (block_id == 0) continue;
            
            auto block = block_mgr.get_block(block_id);
            if (!block) continue;
            
            Vec3i local_coords = ChunkNugget::index_to_coords(local_index);
            uint8_t local_x = static_cast<uint8_t>(local_coords.x);
            uint8_t local_y = static_cast<uint8_t>(local_coords.y);
            uint8_t local_z = static_cast<uint8_t>(local_coords.z);
            
            uint16_t global_y = static_cast<uint16_t>(nugget_idx * ChunkNugget::SIZE + local_y);
            uint32_t global_index = Chunk::coords_to_global_index(local_x, global_y, local_z);
            
            if (chunk.is_standard_cube(global_index)) {
                build_cube_faces(mesh, chunk, local_x, global_y, local_z, *block, block_mgr, tex_blocks_mgr);
            } else {
                if (block->inner) {
                    append_inner_mesh(mesh, chunk, *block->inner, local_x, global_y, local_z);
                }
            }
        }
    }
    
    return mesh;
}


