#include "light_engine.h"
#include "world.h"
#include <algorithm>

namespace {
constexpr int SIZE = static_cast<int>(Chunk::SIZE_XZ);
constexpr int HEIGHT = static_cast<int>(Chunk::HEIGHT);

int floor_div(int value, int divisor) {
    int q = value / divisor;
    int r = value % divisor;
    if (r < 0) --q;
    return q;
}
} // namespace


LightEngine::LightEngine(World& world) : world(world) {
    full_opacity.fill(255);
}


// ============================================================================
// Индексация 3x3x3
// ============================================================================

int LightEngine::offset_index(int dx, int dy, int dz) {
    return (dx + 1) + 3 * (dy + 1) + 9 * (dz + 1);
}

void LightEngine::index_offset(int index, int& dx, int& dy, int& dz) {
    dx = index % 3 - 1;
    dy = (index / 3) % 3 - 1;
    dz = index / 9 - 1;
}

int LightEngine::offset_distance(int dx, int dy, int dz) {
    int n = (dx != 0) + (dy != 0) + (dz != 0);
    if (n == 1) return 8;   // сторона
    if (n == 2) return 12;  // ребро
    return 14;              // угол
}


// ============================================================================
// fast / slow доступ
// ============================================================================

bool LightEngine::can_use_fast(int x, int y, int z) {
    return x > 0 && x < SIZE - 1 &&
           y > 0 && y < HEIGHT - 1 &&
           z > 0 && z < SIZE - 1;
}

uint8_t LightEngine::opacity_fast(const Chunk& chunk, int x, int y, int z) {
    return chunk.opacity_data[Chunk::coords_to_global_index(
        static_cast<uint8_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z))];
}

uint8_t LightEngine::opacity_slow(const Chunk& chunk, int x, int y, int z) const {
    if (y < 0 || y >= HEIGHT) return 0;
    if (x >= 0 && x < SIZE && z >= 0 && z < SIZE) {
        return opacity_fast(chunk, x, y, z);
    }

    int32_t wx = chunk.chunk_xz.x * SIZE + x;
    int32_t wz = chunk.chunk_xz.y * SIZE + z;
    int cx = floor_div(wx, SIZE);
    int cz = floor_div(wz, SIZE);
    auto neighbor = world.get_chunk(Vec2i(cx, cz));
    if (!neighbor) return 0; // незагруженный чанк считаем воздухом

    int lx = wx - cx * SIZE;
    int lz = wz - cz * SIZE;
    return opacity_fast(*neighbor, lx, y, lz);
}

bool LightEngine::locate_neighbor(const Chunk& base, int x, int y, int z,
                                  Chunk*& out_chunk, Vec3i& out_local) const {
    if (y < 0 || y >= HEIGHT) return false;

    if (x >= 0 && x < SIZE && z >= 0 && z < SIZE) {
        out_chunk = const_cast<Chunk*>(&base);
        out_local = Vec3i(x, y, z);
        return true;
    }

    int32_t wx = base.chunk_xz.x * SIZE + x;
    int32_t wz = base.chunk_xz.y * SIZE + z;
    int cx = floor_div(wx, SIZE);
    int cz = floor_div(wz, SIZE);
    auto neighbor = world.get_chunk(Vec2i(cx, cz));
    if (!neighbor) return false;

    out_chunk = neighbor.get();
    out_local = Vec3i(wx - cx * SIZE, y, wz - cz * SIZE);
    return true;
}


// ============================================================================
// full_opacity
// ============================================================================

void LightEngine::fill_full_opacity_fast(const Chunk& chunk, int x, int y, int z) {
    full_opacity.fill(255);
    full_opacity[CENTER] = opacity_fast(chunk, x, y, z);

    // 6 сторон
    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        int dx, dy, dz;
        index_offset(i, dx, dy, dz);
        int n = (dx != 0) + (dy != 0) + (dz != 0);
        if (n != 1) continue;
        full_opacity[i] = clamp_u8(
            static_cast<int>(opacity_fast(chunk, x + dx, y + dy, z + dz)) + 8);
    }

    // 12 рёбер: минимум из двух прилегающих сторон + 4
    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        int dx, dy, dz;
        index_offset(i, dx, dy, dz);
        int n = (dx != 0) + (dy != 0) + (dz != 0);
        if (n != 2) continue;

        uint8_t a = 255, b = 255;
        bool first = true;
        if (dx != 0) { a = full_opacity[offset_index(0, dy, dz)]; first = false; }
        if (dy != 0) {
            uint8_t v = full_opacity[offset_index(dx, 0, dz)];
            if (first) { a = v; first = false; } else { b = v; }
        }
        if (dz != 0) {
            uint8_t v = full_opacity[offset_index(dx, dy, 0)];
            if (first) { a = v; } else { b = v; }
        }
        full_opacity[i] = clamp_u8(static_cast<int>(std::min(a, b)) + 4);
    }

    // 8 углов: минимум из прилегающих рёбер + 2
    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        int dx, dy, dz;
        index_offset(i, dx, dy, dz);
        int n = (dx != 0) + (dy != 0) + (dz != 0);
        if (n != 3) continue;

        uint8_t m = full_opacity[offset_index(0, dy, dz)];
        m = std::min(m, full_opacity[offset_index(dx, 0, dz)]);
        m = std::min(m, full_opacity[offset_index(dx, dy, 0)]);
        full_opacity[i] = clamp_u8(static_cast<int>(m) + 2);
    }
}

