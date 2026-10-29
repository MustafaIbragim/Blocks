#include "chunk_mesher.h"
#include <cmath>
#include <cstdint>
#include <stdexcept>


namespace {

// Читает свет клетки по локальным координатам чанка. Если клетка выходит
// за горизонтальную границу, переходит в соседний чанк (в том числе по
// диагонали). Возвращает false, если сосед не загружен: такие пробы
// исключаются из среднего.
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

// То же самое, но для небесного света.
bool read_sky_light_sample(const Chunk& chunk,
                           int32_t x, int32_t y, int32_t z,
                           uint8_t& light) {
    if (y < 0) {
        light = 0;
        return true;
    }
    if (y >= static_cast<int32_t>(Chunk::HEIGHT)) {
        light = 255;
        return true;
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
    light = source->sky_light_data[index];
    return true;
}

// Средний небесный свет в 8 клетках, примыкающих к вершине. Координаты
// вершины — мировые, как и позиции в BlockMeshData.
uint8_t sample_sky_light_at_vertex(const Chunk& chunk,
                                   float world_x, float world_y, float world_z) {
    const int32_t origin_x = chunk.chunk_xz.x *
                             static_cast<int32_t>(Chunk::SIZE_XZ);
    const int32_t origin_z = chunk.chunk_xz.y *
                             static_cast<int32_t>(Chunk::SIZE_XZ);

    const int32_t vx = static_cast<int32_t>(std::floor(world_x)) - origin_x;
    const int32_t vy = static_cast<int32_t>(std::floor(world_y));
    const int32_t vz = static_cast<int32_t>(std::floor(world_z)) - origin_z;

    uint32_t sum = 0;
    uint32_t count = 0;
    for (int32_t dx = -1; dx <= 0; ++dx) {
        for (int32_t dy = -1; dy <= 0; ++dy) {
            for (int32_t dz = -1; dz <= 0; ++dz) {
                uint8_t light = 0;
                if (read_sky_light_sample(chunk, vx + dx, vy + dy,
                                          vz + dz, light)) {
                    sum += light;
                    ++count;
                }
            }
        }
    }
    if (count == 0) {
        return 0;
    }
    return static_cast<uint8_t>(sum / count);
}

} // namespace

BlockNeighbor ChunkMesher::read_neighbor(
    const Chunk& chunk,
    int32_t x, int32_t y, int32_t z,
    const BlockManager& block_mgr
) {
    BlockNeighbor info;

    if (y < 0 || y >= static_cast<int32_t>(Chunk::HEIGHT)) {
        return info;
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
        return info;
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
        return info;
    }

    const uint8_t lx = static_cast<uint8_t>(local_x);
    const uint16_t ly = static_cast<uint16_t>(y);
    const uint8_t lz = static_cast<uint8_t>(local_z);

    const uint32_t index = Chunk::coords_to_global_index(lx, ly, lz);
    info.block = source->get_block_fast(lx, ly, lz, block_mgr);
    info.nbt = source->get_nbt(lx, ly, lz);
    info.is_cube = source->is_standard_cube(index);
    return info;
}

std::array<uint8_t, Block::LIGHT_CUBE_SIZE> ChunkMesher::read_light_cube(
    const Chunk& chunk,
    int32_t x, int32_t y, int32_t z
) {
    std::array<uint8_t, Block::LIGHT_CUBE_SIZE> cube{};
    std::array<bool, Block::LIGHT_CUBE_SIZE> valid{};

    uint32_t sum = 0;
    uint32_t count = 0;

    for (int32_t dx = -1; dx <= 1; ++dx) {
        for (int32_t dy = -1; dy <= 1; ++dy) {
            for (int32_t dz = -1; dz <= 1; ++dz) {
                uint8_t light = 0;
                const size_t index = Block::light_cube_index(
                    static_cast<int>(dx), static_cast<int>(dy),
                    static_cast<int>(dz));

                if (read_light_sample(chunk, x + dx, y + dy, z + dz, light)) {
                    cube[index] = light;
                    valid[index] = true;
                    sum += light;
                    ++count;
                }
            }
        }
    }

    if (count == 0) {
        return cube;
    }

    // Недоступные клетки заполняем средним доступных: так среднее по 8
    // клеткам у вершины совпадает с прежним поведением, когда отсутствующие
    // пробы просто исключались.
    const uint8_t fallback = static_cast<uint8_t>(sum / count);
    for (size_t i = 0; i < cube.size(); ++i) {
        if (!valid[i]) {
            cube[i] = fallback;
        }
    }

    return cube;
}

void ChunkMesher::append_block_geometry(
    std::shared_ptr<Mesh>& mesh,
    const BlockMeshData& geometry,
    const TextureBlocksManager& tex_blocks_mgr
) {
    if (geometry.vertices.empty() && geometry.faces.empty()) {
        return;
    }

    if (geometry.vertices.size() != geometry.faces.size() * 3) {
        throw std::runtime_error(
            "Block::build_geometry: vertices count must be faces * 3");
    }

    const uint32_t base_vertex = mesh->vertex_count();

    for (const Vertex& vertex : geometry.vertices) {
        mesh->add_vertex(vertex);
    }

    for (size_t tri = 0; tri < geometry.faces.size(); ++tri) {
        FaceData face = geometry.faces[tri];

        if (geometry.layers_are_texture_ids) {
            const uint16_t texture_id = static_cast<uint16_t>(face.layer);
            auto [layer, anim_data] =
                tex_blocks_mgr.get_layer_and_anim(texture_id);
            face.layer = static_cast<int32_t>(layer);
            face.anim_data = anim_data;
        }

        const uint32_t first = base_vertex + static_cast<uint32_t>(tri * 3);
        mesh->add_triangle(first, first + 1, first + 2, face);
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

            const uint16_t nbt = nugget.nbt_data[local_index];

            Vec3i local_coords = ChunkNugget::index_to_coords(local_index);
            uint8_t local_x = static_cast<uint8_t>(local_coords.x);
            uint8_t local_y = static_cast<uint8_t>(local_coords.y);
            uint8_t local_z = static_cast<uint8_t>(local_coords.z);

            uint16_t global_y = static_cast<uint16_t>(nugget_idx * ChunkNugget::SIZE + local_y);

            const int32_t ix = static_cast<int32_t>(local_x);
            const int32_t iy = static_cast<int32_t>(global_y);
            const int32_t iz = static_cast<int32_t>(local_z);

            const Vec3i world_pos = chunk.local_to_world(local_x, global_y, local_z);

            const std::array<BlockNeighbor, Block::NEIGHBOR_COUNT> neighbors = {
                read_neighbor(chunk, ix - 1, iy, iz, block_mgr),
                read_neighbor(chunk, ix + 1, iy, iz, block_mgr),
                read_neighbor(chunk, ix, iy - 1, iz, block_mgr),
                read_neighbor(chunk, ix, iy + 1, iz, block_mgr),
                read_neighbor(chunk, ix, iy, iz - 1, block_mgr),
                read_neighbor(chunk, ix, iy, iz + 1, block_mgr),
            };

            const std::array<uint8_t, Block::LIGHT_CUBE_SIZE> light_cube =
                read_light_cube(chunk, ix, iy, iz);

            BlockMeshData geometry = block->build_geometry(
                world_pos, nbt, neighbors, light_cube);

            // Блочный свет уже посчитан в build_geometry по кубу 3x3x3.
            // Небесный свет сэмплируем здесь: он хранится отдельно и
            // не входит в старый интерфейс Block::build_geometry.
            for (Vertex& vertex : geometry.vertices) {
                vertex.sky_light = sample_sky_light_at_vertex(
                    chunk, vertex.x, vertex.y, vertex.z);
            }

            append_block_geometry(mesh, geometry, tex_blocks_mgr);
        }
    }

    return mesh;
}
