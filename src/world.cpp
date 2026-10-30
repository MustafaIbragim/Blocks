#include "world.h"
#include "chunk_mesher.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>


World::World(BlockManager& block_mgr, TextureBlocksManager& tex_blocks_mgr, TextureManager& tex_mgr)
    : block_mgr(block_mgr), tex_blocks_mgr(tex_blocks_mgr), tex_mgr(tex_mgr),
      light_engine(std::make_unique<LightEngine>(*this)),
      slow_tick_rng(std::random_device{}()) {
    reshuffle_slow_tick_sequence();
}

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

    // Новый/заменённый чанк и его соседи должны пересобрать меши.
    mark_chunk_dirty(chunk_pos);
    mark_horizontal_neighbors_dirty(chunk_pos);
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
    mark_horizontal_neighbors_dirty(chunk_pos);
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
    
    // Незагруженный чанк — барьер, а не воздух: рейкаст и логика движения
    // не должны «вылетать» за пределы загруженной части мира.
    if (!chunk) {
        return block_mgr.get_barrier_block();
    }
    
    Vec3i local_pos = world_to_local_coords(world_pos);
    return chunk->get_block(static_cast<uint8_t>(local_pos.x),
                           static_cast<uint16_t>(local_pos.y),
                           static_cast<uint8_t>(local_pos.z),
                           block_mgr);
}

std::shared_ptr<Block> World::get_block(const Vec3i& world_pos,
                                        uint16_t& nbt_out) const {
    nbt_out = 0;
    if (world_pos.y < 0 || world_pos.y >= CHUNK_HEIGHT) {
        return block_mgr.get_block(0);
    }

    Vec2i chunk_pos = world_to_chunk_coords(world_pos);
    const Chunk* chunk = get_chunk(chunk_pos);
    if (!chunk) {
        return block_mgr.get_barrier_block();
    }

    Vec3i local_pos = world_to_local_coords(world_pos);
    nbt_out = chunk->get_nbt(static_cast<uint8_t>(local_pos.x),
                             static_cast<uint16_t>(local_pos.y),
                             static_cast<uint8_t>(local_pos.z));
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
    
    // Незагруженные чанки — барьеры: молча игнорируем установку блока
    // вместо выброса исключения (вылет на границах мира при попытке
    // поставить блок за пределами загруженной части).
    if (!chunk) {
        return;
    }
    
    Vec3i local_pos = world_to_local_coords(world_pos);
    uint8_t local_x = static_cast<uint8_t>(local_pos.x);
    uint16_t local_y = static_cast<uint16_t>(local_pos.y);
    uint8_t local_z = static_cast<uint8_t>(local_pos.z);

    uint32_t global_index = Chunk::coords_to_global_index(local_x, local_y, local_z);

    // Запоминаем оптические свойства до изменения, чтобы обновить только
    // те клетки, где свет действительно может измениться.
    std::shared_ptr<Block> old_block = chunk->get_block(
        local_x, local_y, local_z, block_mgr);
    uint8_t old_opacity = chunk->opacity_data[global_index];
    uint8_t old_light = old_block ? old_block->light : 0;
    uint8_t new_light = block ? block->light : 0;

    chunk->set_block(local_x, local_y, local_z, block);
    uint8_t new_opacity = chunk->opacity_data[global_index];

    light_engine->mark_block_changed(*chunk, local_pos,
                                     old_opacity, new_opacity,
                                     old_light, new_light);
    
    mark_chunk_dirty(chunk_pos);

    // На границе чанка соседний меш зависит от этого блока (culling граней).
    if (local_x == 0) mark_chunk_dirty(Vec2i(chunk_pos.x - 1, chunk_pos.y));
    if (local_x == Chunk::SIZE_XZ - 1) mark_chunk_dirty(Vec2i(chunk_pos.x + 1, chunk_pos.y));
    if (local_z == 0) mark_chunk_dirty(Vec2i(chunk_pos.x, chunk_pos.y - 1));
    if (local_z == Chunk::SIZE_XZ - 1) mark_chunk_dirty(Vec2i(chunk_pos.x, chunk_pos.y + 1));
}

void World::remove_block(const Vec3i& world_pos) {
    set_block(world_pos, nullptr);
}

uint16_t World::get_block_id(const Vec3i& world_pos) const {
    if (world_pos.y < 0 || world_pos.y >= CHUNK_HEIGHT) {
        return 0;
    }
    Vec2i chunk_pos = world_to_chunk_coords(world_pos);
    const Chunk* chunk = get_chunk(chunk_pos);
    if (!chunk) {
        return 0;
    }
    Vec3i local_pos = world_to_local_coords(world_pos);
    return chunk->get_block_id(static_cast<uint8_t>(local_pos.x),
                               static_cast<uint16_t>(local_pos.y),
                               static_cast<uint8_t>(local_pos.z));
}