void LightEngine::fill_full_opacity_slow(const Chunk& chunk, int x, int y, int z) {
    full_opacity.fill(255);
    full_opacity[CENTER] = opacity_slow(chunk, x, y, z);

    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        int dx, dy, dz;
        index_offset(i, dx, dy, dz);
        int n = (dx != 0) + (dy != 0) + (dz != 0);
        if (n != 1) continue;
        full_opacity[i] = clamp_u8(
            static_cast<int>(opacity_slow(chunk, x + dx, y + dy, z + dz)) + 8);
    }

    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        int dx, dy, dz;
        index_offset(i, dx, dy, dz);
        int n = (dx != 0) + (dy != 0) + (dz != 0);
        if (n != 2) continue;

        uint8_t a = 255, b = 255;
        bool first = true;
        if (dx != 0) { a = full_opacity[offset_index(0, dy, dz)]; first = false; }
        if (dy != 0) {
            uint8_t v = full_opacity[offset_index(dx, 0, dz)];
            if (first) { a = v; first = false; } else { b = v; }
        }
        if (dz != 0) {
            uint8_t v = full_opacity[offset_index(dx, dy, 0)];
            if (first) { a = v; } else { b = v; }
        }
        full_opacity[i] = clamp_u8(static_cast<int>(std::min(a, b)) + 4);
    }

    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        int dx, dy, dz;
        index_offset(i, dx, dy, dz);
        int n = (dx != 0) + (dy != 0) + (dz != 0);
        if (n != 3) continue;

        uint8_t m = full_opacity[offset_index(0, dy, dz)];
        m = std::min(m, full_opacity[offset_index(dx, 0, dz)]);
        m = std::min(m, full_opacity[offset_index(dx, dy, 0)]);
        full_opacity[i] = clamp_u8(static_cast<int>(m) + 2);
    }
}


// ============================================================================
// Очереди
// ============================================================================

void LightEngine::push_dark(Chunk& chunk, const Vec3i& local, uint8_t level) {
    if (level == 0) return;
    auto& list = can_use_fast(local.x, local.y, local.z)
        ? dark_lists.fast[level - 1]
        : dark_lists.slow[level - 1];
    list.push_back({&chunk, local});
}

void LightEngine::push_light(Chunk& chunk, const Vec3i& local, uint8_t level) {
    if (level == 0) return;
    auto& list = can_use_fast(local.x, local.y, local.z)
        ? light_lists.fast[level - 1]
        : light_lists.slow[level - 1];
    list.push_back({&chunk, local});
}

void LightEngine::schedule_dark(Chunk& chunk, const Vec3i& local) {
    uint32_t idx = Chunk::coords_to_global_index(
        static_cast<uint8_t>(local.x), static_cast<uint16_t>(local.y),
        static_cast<uint8_t>(local.z));

    uint8_t level = chunk.light_data[idx];
    if (level == 0) return;
    if (chunk.is_light_dirty(idx)) return;

    chunk.mark_light_dirty(idx, true);
    push_dark(chunk, local, level);
}

void LightEngine::schedule_light(Chunk& chunk, const Vec3i& local, uint8_t level) {
    if (level == 0) return;

    uint32_t idx = Chunk::coords_to_global_index(
        static_cast<uint8_t>(local.x), static_cast<uint16_t>(local.y),
        static_cast<uint8_t>(local.z));

    if (level <= chunk.light_queue_level[idx]) return;

    chunk.light_queue_level[idx] = level;
    chunk.mark_light_dirty(idx, true);
    push_light(chunk, local, level);
}

