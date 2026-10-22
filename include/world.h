#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "vec.h"
#include "chunk.h"
#include "block.h"
#include "texture.h"
#include "mesh.h"


struct Vec2iHash {
    size_t operator()(const Vec2i& v) const {
        return std::hash<int32_t>()(v.x) ^ (std::hash<int32_t>()(v.y) << 1);
    }
};

class World {
public:
    static constexpr uint32_t CHUNK_SIZE = Chunk::SIZE_XZ;
    static constexpr uint32_t CHUNK_HEIGHT = Chunk::HEIGHT;
    
    World(BlockManager& block_mgr, TextureBlocksManager& tex_blocks_mgr, TextureManager& tex_mgr);
    
    std::shared_ptr<Chunk> get_chunk(const Vec2i& chunk_pos);
    const Chunk* get_chunk(const Vec2i& chunk_pos) const;
    
    void set_chunk(const Vec2i& chunk_pos, std::shared_ptr<Chunk> chunk);
    
    bool has_chunk(const Vec2i& chunk_pos) const;
    void remove_chunk(const Vec2i& chunk_pos);
    
    static Vec2i world_to_chunk_coords(const Vec3i& world_pos);
    static Vec3i world_to_local_coords(const Vec3i& world_pos);
    
    std::shared_ptr<Block> get_block(const Vec3i& world_pos) const;
    void set_block(const Vec3i& world_pos, std::shared_ptr<Block> block);
    
    void rebuild_chunk_mesh(const Vec2i& chunk_pos);
    std::shared_ptr<Mesh> get_chunk_mesh(const Vec2i& chunk_pos) const;
    bool has_chunk_mesh(const Vec2i& chunk_pos) const;
    void mark_chunk_dirty(const Vec2i& chunk_pos);
    bool is_chunk_dirty(const Vec2i& chunk_pos) const;
    
    void tick_all(uint64_t tick);
    void slow_tick(uint64_t global_tick, uint32_t tick_counter);
    
    TextureBlocksManager& get_tex_blocks_mgr() const { return tex_blocks_mgr; }
    TextureManager& get_tex_mgr() const { return tex_mgr; }

private:
    BlockManager& block_mgr;
    TextureBlocksManager& tex_blocks_mgr;
    TextureManager& tex_mgr;
    
    std::unordered_map<Vec2i, std::shared_ptr<Chunk>, Vec2iHash> chunks;
    std::unordered_map<Vec2i, std::shared_ptr<Mesh>, Vec2iHash> chunk_meshes;
    std::unordered_set<Vec2i, Vec2iHash> dirty_chunks;
    
    void link_horizontal_neighbors(const Vec2i& chunk_pos);
    void unlink_horizontal_neighbors(const Vec2i& chunk_pos);
};


