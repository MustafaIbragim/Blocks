#include "world.h"
#include "chunk_mesher.h"
#include <stdexcept>


World::World(BlockManager& block_mgr, TextureBlocksManager& tex_blocks_mgr, TextureManager& tex_mgr)
    : block_mgr(block_mgr), tex_blocks_mgr(tex_blocks_mgr), tex_mgr(tex_mgr),
      light_engine(std::make_unique<LightEngine>(*this)) {}

std::shared_ptr<Chunk> World::get_chunk(const Vec2i& chunk_pos) {
    auto it = chunks.find(chunk_pos);
    return (it != chunks.end()) ? it->second : nullptr;
}

const Chunk* World::get_chunk(const Vec2i& chunk_pos) const {
    auto it = chunks.find(chunk_pos);
    return (it != chunks.end()) ? it->second.get() : nullptr;
}

void World::set_chunk(const Vec2i& chunk_pos, std::shared_ptr<Chunk> chunk) {
    if (!chunk) return;

    auto existing = chunks.find(chunk_pos);
    bool replaced = false;
    if (existing != chunks.end() && existing->second != chunk) {
        light_engine->forget_chunk(existing->second.get());
        replaced = true;
    }

    chunk->chunk_xz = chunk_pos;
    chunks[chunk_pos] = chunk;
    link_horizontal_neighbors(chunk_pos);
    light_engine->mark_chunk_sources(*chunk);
    light_engine->mark_chunk_border(*chunk, replaced);

    // Уже загруженные соседи должны пересчитать свет на общей границе.
    // При замене чанка свет мог уменьшиться (в старом чанке были источники),
    // при обычной загрузке — только увеличиться.
    for (ChunkNeighborDir dir : {ChunkNeighborDir::NegX, ChunkNeighborDir::PosX,
                                 ChunkNeighborDir::NegZ, ChunkNeighborDir::PosZ}) {
        Chunk* neighbor = chunk->get_neighbor(dir);
        if (neighbor) light_engine->mark_chunk_border(*neighbor, replaced);
    }
}

bool World::has_chunk(const Vec2i& chunk_pos) const {
    return chunks.find(chunk_pos) != chunks.end();
}

void World::remove_chunk(const Vec2i& chunk_pos) {
    auto it = chunks.find(chunk_pos);
    if (it != chunks.end()) {
        Chunk* removed = it->second.get();
        for (ChunkNeighborDir dir : {ChunkNeighborDir::NegX, ChunkNeighborDir::PosX,
                                     ChunkNeighborDir::NegZ, ChunkNeighborDir::PosZ}) {
            Chunk* neighbor = removed->get_neighbor(dir);
            if (neighbor) light_engine->mark_chunk_border(*neighbor, true);
        }
        light_engine->forget_chunk(removed);
    }
    unlink_horizontal_neighbors(chunk_pos);
    chunks.erase(chunk_pos);
    chunk_meshes.erase(chunk_pos);
    dirty_chunks.erase(chunk_pos);
}

Vec2i World::world_to_chunk_coords(const Vec3i& world_pos) {
    int32_t chunk_x = (world_pos.x >= 0) ? (world_pos.x >> 5) : ((world_pos.x - 31) >> 5);
    int32_t chunk_z = (world_pos.z >= 0) ? (world_pos.z >> 5) : ((world_pos.z - 31) >> 5);
    return Vec2i(chunk_x, chunk_z);
}

Vec3i World::world_to_local_coords(const Vec3i& world_pos) {
    int32_t local_x = ((world_pos.x % 32) + 32) % 32;
    int32_t local_z = ((world_pos.z % 32) + 32) % 32;
    return Vec3i(local_x, world_pos.y, local_z);
}

std::shared_ptr<Block> World::get_block(const Vec3i& world_pos) const {
    if (world_pos.y < 0 || world_pos.y >= CHUNK_HEIGHT) {
        return block_mgr.get_block(0);
    }
    
    Vec2i chunk_pos = world_to_chunk_coords(world_pos);
    const Chunk* chunk = get_chunk(chunk_pos);
    
    if (!chunk) {
        return block_mgr.get_block(0);
    }
    
    Vec3i local_pos = world_to_local_coords(world_pos);
    return chunk->get_block(static_cast<uint8_t>(local_pos.x),
                           static_cast<uint16_t>(local_pos.y),
                           static_cast<uint8_t>(local_pos.z),
                           block_mgr);
}