void LightEngine::schedule_light_source(Chunk& chunk, const Vec3i& local) {
    uint16_t id = chunk.get_block_id(
        static_cast<uint8_t>(local.x), static_cast<uint16_t>(local.y),
        static_cast<uint8_t>(local.z));
    if (id == 0) return;
    auto block = world.get_block_mgr().get_block(id);
    if (!block || block->light == 0) return;

    schedule_light(chunk, local, block->light);
}

void LightEngine::mark_neighborhood(Chunk& chunk, const Vec3i& local_pos) {
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                Chunk* target = nullptr;
                Vec3i target_local;
                if (!locate_neighbor(chunk, local_pos.x + dx, local_pos.y + dy,
                                     local_pos.z + dz, target, target_local)) {
                    continue;
                }

                uint32_t idx = Chunk::coords_to_global_index(
                    static_cast<uint8_t>(target_local.x),
                    static_cast<uint16_t>(target_local.y),
                    static_cast<uint8_t>(target_local.z));

                bool center = (dx == 0 && dy == 0 && dz == 0);
                if (center) {
                    // Изменённый блок перепланируем принудительно: старый свет
                    // должен быть снят, даже если блок уже был помечен.
                    target->mark_light_dirty(idx, false);
                    target->light_queue_level[idx] = 0;
                }

                if (target->light_data[idx] > 0) {
                    if (center) {
                        target->mark_light_dirty(idx, true);
                        push_dark(*target, target_local, target->light_data[idx]);
                    } else {
                        schedule_dark(*target, target_local);
                    }
                } else {
                    schedule_light_source(*target, target_local);
                }
            }
        }
    }
}

void LightEngine::mark_chunk_sources(Chunk& chunk) {
    for (auto& nugget : chunk.nuggets) {
        for (uint16_t i = 0; i < ChunkNugget::BLOCKS_COUNT; ++i) {
            uint16_t id = nugget.block_ids[i];
            if (id == 0) continue;

            auto block = world.get_block_mgr().get_block(id);
            if (!block || block->light == 0) continue;

            Vec3i lc = ChunkNugget::index_to_coords(i);
            uint16_t gy = static_cast<uint16_t>(nugget.local_y_index * ChunkNugget::SIZE +
                                                static_cast<uint16_t>(lc.y));
            Vec3i local(lc.x, gy, lc.z);
            schedule_light(chunk, local, block->light);
        }
    }
}

void LightEngine::mark_chunk_border(Chunk& chunk) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            schedule_dark(chunk, Vec3i(x, y, 0));
            schedule_dark(chunk, Vec3i(x, y, SIZE - 1));
        }
        for (int z = 1; z < SIZE - 1; ++z) {
            schedule_dark(chunk, Vec3i(0, y, z));
            schedule_dark(chunk, Vec3i(SIZE - 1, y, z));
        }
    }
}

void LightEngine::forget_chunk(Chunk* chunk) {
    auto erase_from = [chunk](LevelLists& lists) {
        for (auto& list : lists.fast) {
            list.erase(std::remove_if(list.begin(), list.end(),
                [chunk](const Entry& e) { return e.chunk == chunk; }), list.end());
        }
        for (auto& list : lists.slow) {
            list.erase(std::remove_if(list.begin(), list.end(),
                [chunk](const Entry& e) { return e.chunk == chunk; }), list.end());
        }
    };
    erase_from(dark_lists);
    erase_from(light_lists);
    darkened_blocks.erase(std::remove_if(darkened_blocks.begin(), darkened_blocks.end(),
        [chunk](const Entry& e) { return e.chunk == chunk; }), darkened_blocks.end());
}

bool LightEngine::has_pending() const {
    return has_dark_pending() || has_light_pending();
}

bool LightEngine::has_dark_pending() const {
    for (const auto& list : dark_lists.fast) if (!list.empty()) return true;
    for (const auto& list : dark_lists.slow) if (!list.empty()) return true;
    return false;
}

bool LightEngine::has_light_pending() const {
    for (const auto& list : light_lists.fast) if (!list.empty()) return true;
    for (const auto& list : light_lists.slow) if (!list.empty()) return true;
    return false;
}

