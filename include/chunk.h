#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include "vec.h"
#include "block.h"


class Chunk;

enum class ChunkNeighborDir : uint8_t {
    NegX = 0,
    PosX = 1,
    NegZ = 2,
    PosZ = 3
};


class ChunkNugget {
public:
    static constexpr uint32_t SIZE = 32;
    static constexpr uint32_t BLOCKS_COUNT = SIZE * SIZE * SIZE;
    
    std::array<uint16_t, BLOCKS_COUNT> block_ids;
    
    std::vector<uint16_t> tick_indices;
    std::vector<uint16_t> slow_tick_indices;
    
    Chunk* parent;
    uint8_t local_y_index;
    
    explicit ChunkNugget(Chunk* parent = nullptr, uint8_t y_idx = 0);
    
    // --- Быстрый доступ по ID ---
    uint16_t get_block_id(uint8_t x, uint8_t y, uint8_t z) const;
    void set_block_id(uint8_t x, uint8_t y, uint8_t z, uint16_t id);
    
    // --- Удобный доступ через Block (с BlockManager) ---
    std::shared_ptr<Block> get_block(uint8_t x, uint8_t y, uint8_t z, const BlockManager& block_mgr) const;
    void set_block(uint8_t x, uint8_t y, uint8_t z, std::shared_ptr<Block> block);
    
    std::shared_ptr<Block> get_block(const Vec3i& pos, const BlockManager& block_mgr) const;
    void set_block(const Vec3i& pos, std::shared_ptr<Block> block);
    
    // --- Утилиты ---
    static uint16_t coords_to_index(uint8_t x, uint8_t y, uint8_t z);
    static Vec3i index_to_coords(uint16_t index);
    
    void add_tick_block(uint16_t local_index);
    void remove_tick_block(uint16_t local_index);
    void add_slow_tick_block(uint16_t local_index);
    void remove_slow_tick_block(uint16_t local_index);
    
    void tick_all(uint64_t tick, const BlockManager& block_mgr);
    void slow_tick(uint64_t global_tick, uint32_t tick_counter, const BlockManager& block_mgr);
};


class Chunk {
public:
    static constexpr uint32_t NUGGETS_COUNT = 16;
    static constexpr uint32_t SIZE_XZ = 32;
    static constexpr uint32_t HEIGHT = NUGGETS_COUNT * ChunkNugget::SIZE;
    static constexpr uint32_t VOLUME = SIZE_XZ * HEIGHT * SIZE_XZ;
    static constexpr uint32_t FLAG_WORDS = VOLUME / 64;
    
    Vec2i chunk_xz;
    std::array<ChunkNugget, NUGGETS_COUNT> nuggets;
    
    std::array<uint8_t, VOLUME>      opacity_data;
    std::array<uint8_t, VOLUME>      light_data;
    // Уровень, на котором блок стоит в очереди освещения (0 = не в очереди).
    // Нужен, чтобы при появлении более яркого пути не плодить дубликаты.
    std::array<uint8_t, VOLUME>      light_queue_level;
    std::array<uint64_t, FLAG_WORDS> light_dirty;
    std::array<uint64_t, FLAG_WORDS> cube_flags;
    
    std::array<Chunk*, 4> horizontal_neighbors;
    
    Chunk();
    explicit Chunk(const Vec2i& xz);
    
    void set_neighbor(ChunkNeighborDir dir, Chunk* neighbor);
    Chunk* get_neighbor(ChunkNeighborDir dir) const;
    void clear_neighbors();
    
    uint16_t get_block_id(uint8_t x, uint16_t y, uint8_t z) const;
    void set_block_id(uint8_t x, uint16_t y, uint8_t z, uint16_t id);
    
    std::shared_ptr<Block> get_block(int32_t x, int32_t y, int32_t z, const BlockManager& block_mgr) const;
    std::shared_ptr<Block> get_block(const Vec3i& pos, const BlockManager& block_mgr) const;
    
    std::shared_ptr<Block> get_block_fast(uint8_t x, uint16_t y, uint8_t z, const BlockManager& block_mgr) const;
    
    void set_block(uint8_t x, uint16_t y, uint8_t z, std::shared_ptr<Block> block);
    void set_block(const Vec3i& pos, std::shared_ptr<Block> block);
    
    inline uint8_t get_opacity(uint8_t x, uint16_t y, uint8_t z) const {
        return opacity_data[coords_to_global_index(x, y, z)];
    }
    
    inline void set_opacity(uint8_t x, uint16_t y, uint8_t z, uint8_t value) {
        opacity_data[coords_to_global_index(x, y, z)] = value;
    }
    
    inline uint8_t get_light(uint8_t x, uint16_t y, uint8_t z) const {
        return light_data[coords_to_global_index(x, y, z)];
    }
    
    inline void set_light(uint8_t x, uint16_t y, uint8_t z, uint8_t value) {
        light_data[coords_to_global_index(x, y, z)] = value;
    }
    
    inline bool is_light_dirty(uint32_t global_index) const {
        return (light_dirty[global_index >> 6] >> (global_index & 63)) & 1ULL;
    }
    
    inline void mark_light_dirty(uint32_t global_index, bool state) {
        uint64_t& word = light_dirty[global_index >> 6];
        uint64_t mask = 1ULL << (global_index & 63);
        if (state) word |= mask;
        else word &= ~mask;
    }
    
    inline void clear_all_light_dirty() {
        light_dirty.fill(0);
    }
    
    inline bool is_standard_cube(uint32_t global_index) const {
        return (cube_flags[global_index >> 6] >> (global_index & 63)) & 1ULL;
    }
    
    inline void set_standard_cube(uint32_t global_index, bool state) {
        uint64_t& word = cube_flags[global_index >> 6];
        uint64_t mask = 1ULL << (global_index & 63);
        if (state) word |= mask;
        else word &= ~mask;
    }
    
    inline static uint32_t coords_to_global_index(uint8_t x, uint16_t y, uint8_t z) {
        return static_cast<uint32_t>(x + y * SIZE_XZ + static_cast<uint32_t>(z) * SIZE_XZ * HEIGHT);
    }
    
    Vec3i local_to_world(uint8_t x, uint16_t y, uint8_t z) const;
    
    void tick_all(uint64_t tick, const BlockManager& block_mgr);
    void slow_tick(uint64_t global_tick, uint32_t tick_counter, const BlockManager& block_mgr);
};