void World::set_block(const Vec3i& world_pos, std::shared_ptr<Block> block) {
    if (world_pos.y < 0 || world_pos.y >= CHUNK_HEIGHT) {
        return;
    }
    
    Vec2i chunk_pos = world_to_chunk_coords(world_pos);
    std::shared_ptr<Chunk> chunk = get_chunk(chunk_pos);
    
    if (!chunk) {
        throw std::runtime_error("Chunk not loaded at position");
    }
    
    Vec3i local_pos = world_to_local_coords(world_pos);
    uint8_t local_x = static_cast<uint8_t>(local_pos.x);
    uint16_t local_y = static_cast<uint16_t>(local_pos.y);
    uint8_t local_z = static_cast<uint8_t>(local_pos.z);
    uint32_t global_index = Chunk::coords_to_global_index(local_x, local_y, local_z);

    // Запоминаем оптические свойства до изменения, чтобы обновить только
    // те клетки, где свет действительно может измениться.
    uint16_t old_id = chunk->get_block_id(local_x, local_y, local_z);
    uint8_t old_opacity = chunk->opacity_data[global_index];
    uint8_t old_light = block_mgr.get_light(old_id);
    uint8_t new_light = block ? block->light : 0;

    chunk->set_block(local_x, local_y, local_z, block);
    uint8_t new_opacity = chunk->opacity_data[global_index];

    light_engine->mark_block_changed(*chunk, local_pos,
                                     old_opacity, new_opacity,
                                     old_light, new_light);
    
    mark_chunk_dirty(chunk_pos);
}

void World::remove_block(const Vec3i& world_pos) {
    set_block(world_pos, nullptr);
}

void World::rebuild_chunk_mesh(const Vec2i& chunk_pos) {
    auto it = chunks.find(chunk_pos);
    if (it == chunks.end()) return;
    
    auto mesh = ChunkMesher::build_mesh(*it->second, block_mgr, tex_blocks_mgr);
    chunk_meshes[chunk_pos] = mesh;
    dirty_chunks.erase(chunk_pos);
}

std::shared_ptr<Mesh> World::get_chunk_mesh(const Vec2i& chunk_pos) const {
    auto it = chunk_meshes.find(chunk_pos);
    return (it != chunk_meshes.end()) ? it->second : nullptr;
}

bool World::has_chunk_mesh(const Vec2i& chunk_pos) const {
    return chunk_meshes.find(chunk_pos) != chunk_meshes.end();
}

void World::mark_chunk_dirty(const Vec2i& chunk_pos) {
    dirty_chunks.insert(chunk_pos);
}

bool World::is_chunk_dirty(const Vec2i& chunk_pos) const {
    return dirty_chunks.find(chunk_pos) != dirty_chunks.end();
}

void World::tick_all(uint64_t tick) {
    for (auto& [pos, chunk] : chunks) {
        chunk->tick_all(tick, block_mgr);
    }
}

void World::slow_tick(uint64_t global_tick, uint32_t tick_counter) {
    for (auto& [pos, chunk] : chunks) {
        chunk->slow_tick(global_tick, tick_counter, block_mgr);
    }
}

void World::link_horizontal_neighbors(const Vec2i& pos) {
    auto it = chunks.find(pos);
    if (it == chunks.end()) return;
    Chunk* current = it->second.get();
    
    Vec2i directions[4] = {
        {pos.x - 1, pos.y},
        {pos.x + 1, pos.y},
        {pos.x, pos.y - 1},
        {pos.x, pos.y + 1}
    };
    
    ChunkNeighborDir dirs[4] = {
        ChunkNeighborDir::NegX,
        ChunkNeighborDir::PosX,
        ChunkNeighborDir::NegZ,
        ChunkNeighborDir::PosZ
    };
    
    ChunkNeighborDir opposite_dirs[4] = {
        ChunkNeighborDir::PosX,
        ChunkNeighborDir::NegX,
        ChunkNeighborDir::PosZ,
        ChunkNeighborDir::NegZ
    };
    
    for (int i = 0; i < 4; ++i) {
        auto neighbor_it = chunks.find(directions[i]);
        if (neighbor_it != chunks.end()) {
            Chunk* neighbor = neighbor_it->second.get();
            current->set_neighbor(dirs[i], neighbor);
            neighbor->set_neighbor(opposite_dirs[i], current);
        }
    }
}

void World::unlink_horizontal_neighbors(const Vec2i& pos) {
    auto it = chunks.find(pos);
    if (it == chunks.end()) return;
    Chunk* current = it->second.get();
    
    Vec2i directions[4] = {
        {pos.x - 1, pos.y},
        {pos.x + 1, pos.y},
        {pos.x, pos.y - 1},
        {pos.x, pos.y + 1}
    };
    
    ChunkNeighborDir opposite_dirs[4] = {
        ChunkNeighborDir::PosX,
        ChunkNeighborDir::NegX,
        ChunkNeighborDir::PosZ,
        ChunkNeighborDir::NegZ
    };
    
    for (int i = 0; i < 4; ++i) {
        auto neighbor_it = chunks.find(directions[i]);
        if (neighbor_it != chunks.end()) {
            Chunk* neighbor = neighbor_it->second.get();
            neighbor->set_neighbor(opposite_dirs[i], nullptr);
        }
    }
    
    current->clear_neighbors();
}