void LightEngine::clear_queues() {
    auto reset_dark = [](std::array<std::vector<Entry>, LEVEL_COUNT>& lists) {
        for (auto& list : lists) {
            for (const Entry& entry : list) {
                uint32_t idx = Chunk::coords_to_global_index(
                    static_cast<uint8_t>(entry.local.x),
                    static_cast<uint16_t>(entry.local.y),
                    static_cast<uint8_t>(entry.local.z));
                entry.chunk->mark_light_dirty(idx, false);
            }
            list.clear();
        }
    };

    auto reset_light = [](std::array<std::vector<Entry>, LEVEL_COUNT>& lists) {
        for (int level = 1; level <= LEVEL_COUNT; ++level) {
            for (const Entry& entry : lists[level - 1]) {
                uint32_t idx = Chunk::coords_to_global_index(
                    static_cast<uint8_t>(entry.local.x),
                    static_cast<uint16_t>(entry.local.y),
                    static_cast<uint8_t>(entry.local.z));
                entry.chunk->mark_light_dirty(idx, false);
                if (entry.chunk->light_queue_level[idx] == level) {
                    entry.chunk->light_queue_level[idx] = 0;
                }
            }
            lists[level - 1].clear();
        }
    };

    reset_dark(dark_lists.fast);
    reset_dark(dark_lists.slow);
    reset_light(light_lists.fast);
    reset_light(light_lists.slow);
    darkened_blocks.clear();
}


// ============================================================================
// Затемнение
// ============================================================================

void LightEngine::process_dark_entry(const Entry& entry, bool fast) {
    Chunk& chunk = *entry.chunk;
    int x = entry.local.x;
    int y = entry.local.y;
    int z = entry.local.z;
    uint32_t idx = Chunk::coords_to_global_index(
        static_cast<uint8_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z));

    uint8_t light = chunk.light_data[idx];
    chunk.mark_light_dirty(idx, false);

    if (light == 0) return;

    if (fast) fill_full_opacity_fast(chunk, x, y, z);
    else fill_full_opacity_slow(chunk, x, y, z);

    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        int dx, dy, dz;
        index_offset(i, dx, dy, dz);

        Chunk* nchunk = nullptr;
        Vec3i nlocal;
        if (!locate_neighbor(chunk, x + dx, y + dy, z + dz, nchunk, nlocal)) {
            continue;
        }

        uint32_t nidx = Chunk::coords_to_global_index(
            static_cast<uint8_t>(nlocal.x), static_cast<uint16_t>(nlocal.y),
            static_cast<uint8_t>(nlocal.z));
        uint8_t nlight = nchunk->light_data[nidx];

        // Сосед мог получить свой свет от текущего блока. Проверяем оба
        // направления: свет при распространении считается по full_opacity,
        // а при пересчёте блока — через opacity этого блока и расстояние.
        int n_opacity = nchunk->opacity_data[nidx];
        int expected_forward = static_cast<int>(light) - static_cast<int>(full_opacity[i]);
        int expected_reverse = static_cast<int>(light) -
                               offset_distance(dx, dy, dz) - n_opacity;
        int expected = std::max(expected_forward, expected_reverse);
        if (expected > 0 && nlight > 0 && static_cast<int>(nlight) <= expected) {
            schedule_dark(*nchunk, nlocal);
        }
    }

    chunk.light_data[idx] = 0;
    darkened_blocks.push_back(entry);
}

void LightEngine::seed_after_darkening() {
    for (const Entry& entry : darkened_blocks) {
        Chunk& chunk = *entry.chunk;
        int x = entry.local.x;
        int y = entry.local.y;
        int z = entry.local.z;
        uint32_t idx = Chunk::coords_to_global_index(
            static_cast<uint8_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z));

        if (chunk.light_data[idx] != 0) continue;

        if (can_use_fast(x, y, z)) fill_full_opacity_fast(chunk, x, y, z);
        else fill_full_opacity_slow(chunk, x, y, z);

        int center_opacity = full_opacity[CENTER];
        int best = 0;

        for (int i = 0; i < 27; ++i) {
            if (i == CENTER) continue;
            int dx, dy, dz;
            index_offset(i, dx, dy, dz);

            Chunk* nchunk = nullptr;
            Vec3i nlocal;
            if (!locate_neighbor(chunk, x + dx, y + dy, z + dz, nchunk, nlocal)) {
                continue;
            }

            uint32_t nidx = Chunk::coords_to_global_index(
                static_cast<uint8_t>(nlocal.x), static_cast<uint16_t>(nlocal.y),
                static_cast<uint8_t>(nlocal.z));
            int candidate = static_cast<int>(nchunk->light_data[nidx]) -
                            offset_distance(dx, dy, dz) - center_opacity;
            if (candidate > best) best = candidate;
        }

        uint16_t block_id = chunk.get_block_id(
            static_cast<uint8_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z));
        auto block = world.get_block_mgr().get_block(block_id);
        int emission = block ? static_cast<int>(block->light) : 0;
        int seed = std::max(best, emission);

        if (seed > 0) {
            schedule_light(chunk, entry.local,
                           static_cast<uint8_t>(std::min(seed, 255)));
        }
    }
    darkened_blocks.clear();
}

