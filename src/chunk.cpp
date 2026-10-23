#include "chunk.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>


// ============================================================================
// ChunkNugget
// ============================================================================

ChunkNugget::ChunkNugget(Chunk* parent, uint8_t y_idx) 
    : parent(parent), local_y_index(y_idx) {
    block_ids.fill(0);
}

uint16_t ChunkNugget::coords_to_index(uint8_t x, uint8_t y, uint8_t z) {
    return static_cast<uint16_t>(x + y * SIZE + z * SIZE * SIZE);
}

Vec3i ChunkNugget::index_to_coords(uint16_t index) {
    uint8_t x = index % SIZE;
    uint8_t y = (index / SIZE) % SIZE;
    uint8_t z = index / (SIZE * SIZE);
    return Vec3i(x, y, z);
}

uint16_t ChunkNugget::get_block_id(uint8_t x, uint8_t y, uint8_t z) const {
    if (x >= SIZE || y >= SIZE || z >= SIZE) return 0;
    return block_ids[coords_to_index(x, y, z)];
}

void ChunkNugget::set_block_id(uint8_t x, uint8_t y, uint8_t z, uint16_t id) {
    if (x >= SIZE || y >= SIZE || z >= SIZE) return;
    block_ids[coords_to_index(x, y, z)] = id;
}

void ChunkNugget::set_block(uint8_t x, uint8_t y, uint8_t z, std::shared_ptr<Block> block) {
    set_block_id(x, y, z, block ? block->id : 0);
}

std::shared_ptr<Block> ChunkNugget::get_block(uint8_t x, uint8_t y, uint8_t z, const BlockManager& block_mgr) const {
    return block_mgr.get_block(get_block_id(x, y, z));
}

void ChunkNugget::set_block(const Vec3i& pos, std::shared_ptr<Block> block) {
    set_block(static_cast<uint8_t>(pos.x), static_cast<uint8_t>(pos.y), static_cast<uint8_t>(pos.z), block);
}

std::shared_ptr<Block> ChunkNugget::get_block(const Vec3i& pos, const BlockManager& block_mgr) const {
    return get_block(static_cast<uint8_t>(pos.x), static_cast<uint8_t>(pos.y), static_cast<uint8_t>(pos.z), block_mgr);
}

void ChunkNugget::add_tick_block(uint16_t local_index) {
    if (local_index >= BLOCKS_COUNT) return;
    if (std::find(tick_indices.begin(), tick_indices.end(), local_index) == tick_indices.end()) {
        tick_indices.push_back(local_index);
    }
}

void ChunkNugget::remove_tick_block(uint16_t local_index) {
    tick_indices.erase(std::remove(tick_indices.begin(), tick_indices.end(), local_index), tick_indices.end());
}

void ChunkNugget::add_slow_tick_block(uint16_t local_index) {
    if (local_index >= BLOCKS_COUNT) return;
    if (std::find(slow_tick_indices.begin(), slow_tick_indices.end(), local_index) == slow_tick_indices.end()) {
        slow_tick_indices.push_back(local_index);
    }
}

void ChunkNugget::remove_slow_tick_block(uint16_t local_index) {
    slow_tick_indices.erase(std::remove(slow_tick_indices.begin(), slow_tick_indices.end(), local_index), slow_tick_indices.end());
}

void ChunkNugget::tick_all(uint64_t tick, const BlockManager& block_mgr) {
    if (!parent) return;
    
    int32_t base_world_x = parent->chunk_xz.x * SIZE;
    int32_t base_world_y = local_y_index * SIZE;
    int32_t base_world_z = parent->chunk_xz.y * SIZE;
    
    for (uint16_t index : tick_indices) {
        uint16_t id = block_ids[index];
        if (id != 0) {
            auto block = block_mgr.get_block(id);
            Vec3i local = index_to_coords(index);
            Vec3i world_pos(base_world_x + local.x, base_world_y + local.y, base_world_z + local.z);
            block->on_tick(world_pos, tick);
        }
    }
}

void ChunkNugget::slow_tick(uint64_t global_tick, uint32_t tick_counter, const BlockManager& block_mgr) {
    if (!parent || slow_tick_indices.empty()) return;
    
    int32_t base_world_x = parent->chunk_xz.x * SIZE;
    int32_t base_world_y = local_y_index * SIZE;
    int32_t base_world_z = parent->chunk_xz.y * SIZE;
    
    uint64_t start = global_tick % tick_counter;
    for (uint64_t i = start; i < slow_tick_indices.size(); i += tick_counter) {
        uint16_t index = slow_tick_indices[i];
        uint16_t id = block_ids[index];
        if (id != 0) {
            auto block = block_mgr.get_block(id);
            Vec3i local = index_to_coords(index);
            Vec3i world_pos(base_world_x + local.x, base_world_y + local.y, base_world_z + local.z);
            block->on_slow_tick(world_pos, global_tick);
        }
    }
}


// ============================================================================
// Chunk
// ============================================================================

Chunk::Chunk() : Chunk(Vec2i(0, 0)) {}

