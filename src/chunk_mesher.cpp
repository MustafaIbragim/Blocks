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

namespace {

// Читает свет клетки по локальным координатам чанка. Если клетка выходит
// за горизонтальную границу, переходит в соседний чанк (в том числе по
// диагонали). Возвращает false, если сосед не загружен: такие пробы
// исключаются из среднего, как и раньше на краю мира.
bool read_light_sample(const Chunk& chunk,
                       int32_t x, int32_t y, int32_t z,
                       uint8_t& light) {
    if (y < 0 || y >= static_cast<int32_t>(Chunk::HEIGHT)) {
        return false;
    }

    const Chunk* source = &chunk;
    int32_t local_x = x;
    int32_t local_z = z;

    if (local_x < 0) {
        source = source->get_neighbor(ChunkNeighborDir::NegX);
        local_x += static_cast<int32_t>(Chunk::SIZE_XZ);
    } else if (local_x >= static_cast<int32_t>(Chunk::SIZE_XZ)) {
        source = source->get_neighbor(ChunkNeighborDir::PosX);
        local_x -= static_cast<int32_t>(Chunk::SIZE_XZ);
    }

    if (!source) {
        return false;
    }

    if (local_z < 0) {
        source = source->get_neighbor(ChunkNeighborDir::NegZ);
        local_z += static_cast<int32_t>(Chunk::SIZE_XZ);
    } else if (local_z >= static_cast<int32_t>(Chunk::SIZE_XZ)) {
        source = source->get_neighbor(ChunkNeighborDir::PosZ);
        local_z -= static_cast<int32_t>(Chunk::SIZE_XZ);
    }

    if (!source ||
        local_x < 0 || local_x >= static_cast<int32_t>(Chunk::SIZE_XZ) ||
        local_z < 0 || local_z >= static_cast<int32_t>(Chunk::SIZE_XZ)) {
        return false;
    }

    const uint32_t index = Chunk::coords_to_global_index(
        static_cast<uint8_t>(local_x),
        static_cast<uint16_t>(y),
        static_cast<uint8_t>(local_z));
    light = source->light_data[index];
    return true;
}

} // namespace