void LightEngine::remove_light() {
    while (has_dark_pending()) {
        for (int level = LEVEL_COUNT; level >= 1; --level) {
            auto& fast_list = dark_lists.fast[level - 1];
            for (size_t i = 0; i < fast_list.size(); ++i) {
                process_dark_entry(fast_list[i], true);
            }
            fast_list.clear();

            auto& slow_list = dark_lists.slow[level - 1];
            for (size_t i = 0; i < slow_list.size(); ++i) {
                process_dark_entry(slow_list[i], false);
            }
            slow_list.clear();
        }
    }

    seed_after_darkening();
}


// ============================================================================
// Освещение
// ============================================================================

void LightEngine::process_light_entry(const Entry& entry, int level, bool fast) {
    Chunk& chunk = *entry.chunk;
    int x = entry.local.x;
    int y = entry.local.y;
    int z = entry.local.z;
    uint32_t idx = Chunk::coords_to_global_index(
        static_cast<uint8_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z));

    uint8_t queued = chunk.light_queue_level[idx];
    if (level < queued) return; // есть запись с более высоким приоритетом

    uint8_t current = chunk.light_data[idx];
    if (queued == level) {
        chunk.light_queue_level[idx] = 0;
        chunk.mark_light_dirty(idx, false);
    } else {
        // Записи в очереди уже нет: если свет не ниже её уровня, работа сделана.
        chunk.mark_light_dirty(idx, false);
        if (current >= level) return;
    }

    if (fast) fill_full_opacity_fast(chunk, x, y, z);
    else fill_full_opacity_slow(chunk, x, y, z);

    int center_opacity = full_opacity[CENTER];
    int new_light = 0;

    uint16_t block_id = chunk.get_block_id(
        static_cast<uint8_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z));
    auto block = world.get_block_mgr().get_block(block_id);
    if (block) new_light = block->light;

    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        int dx, dy, dz;
        index_offset(i, dx, dy, dz);

        Chunk* nchunk = nullptr;
        Vec3i nlocal;
        if (!locate_neighbor(chunk, x + dx, y + dy, z + dz, nchunk, nlocal)) {
            continue;
        }

        uint32_t nidx = Chunk::coords_to_global_index(
            static_cast<uint8_t>(nlocal.x), static_cast<uint16_t>(nlocal.y),
            static_cast<uint8_t>(nlocal.z));
        int candidate = static_cast<int>(nchunk->light_data[nidx]) -
                        offset_distance(dx, dy, dz) - center_opacity;
        if (candidate > new_light) new_light = candidate;
    }

    new_light = std::max(0, std::min(new_light, 255));
    chunk.light_data[idx] = static_cast<uint8_t>(new_light);

    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        int dx, dy, dz;
        index_offset(i, dx, dy, dz);

        int target = new_light - static_cast<int>(full_opacity[i]);
        if (target <= 0) continue;

        Chunk* nchunk = nullptr;
        Vec3i nlocal;
        if (!locate_neighbor(chunk, x + dx, y + dy, z + dz, nchunk, nlocal)) {
            continue;
        }

        uint32_t nidx = Chunk::coords_to_global_index(
            static_cast<uint8_t>(nlocal.x), static_cast<uint16_t>(nlocal.y),
            static_cast<uint8_t>(nlocal.z));
        if (nchunk->light_data[nidx] < target) {
            schedule_light(*nchunk, nlocal, static_cast<uint8_t>(std::min(target, 255)));
        }
    }
}

void LightEngine::propagate_light() {
    // Обычно хватает одного прохода. Повтор нужен только в редком случае,
    // когда запись попала в уже пройденный (более высокий) уровень.
    while (has_light_pending()) {
        for (int level = LEVEL_COUNT; level >= 1; --level) {
            auto& fast_list = light_lists.fast[level - 1];
            for (size_t i = 0; i < fast_list.size(); ++i) {
                process_light_entry(fast_list[i], level, true);
            }
            fast_list.clear();

            auto& slow_list = light_lists.slow[level - 1];
            for (size_t i = 0; i < slow_list.size(); ++i) {
                process_light_entry(slow_list[i], level, false);
            }
            slow_list.clear();
        }
    }
}

void LightEngine::update() {
    remove_light();
    propagate_light();
}
