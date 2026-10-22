#pragma once

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
    static void build_cube_faces(
        std::shared_ptr<Mesh>& mesh,
        const Chunk& chunk,
        uint8_t x, uint16_t y, uint8_t z,
        const Block& block,
        const BlockManager& block_mgr,
        const TextureBlocksManager& tex_blocks_mgr
    );
    
    static void add_face(
        std::shared_ptr<Mesh>& mesh,
        const Chunk& chunk,
        uint8_t x, uint16_t y, uint8_t z,
        uint8_t face_dir,
        uint16_t layer,
        uint8_t anim_data
    );
    
    static void append_inner_mesh(
        std::shared_ptr<Mesh>& mesh,
        const Chunk& chunk,
        const Mesh& inner,
        uint8_t x, uint16_t y, uint8_t z
    );
    
    static bool is_neighbor_cube(
        const Chunk& chunk,
        int32_t x, int32_t y, int32_t z,
        const BlockManager& block_mgr
    );
    
    static uint64_t pack_block_pos(int32_t x, int32_t y, int32_t z);
    
    static uint8_t get_vertex_light(const Chunk& chunk, int32_t vx, int32_t vy, int32_t vz);
};