uint16_t World::get_nbt(const Vec3i& world_pos) const {
    if (world_pos.y < 0 || world_pos.y >= CHUNK_HEIGHT) {
        return 0;
    }
    Vec2i chunk_pos = world_to_chunk_coords(world_pos);
    const Chunk* chunk = get_chunk(chunk_pos);
    if (!chunk) {
        return 0;
    }
    Vec3i local_pos = world_to_local_coords(world_pos);
    return chunk->get_nbt(static_cast<uint8_t>(local_pos.x),
                          static_cast<uint16_t>(local_pos.y),
                          static_cast<uint8_t>(local_pos.z));
}

bool World::set_nbt(const Vec3i& world_pos, uint16_t nbt) {
    if (world_pos.y < 0 || world_pos.y >= CHUNK_HEIGHT) {
        return false;
    }
    Vec2i chunk_pos = world_to_chunk_coords(world_pos);
    std::shared_ptr<Chunk> chunk = get_chunk(chunk_pos);
    if (!chunk) {
        return false;
    }
    Vec3i local_pos = world_to_local_coords(world_pos);
    chunk->set_nbt(static_cast<uint8_t>(local_pos.x),
                   static_cast<uint16_t>(local_pos.y),
                   static_cast<uint8_t>(local_pos.z), nbt);

    // NBT может влиять на вариант меша/текстуры, поэтому помечаем чанк
    // dirty сразу.
    mark_chunk_dirty(chunk_pos);
    return true;
}

// ============================================================================
// Item-entity (временно на уровне World)
// ============================================================================

uint32_t World::spawn_item_entity(const ItemStack& stack,
                                  const Vec3f& position,
                                  const Vec3f& velocity) {
    if (stack.empty()) {
        return 0;
    }

    auto entity = std::make_shared<ItemEntity>(stack, position, velocity);
    entity->id = next_item_entity_id++;
    entity->sync_render_mesh();
    entity->refresh_mesh_lighting(*this);
    item_entities.push_back(entity);
    return entity->id;
}

bool World::remove_item_entity(uint32_t entity_id) {
    auto it = std::find_if(
        item_entities.begin(), item_entities.end(),
        [entity_id](const std::shared_ptr<ItemEntity>& entity) {
            return entity && entity->id == entity_id;
        });
    if (it == item_entities.end()) {
        return false;
    }
    item_entities.erase(it);
    return true;
}

void World::clear_item_entities() {
    item_entities.clear();
    next_item_entity_id = 1;
}

std::shared_ptr<ItemEntity> World::get_item_entity(uint32_t entity_id) const {
    for (const auto& entity : item_entities) {
        if (entity && entity->id == entity_id) {
            return entity;
        }
    }
    return nullptr;
}

uint32_t World::item_entity_count() const {
    uint32_t count = 0;
    for (const auto& entity : item_entities) {
        if (entity && entity->alive && !entity->stack.empty()) {
            ++count;
        }
    }
    return count;
}

bool World::pickup_item_entity(uint32_t entity_id, Inventory& inventory,
                               uint32_t preferred_slot) {
    auto entity = get_item_entity(entity_id);
    if (!entity || !entity->can_pickup()) {
        return false;
    }

    ItemStack stack = entity->stack;
    const uint16_t before = stack.count;

    if (preferred_slot != UINT32_MAX) {
        inventory.add_to_slot(preferred_slot, stack);
    } else {
        inventory.add(stack);
    }

    const uint16_t picked = static_cast<uint16_t>(before - stack.count);
    if (picked == 0) {
        return false;
    }

    entity->stack = stack;
    if (entity->stack.empty()) {
        remove_item_entity(entity_id);
    }
    return true;
}

bool World::pickup_nearest_item(const Vec3f& position, float radius,
                                Inventory& inventory,
                                uint32_t preferred_slot) {
    std::shared_ptr<ItemEntity> best;
    float best_distance = radius;

    for (const auto& entity : item_entities) {
        if (!entity || !entity->can_pickup()) {
            continue;
        }
        Vec3f delta = entity->transform.position - position;
        float distance = delta.length();
        if (distance <= best_distance) {
            best_distance = distance;
            best = entity;
        }
    }

    if (!best) {
        return false;
    }
    return pickup_item_entity(best->id, inventory, preferred_slot);
}

void World::update_items(float delta_time) {
    if (delta_time <= 0.0f) {
        return;
    }

    constexpr float GRAVITY = 20.0f;
    constexpr float MAX_DELTA = 0.1f;
    const float dt = std::min(delta_time, MAX_DELTA);

    for (auto& entity : item_entities) {
        if (!entity) {
            continue;
        }
        if (entity->stack.empty()) {
            entity->alive = false;
            continue;
        }

        if (entity->pickup_delay > 0.0f) {
            entity->pickup_delay -= dt;
        }

        FragmentCollision& body = entity->collision_body;

        // Внешнее изменение transform (например, телепорт/выброс) должно
        // сбросить историю интерполяции; обычное движение идёт через body.
        Vec3f delta = entity->transform.position - body.position;
        if (delta.length() > 1e-5f) {
            body.set_position(entity->transform.position);
        }

        if (entity->mover.is_moving()) {
            // Предмет идёт по заранее проверенному пути; mover задаёт
            // горизонтальную скорость вдоль него.
            entity->mover.update(*this, body, dt);
        } else {
            body.velocity = entity->velocity;
            const Vec3f desired(body.velocity.x * dt,
                                0.0f,
                                body.velocity.z * dt);
            GroundMover::try_step_up(*this, body, desired,
                                     entity->step_height);
        }

        step_fragment_against_world(
            *this, body, dt, Vec3f(0.0f, -GRAVITY, 0.0f));

        entity->velocity = body.velocity;
        entity->transform.position = body.position;
        entity->render_position = body.get_render_position(0.0f);
        entity->sync_render_mesh();
        entity->refresh_mesh_lighting(*this);
    }

    item_entities.erase(
        std::remove_if(item_entities.begin(), item_entities.end(),
                       [](const std::shared_ptr<ItemEntity>& entity) {
                           return !entity || !entity->alive || entity->stack.empty();
                       }),
        item_entities.end());
}

