#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include "vec.h"
#include "chunk.h"
#include "block.h"
#include "texture.h"
#include "mesh.h"
#include "light_engine.h"
#include "item.h"


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

    // Не владеющий указатель без копирования shared_ptr — для горячих путей.
    Chunk* get_chunk_ptr(const Vec2i& chunk_pos) {
        auto it = chunks.find(chunk_pos);
        return it != chunks.end() ? it->second.get() : nullptr;
    }

    const Chunk* get_chunk_ptr(const Vec2i& chunk_pos) const {
        auto it = chunks.find(chunk_pos);
        return it != chunks.end() ? it->second.get() : nullptr;
    }

    // Список позиций загруженных чанков (для физики: незагруженные чанки
    // считаются барьерами и должны создавать коллизии).
    std::vector<Vec2i> chunk_positions() const {
        std::vector<Vec2i> result;
        result.reserve(chunks.size());
        for (const auto& entry : chunks) {
            if (entry.second) {
                result.push_back(entry.first);
            }
        }
        return result;
    }
    
    void set_chunk(const Vec2i& chunk_pos, std::shared_ptr<Chunk> chunk);
    
    bool has_chunk(const Vec2i& chunk_pos) const;
    void remove_chunk(const Vec2i& chunk_pos);
    
    static Vec2i world_to_chunk_coords(const Vec3i& world_pos);
    static Vec3i world_to_local_coords(const Vec3i& world_pos);
    
    std::shared_ptr<Block> get_block(const Vec3i& world_pos) const;

    // Одновременно отдаёт блок и его NBT. Удобно для логики, которой нужен
    // не только тип блока, но и состояние конкретной клетки.
    std::shared_ptr<Block> get_block(const Vec3i& world_pos,
                                     uint16_t& nbt_out) const;

    void set_block(const Vec3i& world_pos, std::shared_ptr<Block> block);
    void remove_block(const Vec3i& world_pos);

    // 16-битный id обычного блока из BlockManager.
    uint16_t get_block_id(const Vec3i& world_pos) const;

    // NBT (16 бит) для блока в мире. Интерпретация полностью зависит от
    // типа блока; мир лишь хранит значение.
    uint16_t get_nbt(const Vec3i& world_pos) const;
    bool set_nbt(const Vec3i& world_pos, uint16_t nbt);

    // --- Items в мире (временно на уровне World) ---
    // Item-entity будет перенесён в чанки после переработки коллизий и
    // хранения сущностей (roadmap, п.3/5/8).
    uint32_t spawn_item_entity(
        const ItemStack& stack,
        const Vec3f& position,
        const Vec3f& velocity = Vec3f::zero());
    bool remove_item_entity(uint32_t entity_id);
    void clear_item_entities();
    std::shared_ptr<ItemEntity> get_item_entity(uint32_t entity_id) const;
    const std::vector<std::shared_ptr<ItemEntity>>& get_item_entities() const {
        return item_entities;
    }
    uint32_t item_entity_count() const;

    // Подбор предмета в инвентарь. Если preferred_slot задан, проверяется
    // именно эта ячейка (совпадение предмета и свободное место).
    // Возвращает true, если хотя бы часть стака была подобрана.
    bool pickup_item_entity(uint32_t entity_id, Inventory& inventory,
                            uint32_t preferred_slot = UINT32_MAX);
    bool pickup_nearest_item(const Vec3f& position, float radius,
                             Inventory& inventory,
                             uint32_t preferred_slot = UINT32_MAX);

    // Гравитация/приземление item-entity и удаление пустых сущностей.
    void update_items(float delta_time);
    
    // Тактовое обновление мира. Внутри ограничено 20 Гц.
    // Первым шагом обновляется освещение, затем пересобираются dirty-меши.
    static constexpr float UPDATE_INTERVAL = 1.0f / 20.0f;
    void update(float delta_time);

    // Освещение
    LightEngine& get_light_engine() const { return *light_engine; }
    void update_lighting();
    
    void rebuild_chunk_mesh(const Vec2i& chunk_pos);
    void rebuild_dirty_meshes();
    std::shared_ptr<Mesh> get_chunk_mesh(const Vec2i& chunk_pos) const;
    bool has_chunk_mesh(const Vec2i& chunk_pos) const;
    void mark_chunk_dirty(const Vec2i& chunk_pos);
    bool is_chunk_dirty(const Vec2i& chunk_pos) const;

    // Вызывается после пересборки меша. Позволяет автоматически
    // отправлять меш в рендерер без ручного перебора чанков из Python.
    using MeshUpdateCallback =
        std::function<void(const Vec2i&, const std::shared_ptr<Mesh>&)>;
    void set_mesh_update_callback(MeshUpdateCallback callback);
    
    void tick_all(uint64_t tick);

    // Частичное обновление блоков. Мир хранит одну перестановку всех
    // локальных индексов чанка (32*512*32) и на каждом такте рассылает
    // следующие SLOW_TICK_BATCH_SIZE индексов во ВСЕ загруженные чанки.
    // Второй аргумент оставлен для совместимости со старым API и не влияет
    // на выбор блоков.
    static constexpr uint32_t SLOW_TICK_BLOCK_COUNT = Chunk::VOLUME;
    static constexpr uint32_t SLOW_TICK_BATCH_SIZE = 32;

    void slow_tick(uint64_t global_tick, uint32_t tick_counter = 0);

    // Пересобрать перестановку и начать цикл с начала.
    void reshuffle_slow_tick_sequence();

    uint32_t get_slow_tick_cursor() const { return slow_tick_cursor; }
    void set_slow_tick_cursor(uint32_t cursor);

    const std::array<uint32_t, SLOW_TICK_BLOCK_COUNT>&
    get_slow_tick_sequence() const { return slow_tick_sequence; }

    std::array<uint32_t, SLOW_TICK_BLOCK_COUNT>&
    get_slow_tick_sequence() { return slow_tick_sequence; }
    
    BlockManager& get_block_mgr() const { return block_mgr; }
    TextureBlocksManager& get_tex_blocks_mgr() const { return tex_blocks_mgr; }
    TextureManager& get_tex_mgr() const { return tex_mgr; }

private:
    BlockManager& block_mgr;
    TextureBlocksManager& tex_blocks_mgr;
    TextureManager& tex_mgr;
    
    std::unique_ptr<LightEngine> light_engine;
    
    std::unordered_map<Vec2i, std::shared_ptr<Chunk>, Vec2iHash> chunks;
    std::unordered_map<Vec2i, std::shared_ptr<Mesh>, Vec2iHash> chunk_meshes;
    std::unordered_set<Vec2i, Vec2iHash> dirty_chunks;

    std::vector<std::shared_ptr<ItemEntity>> item_entities;
    uint32_t next_item_entity_id = 1;

    float update_accumulator = 0.0f;
    MeshUpdateCallback mesh_update_callback;

    // Одна перестановка 0..SLOW_TICK_BLOCK_COUNT-1, общая для всех чанков.
    std::array<uint32_t, SLOW_TICK_BLOCK_COUNT> slow_tick_sequence;
    uint32_t slow_tick_cursor = 0;
    uint64_t slow_tick_tick = 0;
    std::mt19937 slow_tick_rng;

    void link_horizontal_neighbors(const Vec2i& chunk_pos);
    void unlink_horizontal_neighbors(const Vec2i& chunk_pos);
    void mark_horizontal_neighbors_dirty(const Vec2i& chunk_pos);
};