uint8_t ChunkMesher::get_vertex_light(const Chunk& chunk, int32_t vx, int32_t vy, int32_t vz) {
    uint32_t total = 0;
    int count = 0;

    int32_t offsets[8][3] = {
        { 0,  0,  0}, {-1,  0,  0}, { 0, -1,  0}, { 0,  0, -1},
        {-1, -1,  0}, {-1,  0, -1}, { 0, -1, -1}, {-1, -1, -1}
    };

    for (auto& off : offsets) {
        uint8_t light = 0;
        if (read_light_sample(chunk, vx + off[0], vy + off[1], vz + off[2],
                              light)) {
            total += light;
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
    // Мировое смещение чанка
    float world_offset_x = static_cast<float>(chunk.chunk_xz.x * Chunk::SIZE_XZ);
    float world_offset_z = static_cast<float>(chunk.chunk_xz.y * Chunk::SIZE_XZ);

    float fx = static_cast<float>(x) + world_offset_x;
    float fy = static_cast<float>(y);
    float fz = static_cast<float>(z) + world_offset_z;

    int32_t ix = static_cast<int32_t>(x);
    int32_t iy = static_cast<int32_t>(y);
    int32_t iz = static_cast<int32_t>(z);

    // Углы грани: позиция, координата блока для света, uv-координаты угла.
    struct Corner { float px, py, pz; int32_t lx, ly, lz; float u, v; };
    Corner corners[4];

    switch (face_dir) {
        case 0: // -X
            corners[0] = {fx,     fy,     fz + 1.0f, ix,     iy,     iz + 1, 0.0f, 0.0f};
            corners[1] = {fx,     fy + 1, fz + 1.0f, ix,     iy + 1, iz + 1, 0.0f, 1.0f};
            corners[2] = {fx,     fy + 1, fz,        ix,     iy + 1, iz,     1.0f, 1.0f};
            corners[3] = {fx,     fy,     fz,        ix,     iy,     iz,     1.0f, 0.0f};
            break;
        case 1: // +X
            corners[0] = {fx + 1, fy,     fz,        ix + 1, iy,     iz,     0.0f, 0.0f};
            corners[1] = {fx + 1, fy + 1, fz,        ix + 1, iy + 1, iz,     0.0f, 1.0f};
            corners[2] = {fx + 1, fy + 1, fz + 1.0f, ix + 1, iy + 1, iz + 1, 1.0f, 1.0f};
            corners[3] = {fx + 1, fy,     fz + 1.0f, ix + 1, iy,     iz + 1, 1.0f, 0.0f};
            break;
        case 2: // -Y
            corners[0] = {fx,     fy,     fz,        ix,     iy,     iz,     0.0f, 0.0f};
            corners[1] = {fx + 1, fy,     fz,        ix + 1, iy,     iz,     1.0f, 0.0f};
            corners[2] = {fx + 1, fy,     fz + 1.0f, ix + 1, iy,     iz + 1, 1.0f, 1.0f};
            corners[3] = {fx,     fy,     fz + 1.0f, ix,     iy,     iz + 1, 0.0f, 1.0f};
            break;
        case 3: // +Y
            corners[0] = {fx,     fy + 1, fz,        ix,     iy + 1, iz,     0.0f, 0.0f};
            corners[1] = {fx,     fy + 1, fz + 1.0f, ix,     iy + 1, iz + 1, 0.0f, 1.0f};
            corners[2] = {fx + 1, fy + 1, fz + 1.0f, ix + 1, iy + 1, iz + 1, 1.0f, 1.0f};
            corners[3] = {fx + 1, fy + 1, fz,        ix + 1, iy + 1, iz,     1.0f, 0.0f};
            break;
        case 4: // -Z
            corners[0] = {fx,     fy,     fz,        ix,     iy,     iz,     0.0f, 0.0f};
            corners[1] = {fx,     fy + 1, fz,        ix,     iy + 1, iz,     0.0f, 1.0f};
            corners[2] = {fx + 1, fy + 1, fz,        ix + 1, iy + 1, iz,     1.0f, 1.0f};
            corners[3] = {fx + 1, fy,     fz,        ix + 1, iy,     iz,     1.0f, 0.0f};
            break;
        case 5: // +Z
            corners[0] = {fx + 1, fy,     fz + 1.0f, ix + 1, iy,     iz + 1, 0.0f, 0.0f};
            corners[1] = {fx + 1, fy + 1, fz + 1.0f, ix + 1, iy + 1, iz + 1, 0.0f, 1.0f};
            corners[2] = {fx,     fy + 1, fz + 1.0f, ix,     iy + 1, iz + 1, 1.0f, 1.0f};
            corners[3] = {fx,     fy,     fz + 1.0f, ix,     iy,     iz + 1, 1.0f, 0.0f};
            break;
        default:
            return;
    }

    // Вершины хранят только позицию и свет — один и тот же угол куба может
    // переиспользоваться несколькими гранями с разными uv/текстурами.
    uint32_t vi[4];
    for (int c = 0; c < 4; ++c) {
        Vertex vertex;
        vertex.x = corners[c].px;
        vertex.y = corners[c].py;
        vertex.z = corners[c].pz;
        vertex.block_light = get_vertex_light(chunk, corners[c].lx, corners[c].ly, corners[c].lz);
        vertex.sky_light = 0;
        vertex.padding = 0;
        vi[c] = mesh->add_vertex(vertex);
    }

    // Два треугольника грани; uv каждой вершины треугольника берётся из грани.
    FaceData f0{};
    f0.u_0 = corners[0].u; f0.v_0 = corners[0].v;
    f0.u_1 = corners[1].u; f0.v_1 = corners[1].v;
    f0.u_2 = corners[2].u; f0.v_2 = corners[2].v;
    f0.layer = static_cast<int32_t>(layer);
    f0.anim_data = anim_data;

    FaceData f1 = f0;
    f1.u_0 = corners[0].u; f1.v_0 = corners[0].v;
    f1.u_1 = corners[2].u; f1.v_1 = corners[2].v;
    f1.u_2 = corners[3].u; f1.v_2 = corners[3].v;

    mesh->add_triangle(vi[0], vi[1], vi[2], f0);
    mesh->add_triangle(vi[0], vi[2], vi[3], f1);
}


void ChunkMesher::build_cube_faces(
    std::shared_ptr<Mesh>& mesh,
    const Chunk& chunk,
    uint8_t x, uint16_t y, uint8_t z,
    const Block& block,
    uint16_t nbt,
    const BlockManager& block_mgr,
    const TextureBlocksManager& tex_blocks_mgr
) {
    int32_t ix = static_cast<int32_t>(x);
    int32_t iy = static_cast<int32_t>(y);
    int32_t iz = static_cast<int32_t>(z);

    if (!is_neighbor_cube(chunk, ix - 1, iy, iz, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(
            block.get_face_texture(0, nbt));
        add_face(mesh, chunk, x, y, z, 0, layer, anim_data);
    }

    if (!is_neighbor_cube(chunk, ix + 1, iy, iz, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(
            block.get_face_texture(1, nbt));
        add_face(mesh, chunk, x, y, z, 1, layer, anim_data);
    }

    if (!is_neighbor_cube(chunk, ix, iy - 1, iz, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(
            block.get_face_texture(2, nbt));
        add_face(mesh, chunk, x, y, z, 2, layer, anim_data);
    }

    if (!is_neighbor_cube(chunk, ix, iy + 1, iz, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(
            block.get_face_texture(3, nbt));
        add_face(mesh, chunk, x, y, z, 3, layer, anim_data);
    }

    if (!is_neighbor_cube(chunk, ix, iy, iz - 1, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(
            block.get_face_texture(4, nbt));
        add_face(mesh, chunk, x, y, z, 4, layer, anim_data);
    }

    if (!is_neighbor_cube(chunk, ix, iy, iz + 1, block_mgr)) {
        auto [layer, anim_data] = tex_blocks_mgr.get_layer_and_anim(
            block.get_face_texture(5, nbt));
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
    uint32_t base_face = mesh->face_count();

    float fx = static_cast<float>(x) + static_cast<float>(chunk.chunk_xz.x * Chunk::SIZE_XZ);
    float fy = static_cast<float>(y);
    float fz = static_cast<float>(z) + static_cast<float>(chunk.chunk_xz.y * Chunk::SIZE_XZ);

    for (const auto& v : inner.vertices) {
        Vertex shifted = v;
        shifted.x += fx;
        shifted.y += fy;
        shifted.z += fz;
        mesh->add_vertex(shifted);
    }

    for (uint32_t idx : inner.indices) {
        mesh->indices.push_back(base_vertex + idx);
    }

    for (const auto& face : inner.faces) {
        mesh->faces.push_back(face);
    }

    (void)base_face;
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

            const uint16_t nbt = nugget.nbt_data[local_index];

            Vec3i local_coords = ChunkNugget::index_to_coords(local_index);
            uint8_t local_x = static_cast<uint8_t>(local_coords.x);
            uint8_t local_y = static_cast<uint8_t>(local_coords.y);
            uint8_t local_z = static_cast<uint8_t>(local_coords.z);

            uint16_t global_y = static_cast<uint16_t>(nugget_idx * ChunkNugget::SIZE + local_y);
            uint32_t global_index = Chunk::coords_to_global_index(local_x, global_y, local_z);

            if (chunk.is_standard_cube(global_index)) {
                build_cube_faces(mesh, chunk, local_x, global_y, local_z,
                                 *block, nbt, block_mgr, tex_blocks_mgr);
            } else {
                std::shared_ptr<Mesh> inner = block->get_inner_mesh(nbt);
                if (inner) {
                    append_inner_mesh(mesh, chunk, *inner, local_x, global_y, local_z);
                }
            }
        }
    }

    return mesh;
}