void World::update(float delta_time) {
    // Items не зависят от 20-герцового такта мира: двигаем их каждый кадр.
    update_items(delta_time);

    if (delta_time > 0.0f) {
        update_accumulator += delta_time;
    }
    if (update_accumulator < UPDATE_INTERVAL) {
        return;
    }
    update_accumulator = 0.0f;

    // Частичное обновление блоков. Оно может изменить блоки/NBT, поэтому
    // выполняется до пересчёта света и пересборки мешей.
    slow_tick(slow_tick_tick++);

    // Обязательный шаг №1: сначала освещение. Оно само сообщит,
    // в каких nugget'ах свет изменился.
    update_lighting();

    // Затем пересобираем только затронутые чанки.
    rebuild_dirty_meshes();
}

void World::update_lighting() {
    light_engine->update();
    for (Chunk* chunk : light_engine->consume_changed_chunks()) {
        if (chunk) {
            mark_chunk_dirty(chunk->chunk_xz);
            // Вершины на границе соседнего чанка читают light_data этого
            // чанка, поэтому его меш тоже нужно пересобрать.
            mark_horizontal_neighbors_dirty(chunk->chunk_xz);
        }
    }
}

void World::rebuild_chunk_mesh(const Vec2i& chunk_pos) {
    auto it = chunks.find(chunk_pos);
    if (it == chunks.end()) {
        dirty_chunks.erase(chunk_pos);
        return;
    }
    
    auto mesh = ChunkMesher::build_mesh(*it->second, block_mgr, tex_blocks_mgr);
    chunk_meshes[chunk_pos] = mesh;
    dirty_chunks.erase(chunk_pos);

    if (mesh_update_callback) {
        mesh_update_callback(chunk_pos, mesh);
    }
}

void World::rebuild_dirty_meshes() {
    if (dirty_chunks.empty()) {
        return;
    }
    std::vector<Vec2i> positions(dirty_chunks.begin(), dirty_chunks.end());
    for (const Vec2i& pos : positions) {
        rebuild_chunk_mesh(pos);
    }
}

void World::set_mesh_update_callback(MeshUpdateCallback callback) {
    mesh_update_callback = std::move(callback);
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
    (void)tick_counter;  // Оставлен только для совместимости старого API.

    if (slow_tick_cursor + SLOW_TICK_BATCH_SIZE > SLOW_TICK_BLOCK_COUNT) {
        reshuffle_slow_tick_sequence();
    }

    const uint32_t* batch = slow_tick_sequence.data() + slow_tick_cursor;
    for (auto& [pos, chunk] : chunks) {
        if (chunk) {
            chunk->slow_tick(global_tick, batch, SLOW_TICK_BATCH_SIZE,
                             block_mgr);
        }
    }
    slow_tick_cursor += SLOW_TICK_BATCH_SIZE;
}

void World::reshuffle_slow_tick_sequence() {
    std::iota(slow_tick_sequence.begin(), slow_tick_sequence.end(), 0u);
    std::shuffle(slow_tick_sequence.begin(), slow_tick_sequence.end(),
                 slow_tick_rng);
    slow_tick_cursor = 0;
}

void World::set_slow_tick_cursor(uint32_t cursor) {
    slow_tick_cursor =
        std::min(cursor, static_cast<uint32_t>(SLOW_TICK_BLOCK_COUNT));
}

void World::mark_horizontal_neighbors_dirty(const Vec2i& pos) {
    // Вершины на углу чанка читают свет диагонального соседа, поэтому
    // обходим все 8 окружающих чанков, а не только 4 по сторонам.
    const Vec2i directions[8] = {
        {pos.x - 1, pos.y},
        {pos.x + 1, pos.y},
        {pos.x, pos.y - 1},
        {pos.x, pos.y + 1},
        {pos.x - 1, pos.y - 1},
        {pos.x + 1, pos.y - 1},
        {pos.x - 1, pos.y + 1},
        {pos.x + 1, pos.y + 1}
    };
    for (const Vec2i& neighbor : directions) {
        if (has_chunk(neighbor)) {
            mark_chunk_dirty(neighbor);
        }
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