Chunk::Chunk(const Vec2i& xz) : chunk_xz(xz) {
    horizontal_neighbors.fill(nullptr);
    
    opacity_data.fill(0);
    light_data.fill(0);
    light_queue_level.fill(0);
    light_dirty.fill(0);
    cube_flags.fill(0);
    
    for (size_t i = 0; i < NUGGETS_COUNT; ++i) {
        nuggets[i] = ChunkNugget(this, static_cast<uint8_t>(i));
    }
}

void Chunk::set_neighbor(ChunkNeighborDir dir, Chunk* neighbor) {
    horizontal_neighbors[static_cast<size_t>(dir)] = neighbor;
}

Chunk* Chunk::get_neighbor(ChunkNeighborDir dir) const {
    return horizontal_neighbors[static_cast<size_t>(dir)];
}

void Chunk::clear_neighbors() {
    horizontal_neighbors.fill(nullptr);
}

// --- Быстрый доступ (без проверок) ---

uint16_t Chunk::get_block_id(uint8_t x, uint16_t y, uint8_t z) const {
    uint8_t nugget_idx = y / ChunkNugget::SIZE;
    uint8_t local_y = y % ChunkNugget::SIZE;
    return nuggets[nugget_idx].get_block_id(x, local_y, z);
}

void Chunk::set_block_id(uint8_t x, uint16_t y, uint8_t z, uint16_t id) {
    uint8_t nugget_idx = y / ChunkNugget::SIZE;
    uint8_t local_y = y % ChunkNugget::SIZE;
    nuggets[nugget_idx].set_block_id(x, local_y, z, id);
}

std::shared_ptr<Block> Chunk::get_block_fast(uint8_t x, uint16_t y, uint8_t z, const BlockManager& block_mgr) const {
    return block_mgr.get_block(get_block_id(x, y, z));
}

// --- Доступ с проверкой границ ---

std::shared_ptr<Block> Chunk::get_block(int32_t x, int32_t y, int32_t z, const BlockManager& block_mgr) const {
    if (y < 0 || y >= HEIGHT) return block_mgr.get_block(0);
    
    if (x >= 0 && x < SIZE_XZ && z >= 0 && z < SIZE_XZ) {
        return get_block_fast(static_cast<uint8_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z), block_mgr);
    }
    
    Chunk* neighbor = nullptr;
    int32_t local_x = x, local_z = z;
    
    if (x < 0) {
        neighbor = horizontal_neighbors[static_cast<size_t>(ChunkNeighborDir::NegX)];
        local_x += SIZE_XZ;
    } else if (x >= SIZE_XZ) {
        neighbor = horizontal_neighbors[static_cast<size_t>(ChunkNeighborDir::PosX)];
        local_x -= SIZE_XZ;
    }
    
    if (z < 0) {
        neighbor = horizontal_neighbors[static_cast<size_t>(ChunkNeighborDir::NegZ)];
        local_z += SIZE_XZ;
    } else if (z >= SIZE_XZ) {
        neighbor = horizontal_neighbors[static_cast<size_t>(ChunkNeighborDir::PosZ)];
        local_z -= SIZE_XZ;
    }
    
    if (neighbor) {
        return neighbor->get_block_fast(static_cast<uint8_t>(local_x), static_cast<uint16_t>(y), static_cast<uint8_t>(local_z), block_mgr);
    }
    
    return block_mgr.get_block(0);
}

std::shared_ptr<Block> Chunk::get_block(const Vec3i& pos, const BlockManager& block_mgr) const {
    return get_block(pos.x, pos.y, pos.z, block_mgr);
}

void Chunk::set_block(uint8_t x, uint16_t y, uint8_t z, std::shared_ptr<Block> block) {
    uint32_t global_index = coords_to_global_index(x, y, z);
    uint16_t block_id = block ? block->id : 0;
    
    set_block_id(x, y, z, block_id);
    
    uint8_t opacity = block ? block->opacity : 0;
    opacity_data[global_index] = opacity;
    
    bool is_cube = block && (block->status & 0x01);
    set_standard_cube(global_index, is_cube);
    
    mark_light_dirty(global_index, true);
}

void Chunk::set_block(const Vec3i& pos, std::shared_ptr<Block> block) {
    set_block(static_cast<uint8_t>(pos.x), static_cast<uint16_t>(pos.y), static_cast<uint8_t>(pos.z), block);
}

// --- Утилиты ---

Vec3i Chunk::local_to_world(uint8_t x, uint16_t y, uint8_t z) const {
    return Vec3i(chunk_xz.x * SIZE_XZ + x, y, chunk_xz.y * SIZE_XZ + z);
}

void Chunk::tick_all(uint64_t tick, const BlockManager& block_mgr) {
    for (auto& nugget : nuggets) {
        nugget.tick_all(tick, block_mgr);
    }
}

void Chunk::slow_tick(uint64_t global_tick, uint32_t tick_counter, const BlockManager& block_mgr) {
    for (auto& nugget : nuggets) {
        nugget.slow_tick(global_tick, tick_counter, block_mgr);
    }
}


