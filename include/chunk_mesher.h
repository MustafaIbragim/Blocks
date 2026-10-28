#pragma once

#include <array>
#include <memory>
#include "chunk.h"
#include "block.h"
#include "texture.h"
#include "mesh.h"


class ChunkMesher {
public:
    static std::shared_ptr<Mesh> build_mesh(
        const Chunk& chunk,
        const BlockManager& block_mgr,
        const TextureBlocksManager& tex_blocks_mgr
    );

private:
    // Собирает геометрию, которую вернул блок, в общий меш чанка.
    // FaceData::layer из BlockMeshData может содержать id текстуры
    // (layers_are_texture_ids == true) — тогда здесь он переводится
    // в слой массива текстур и дополняется anim_data.
    static void append_block_geometry(
        std::shared_ptr<Mesh>& mesh,
        const BlockMeshData& geometry,
        const TextureBlocksManager& tex_blocks_mgr
    );

    // Читает данные соседней клетки (блок, NBT, признак полного куба).
    static BlockNeighbor read_neighbor(
        const Chunk& chunk,
        int32_t x, int32_t y, int32_t z,
        const BlockManager& block_mgr
    );

    // Читает куб света 3x3x3 вокруг клетки (27 значений).
    // Недоступные клетки (незагруженные чанки, выход за высоту) заменяются
    // средним доступных значений, чтобы края мира не темнели.
    static std::array<uint8_t, Block::LIGHT_CUBE_SIZE> read_light_cube(
        const Chunk& chunk,
        int32_t x, int32_t y, int32_t z
    );
};
