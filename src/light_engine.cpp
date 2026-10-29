#include "light_engine.h"
#include "world.h"
#include <algorithm>


namespace {
constexpr int SIZE = static_cast<int>(Chunk::SIZE_XZ);
constexpr int HEIGHT = static_cast<int>(Chunk::HEIGHT);
constexpr int SLICE = SIZE * HEIGHT;
constexpr int CENTER = LightEngine::CENTER;

// Смещение соседа в 3x3x3.
struct Offset {
    int8_t dx, dy, dz;
    int32_t delta;   // приращение плоского индекса внутри чанка
    uint8_t dist;    // 8 - сторона, 12 - ребро, 14 - угол
};

constexpr int offset_index(int dx, int dy, int dz) {
    return (dx + 1) + 3 * (dy + 1) + 9 * (dz + 1);
}

constexpr int nonzero_count(int dx, int dy, int dz) {
    return (dx != 0) + (dy != 0) + (dz != 0);
}

constexpr int offset_distance(int n) {
    return n == 1 ? 8 : (n == 2 ? 12 : 14);
}

constexpr std::array<Offset, 27> make_offsets() {
    std::array<Offset, 27> result{};
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int n = nonzero_count(dx, dy, dz);
                Offset o{};
                o.dx = static_cast<int8_t>(dx);
                o.dy = static_cast<int8_t>(dy);
                o.dz = static_cast<int8_t>(dz);
                o.delta = dx + dy * SIZE + dz * SLICE;
                o.dist = static_cast<uint8_t>(n == 0 ? 0 : offset_distance(n));
                result[offset_index(dx, dy, dz)] = o;
            }
        }
    }
    return result;
}

constexpr std::array<Offset, 27> OFFSETS = make_offsets();

constexpr std::array<int, 6> make_side_indices() {
    std::array<int, 6> result{};
    int k = 0;
    for (int i = 0; i < 27; ++i) {
        const Offset& o = OFFSETS[i];
        if (nonzero_count(o.dx, o.dy, o.dz) == 1) result[k++] = i;
    }
    return result;
}

// {индекс ребра, первый прилегающий side, второй прилегающий side}
constexpr std::array<std::array<int, 3>, 12> make_edge_indices() {
    std::array<std::array<int, 3>, 12> result{};
    int k = 0;
    for (int i = 0; i < 27; ++i) {
        const Offset& o = OFFSETS[i];
        if (nonzero_count(o.dx, o.dy, o.dz) != 2) continue;

        int s1 = 0, s2 = 0;
        if (o.dx != 0 && o.dy != 0) {
            s1 = offset_index(0, o.dy, 0);
            s2 = offset_index(o.dx, 0, 0);
        } else if (o.dx != 0 && o.dz != 0) {
            s1 = offset_index(0, 0, o.dz);
            s2 = offset_index(o.dx, 0, 0);
        } else {
            s1 = offset_index(0, 0, o.dz);
            s2 = offset_index(0, o.dy, 0);
        }
        result[k++] = {i, s1, s2};
    }
    return result;
}

// {индекс угла, три прилегающих ребра}
constexpr std::array<std::array<int, 4>, 8> make_corner_indices() {
    std::array<std::array<int, 4>, 8> result{};
    int k = 0;
    for (int i = 0; i < 27; ++i) {
        const Offset& o = OFFSETS[i];
        if (nonzero_count(o.dx, o.dy, o.dz) != 3) continue;
        result[k++] = {
            i,
            offset_index(0, o.dy, o.dz),
            offset_index(o.dx, 0, o.dz),
            offset_index(o.dx, o.dy, 0)
        };
    }
    return result;
}

constexpr std::array<int, 6> SIDES = make_side_indices();
constexpr std::array<std::array<int, 3>, 12> EDGES = make_edge_indices();
constexpr std::array<std::array<int, 4>, 8> CORNERS = make_corner_indices();

inline int index_x(uint32_t index) { return static_cast<int>(index % SIZE); }
inline int index_y(uint32_t index) { return static_cast<int>((index / SIZE) % HEIGHT); }
inline int index_z(uint32_t index) { return static_cast<int>(index / SLICE); }
} // namespace


LightEngine::LightEngine(World& world) : world(world) {
    full_opacity.fill(255);
}


// ============================================================================
// Доступ к соседям
// ============================================================================

bool LightEngine::can_use_fast(uint32_t index) {
    int x = index_x(index);
    int z = index_z(index);
    return x > 0 && x < SIZE - 1 && z > 0 && z < SIZE - 1;
}

void LightEngine::resolve_neighborhood(const Chunk& base, int x, int y, int z,
                                       std::array<Chunk*, 27>& out_chunks,
                                       std::array<uint32_t, 27>& out_indices) const {
    // Один раз находим чанки для девяти комбинаций (dx, dz). В отличие от
    // world.get_chunk() здесь нет хеширования и копирования shared_ptr.
    Chunk* by_xz[3][3];
    for (int sx = -1; sx <= 1; ++sx) {
        for (int sz = -1; sz <= 1; ++sz) {
            Chunk* found = const_cast<Chunk*>(&base);
            if (sx < 0) found = found->get_neighbor(ChunkNeighborDir::NegX);
            else if (sx > 0) found = found->get_neighbor(ChunkNeighborDir::PosX);

            if (found) {
                if (sz < 0) found = found->get_neighbor(ChunkNeighborDir::NegZ);
                else if (sz > 0) found = found->get_neighbor(ChunkNeighborDir::PosZ);
            }

            // Редкий случай: диагональный чанк загружен, а кардинальный нет.
            // Сохраняем прежнее поведение (поиск по миру) для совместимости.
            if (!found) {
                found = world.get_chunk_ptr(
                    Vec2i(base.chunk_xz.x + sx, base.chunk_xz.y + sz));
            }
            by_xz[sx + 1][sz + 1] = found;
        }
    }

    for (int i = 0; i < 27; ++i) {
        const Offset& o = OFFSETS[i];
        int ny = y + o.dy;
        if (ny < 0 || ny >= HEIGHT) {
            out_chunks[i] = nullptr;
            out_indices[i] = 0;
            continue;
        }

        // Чанк меняется только если координата реально выходит за границу,
        // а не просто если смещение отрицательное/положительное.
        int csx = 0;
        int csz = 0;
        if (o.dx < 0 && x == 0) csx = -1;
        else if (o.dx > 0 && x == SIZE - 1) csx = 1;
        if (o.dz < 0 && z == 0) csz = -1;
        else if (o.dz > 0 && z == SIZE - 1) csz = 1;

        Chunk* target = by_xz[csx + 1][csz + 1];
        if (!target) {
            out_chunks[i] = nullptr;
            out_indices[i] = 0;
            continue;
        }

        int lx = (x + o.dx) & (SIZE - 1);
        int lz = (z + o.dz) & (SIZE - 1);
        out_chunks[i] = target;
        out_indices[i] = Chunk::coords_to_global_index(
            static_cast<uint8_t>(lx), static_cast<uint16_t>(ny),
            static_cast<uint8_t>(lz));
    }
}


// ============================================================================
// full_opacity
// ============================================================================

void LightEngine::fill_full_opacity_fast(const Chunk& chunk, uint32_t index, int y) {
    uint8_t* fo = full_opacity.data();
    std::fill(fo, fo + 27, static_cast<uint8_t>(255));

    const uint8_t* opacity = chunk.opacity_data.data();
    fo[CENTER] = opacity[index];

    // 6 сторон
    for (int i : SIDES) {
        const Offset& o = OFFSETS[i];
        int ny = y + o.dy;
        int value = (ny >= 0 && ny < HEIGHT)
            ? static_cast<int>(opacity[index + o.delta])
            : 0;
        fo[i] = clamp_u8(value + 8);
    }

    // 12 рёбер: минимум из двух прилегающих сторон + 4
    for (const auto& edge : EDGES) {
        fo[edge[0]] = clamp_u8(std::min(fo[edge[1]], fo[edge[2]]) + 4);
    }

    // 8 углов: минимум из прилегающих рёбер + 2
    for (const auto& corner : CORNERS) {
        int m = std::min(static_cast<int>(fo[corner[1]]),
                         static_cast<int>(fo[corner[2]]));
        m = std::min(m, static_cast<int>(fo[corner[3]]));
        fo[corner[0]] = clamp_u8(m + 2);
    }
}

void LightEngine::fill_full_opacity_slow(const Chunk& chunk, uint32_t center_index,
                                         const std::array<Chunk*, 27>& chunks,
                                         const std::array<uint32_t, 27>& indices) {
    uint8_t* fo = full_opacity.data();
    std::fill(fo, fo + 27, static_cast<uint8_t>(255));

    fo[CENTER] = chunk.opacity_data[center_index];

    for (int i : SIDES) {
        int value = chunks[i]
            ? static_cast<int>(chunks[i]->opacity_data[indices[i]])
            : 0;
        fo[i] = clamp_u8(value + 8);
    }

    for (const auto& edge : EDGES) {
        fo[edge[0]] = clamp_u8(std::min(fo[edge[1]], fo[edge[2]]) + 4);
    }

    for (const auto& corner : CORNERS) {
        int m = std::min(static_cast<int>(fo[corner[1]]),
                         static_cast<int>(fo[corner[2]]));
        m = std::min(m, static_cast<int>(fo[corner[3]]));
        fo[corner[0]] = clamp_u8(m + 2);
    }
}


// ============================================================================
// Эмиссия и оценка света
// ============================================================================

uint8_t LightEngine::emission(const Chunk& chunk, uint32_t index) const {
    // Прямое чтение id без промежуточных проверок границ.
    uint32_t z = index / SLICE;
    uint32_t rem = index - z * SLICE;
    uint32_t y = rem / SIZE;
    uint32_t x = rem - y * SIZE;

    const ChunkNugget& nugget = chunk.nuggets[y / ChunkNugget::SIZE];
    uint16_t id = nugget.block_ids[
        x + (y % ChunkNugget::SIZE) * ChunkNugget::SIZE +
        z * ChunkNugget::SIZE * ChunkNugget::SIZE];
    const Block* block = world.get_block_mgr().get_block_raw(id);
    return block ? block->light : 0;
}

uint8_t LightEngine::compute_light_fast(const Chunk& chunk, uint32_t index) const {
    int best = emission(chunk, index);
    int center_opacity = chunk.opacity_data[index];
    int y = index_y(index);
    const uint8_t* light = chunk.light_data.data();

    for (int i = 0; i < 27; ++i) {
        if (i == CENTER) continue;
        const Offset& o = OFFSETS[i];
        int ny = y + o.dy;
        if (ny < 0 || ny >= HEIGHT) continue;
        int candidate = static_cast<int>(light[index + o.delta]) -
                        static_cast<int>(o.dist) - center_opacity;
        if (candidate > best) best = candidate;
    }
    return clamp_u8(best);
}

uint8_t LightEngine::compute_light_slow(const std::array<Chunk*, 27>& chunks,
                                        const std::array<uint32_t, 27>& indices) const {
    uint32_t center_index = indices[CENTER];
    int best = emission(*chunks[CENTER], center_index);
    int center_opacity = chunks[CENTER]->opacity_data[center_index];

    for (int i = 0; i < 27; ++i) {
        if (i == CENTER || !chunks[i]) continue;
        const Offset& o = OFFSETS[i];
        int candidate = static_cast<int>(chunks[i]->light_data[indices[i]]) -
                        static_cast<int>(o.dist) - center_opacity;
        if (candidate > best) best = candidate;
    }
    return clamp_u8(best);
}


// ============================================================================
// Небесный (дневной) свет
// ============================================================================

uint8_t LightEngine::sky_column_level(const Chunk& chunk, int x, int y, int z) const {
    if (y < 0) {
        return 0;
    }
    if (y >= HEIGHT) {
        return 255;
    }
    if (x < 0 || x >= SIZE || z < 0 || z >= SIZE) {
        return 0;
    }

    // Свет идёт сверху вниз без потерь на расстояние. Каждый блок вычитает
    // свою opacity (в том числе полупрозрачный). Полностью непрозрачный
    // блок гасит столбец.
    int level = 255;
    const uint32_t base = static_cast<uint32_t>(x) +
                          static_cast<uint32_t>(z) * SLICE;
    for (int cy = HEIGHT - 1; cy >= y; --cy) {
        const uint8_t opacity =
            chunk.opacity_data[base + static_cast<uint32_t>(cy) * SIZE];
        if (opacity != 0) {
            level -= static_cast<int>(opacity);
            if (level <= 0) {
                return 0;
            }
        }
    }
    return static_cast<uint8_t>(level);
}

void LightEngine::initialize_chunk_sky(Chunk& chunk) {
    // Прямой столб неба: без затухания по высоте, с вычитанием opacity.
    for (int z = 0; z < SIZE; ++z) {
        for (int x = 0; x < SIZE; ++x) {
            int level = 255;
            const uint32_t base = static_cast<uint32_t>(x) +
                                  static_cast<uint32_t>(z) * SLICE;
            for (int y = HEIGHT - 1; y >= 0; --y) {
                const uint32_t index = base + static_cast<uint32_t>(y) * SIZE;
                const uint8_t opacity = chunk.opacity_data[index];
                if (opacity != 0) {
                    level -= static_cast<int>(opacity);
                    if (level < 0) level = 0;
                }
                chunk.sky_light_data[index] = static_cast<uint8_t>(level);
            }
        }
    }

    // Семена распространения: клетки на границе чанка и клетки, рядом с
    // которыми прямой свет уже слабее. В полностью открытом воздухе это
    // всего лишь граница, поэтому очередь не разрастается на весь объём.
    for (int z = 0; z < SIZE; ++z) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < SIZE; ++x) {
                const uint32_t index = Chunk::coords_to_global_index(
                    static_cast<uint8_t>(x), static_cast<uint16_t>(y),
                    static_cast<uint8_t>(z));
                const uint8_t level = chunk.sky_light_data[index];
                if (level == 0) continue;

                bool seed = (x == 0 || x == SIZE - 1 ||
                             z == 0 || z == SIZE - 1);
                if (!seed) {
                    for (int i = 0; i < 27; ++i) {
                        if (i == CENTER) continue;
                        const Offset& o = OFFSETS[i];
                        const int ny = y + o.dy;
                        if (ny < 0 || ny >= HEIGHT) continue;
                        if (chunk.sky_light_data[index + o.delta] < level) {
                            seed = true;
                            break;
                        }
                    }
                }
                if (seed) schedule_sky_light(chunk, index, level);
            }
        }
    }
}

void LightEngine::mark_sky_column_changed(Chunk& chunk, int x, int y, int z,
                                          bool darker) {
    if (x < 0 || x >= SIZE || z < 0 || z >= SIZE ||
        y < 0 || y >= HEIGHT) {
        return;
    }

    const uint32_t base = static_cast<uint32_t>(x) +
                          static_cast<uint32_t>(z) * SLICE;

    if (darker) {
        // Блок стал менее прозрачным. Затемняем всё вниз, пока не встретим
        // непрозрачный блок (ниже него прямой свет неба уже не проходит).
        bool first = true;
        for (int cy = y; cy >= 0; --cy) {
            const uint32_t index = base + static_cast<uint32_t>(cy) * SIZE;
            if (chunk.sky_light_data[index] > 0 ||
                chunk.sky_light_queue_level[index] > 0) {
                schedule_sky_dark(chunk, index);
            }
            if (!first && chunk.opacity_data[index] == 255) {
                break;
            }
            first = false;
        }
    } else {
        // Блок стал прозрачнее/удалён. Пересчитываем прямой столб и ставим
        // все клетки вниз на обновление. Даже если сбоку уже есть более
        // яркий свет, прямой свет неба всё равно должен пройти вниз.
        int level = 255;
        for (int cy = HEIGHT - 1; cy >= 0; --cy) {
            const uint32_t index = base + static_cast<uint32_t>(cy) * SIZE;
            const uint8_t opacity = chunk.opacity_data[index];
            if (opacity != 0) {
                level -= static_cast<int>(opacity);
                if (level < 0) level = 0;
            }
            if (cy > y) continue;
            if (level == 0) break;

            schedule_sky_light(chunk, index, static_cast<uint8_t>(level));
            if (opacity == 255) break;
        }
    }
}


// ============================================================================
// Очереди
// ============================================================================

void LightEngine::push_dark(Chunk& chunk, uint32_t index, uint8_t level) {
    if (level == 0) return;
    auto& list = can_use_fast(index)
        ? dark_lists.fast[level - 1]
        : dark_lists.slow[level - 1];
    list.push_back({&chunk, index});
}

void LightEngine::push_light(Chunk& chunk, uint32_t index, uint8_t level) {
    if (level == 0) return;
    auto& list = can_use_fast(index)
        ? light_lists.fast[level - 1]
        : light_lists.slow[level - 1];
    list.push_back({&chunk, index});
}

void LightEngine::schedule_dark(Chunk& chunk, uint32_t index) {
    uint8_t level = chunk.light_data[index];
    if (level == 0) return;
    if (chunk.is_light_dirty(index)) return;

    chunk.mark_light_dirty(index, true);
    push_dark(chunk, index, level);
}

void LightEngine::schedule_light(Chunk& chunk, uint32_t index, uint8_t level) {
    if (level == 0) return;
    if (level <= chunk.light_queue_level[index]) return;

    chunk.light_queue_level[index] = level;
    chunk.mark_light_dirty(index, true);
    push_light(chunk, index, level);
}

void LightEngine::schedule_light_source(Chunk& chunk, uint32_t index) {
    uint8_t level = emission(chunk, index);
    if (level == 0) return;
    schedule_light(chunk, index, level);
}

void LightEngine::push_sky_dark(Chunk& chunk, uint32_t index, uint8_t level) {
    if (level == 0) return;
    auto& list = can_use_fast(index)
        ? sky_dark_lists.fast[level - 1]
        : sky_dark_lists.slow[level - 1];
    list.push_back({&chunk, index});
}

void LightEngine::push_sky_light(Chunk& chunk, uint32_t index, uint8_t level) {
    if (level == 0) return;
    auto& list = can_use_fast(index)
        ? sky_light_lists.fast[level - 1]
        : sky_light_lists.slow[level - 1];
    list.push_back({&chunk, index});
}

void LightEngine::schedule_sky_dark(Chunk& chunk, uint32_t index) {
    uint8_t level = chunk.sky_light_data[index];

    // Если клетка уже стоит в очереди на распространение, эта запись
    // устаревает: затемнение должно победить, а свет будет пересчитан
    // заново в seed_sky_after_darkening(). Это нужно делать даже при
    // level == 0: у клетки может быть отложенное увеличение света,
    // которое после установки блока уже недействительно.
    const bool had_pending = chunk.sky_light_queue_level[index] > 0;
    if (had_pending) {
        chunk.sky_light_queue_level[index] = 0;
        chunk.mark_sky_light_dirty(index, false);
    }
    if (level == 0) {
        if (!had_pending) return;
        // Текущего света нет, но была отложенная запись: пересчитываем
        // клетку служебной записью на уровне 1, чтобы seed восстановил
        // корректное значение из прямого и бокового света.
        chunk.mark_sky_light_dirty(index, true);
        push_sky_dark(chunk, index, 1);
        return;
    }
    if (chunk.is_sky_light_dirty(index)) return;

    chunk.mark_sky_light_dirty(index, true);
    push_sky_dark(chunk, index, level);
}

void LightEngine::schedule_sky_light(Chunk& chunk, uint32_t index, uint8_t level) {
    if (level == 0) return;

    // dirty + пустой queue означает отложенное затемнение. Свет в такую
    // клетку не ставим: после затемнения он будет восстановлен семенем.
    if (chunk.is_sky_light_dirty(index) &&
        chunk.sky_light_queue_level[index] == 0) {
        return;
    }
    if (level <= chunk.sky_light_queue_level[index]) return;

    chunk.sky_light_queue_level[index] = level;
    chunk.mark_sky_light_dirty(index, true);
    push_sky_light(chunk, index, level);
}

void LightEngine::mark_neighborhood(Chunk& chunk, const Vec3i& local_pos) {
    std::array<Chunk*, 27> chunks;
    std::array<uint32_t, 27> indices;
    resolve_neighborhood(chunk, local_pos.x, local_pos.y, local_pos.z,
                         chunks, indices);

    for (int i = 0; i < 27; ++i) {
        Chunk* target_chunk = chunks[i];
        if (!target_chunk) continue;
        uint32_t index = indices[i];
        bool center = (i == CENTER);

        if (center) {
            // Изменённый блок перепланируем принудительно: старый свет
            // должен быть снят, даже если блок уже был помечен.
            target_chunk->mark_light_dirty(index, false);
            target_chunk->light_queue_level[index] = 0;
            target_chunk->mark_sky_light_dirty(index, false);
            target_chunk->sky_light_queue_level[index] = 0;
        }

        if (target_chunk->light_data[index] > 0) {
            if (center) {
                target_chunk->mark_light_dirty(index, true);
                push_dark(*target_chunk, index, target_chunk->light_data[index]);
            } else {
                schedule_dark(*target_chunk, index);
            }
        } else {
            schedule_light_source(*target_chunk, index);
        }

        // Небесный свет пересчитываем консервативно для всего окружения.
        // После затемнения seed_sky_after_darkening восстановит прямой свет
        // столбца, даже если сбоку остался более яркий свет.
        if (target_chunk->sky_light_data[index] > 0) {
            schedule_sky_dark(*target_chunk, index);
        } else {
            const uint8_t direct = sky_column_level(
                *target_chunk, index_x(index), index_y(index), index_z(index));
            if (direct > 0) {
                schedule_sky_light(*target_chunk, index, direct);
            }
        }
    }
}

void LightEngine::mark_block_changed(Chunk& chunk, const Vec3i& local_pos,
                                     uint8_t old_opacity, uint8_t new_opacity,
                                     uint8_t old_light, uint8_t new_light) {
    uint32_t index = Chunk::coords_to_global_index(
        static_cast<uint8_t>(local_pos.x), static_cast<uint16_t>(local_pos.y),
        static_cast<uint8_t>(local_pos.z));

    const bool can_decrease = (new_opacity > old_opacity) || (new_light < old_light);
    const bool can_increase = (new_opacity < old_opacity) || (new_light > old_light);

    if (!can_decrease && !can_increase) {
        // Блок заменён на такой же по оптическим свойствам — поле света
        // не меняется. Снимаем флаг, который мог выставить Chunk::set_block.
        chunk.mark_light_dirty(index, false);
        return;
    }

    const bool dark_seeded = can_decrease && chunk.light_data[index] > 0;
    if (dark_seeded) {
        // Только изменённая клетка является семенем затемнения. Остальные
        // 26 клеток проверяются процессом затемнения и затрагиваются лишь
        // тогда, когда их свет действительно зависел от изменённого блока.
        chunk.mark_light_dirty(index, false);
        push_dark(chunk, index, chunk.light_data[index]);
    }

    if (can_increase && !dark_seeded) {
        // Свет может только вырасти: достаточно пересчитать изменённую
        // клетку и дать распространению дойти до соседей.
        if (can_use_fast(index)) {
            uint8_t level = compute_light_fast(chunk, index);
            schedule_light(chunk, index, level);
        } else {
            std::array<Chunk*, 27> chunks;
            std::array<uint32_t, 27> indices;
            resolve_neighborhood(chunk, local_pos.x, local_pos.y, local_pos.z,
                                 chunks, indices);
            uint8_t level = compute_light_slow(chunks, indices);
            schedule_light(chunk, index, level);
        }
    }

    // Небесный свет зависит только от прозрачности блоков. При изменении
    // opacity пересчитываем весь столбец вниз и 26 соседей изменённой
    // клетки: через неё мог проходить боковой свет.
    if (new_opacity != old_opacity) {
        const bool darker = new_opacity > old_opacity;
        mark_sky_column_changed(chunk, local_pos.x, local_pos.y,
                                local_pos.z, darker);

        std::array<Chunk*, 27> chunks;
        std::array<uint32_t, 27> indices;
        resolve_neighborhood(chunk, local_pos.x, local_pos.y, local_pos.z,
                             chunks, indices);
        for (int i = 0; i < 27; ++i) {
            if (i == CENTER || !chunks[i]) continue;
            Chunk& neighbor = *chunks[i];
            const uint32_t nindex = indices[i];
            if (darker) {
                if (neighbor.sky_light_data[nindex] > 0) {
                    schedule_sky_dark(neighbor, nindex);
                }
            } else {
                const uint8_t level = neighbor.sky_light_data[nindex];
                if (level > 0) {
                    schedule_sky_light(neighbor, nindex, level);
                }
            }
        }
    }
}

void LightEngine::mark_chunk_sources(Chunk& chunk) {
    // При загрузке чанка сразу заполняем прямой свет неба, чтобы меш мог
    // использовать его даже до первого update(). Распространение вбок
    // выполнится в update().
    initialize_chunk_sky(chunk);

    const BlockManager& block_mgr = world.get_block_mgr();
    for (auto& nugget : chunk.nuggets) {
        uint16_t base_y = static_cast<uint16_t>(nugget.local_y_index *
                                               ChunkNugget::SIZE);
        for (uint16_t i = 0; i < ChunkNugget::BLOCKS_COUNT; ++i) {
            uint16_t id = nugget.block_ids[i];
            if (id == 0) continue;

            const Block* block = block_mgr.get_block_raw(id);
            if (!block || block->light == 0) continue;

            Vec3i lc = ChunkNugget::index_to_coords(i);
            uint32_t index = Chunk::coords_to_global_index(
                static_cast<uint8_t>(lc.x),
                static_cast<uint16_t>(base_y + lc.y),
                static_cast<uint8_t>(lc.z));
            schedule_light(chunk, index, block->light);
        }
    }
}

void LightEngine::mark_chunk_border(Chunk& chunk, bool allow_decrease) {
    auto process = [&](int x, int y, int z) {
        uint32_t index = Chunk::coords_to_global_index(
            static_cast<uint8_t>(x), static_cast<uint16_t>(y),
            static_cast<uint8_t>(z));
        if (allow_decrease) {
            // Свет в соседних чанках мог уменьшиться — запускаем затемнение.
            schedule_dark(chunk, index);
            if (chunk.sky_light_data[index] > 0) {
                schedule_sky_dark(chunk, index);
            }
        } else {
            // Свет может только вырасти: пересчитываем клетку и даём свету
            // распространиться в только что загруженное пространство.
            uint8_t level = chunk.light_data[index];
            if (level > 0) schedule_light(chunk, index, level);

            uint8_t sky = chunk.sky_light_data[index];
            if (sky > 0) schedule_sky_light(chunk, index, sky);
        }
    };

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            process(x, y, 0);
            process(x, y, SIZE - 1);
        }
        for (int z = 1; z < SIZE - 1; ++z) {
            process(0, y, z);
            process(SIZE - 1, y, z);
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
    erase_from(sky_dark_lists);
    erase_from(sky_light_lists);
    darkened_blocks.erase(std::remove_if(darkened_blocks.begin(), darkened_blocks.end(),
        [chunk](const Entry& e) { return e.chunk == chunk; }), darkened_blocks.end());
    sky_darkened_blocks.erase(std::remove_if(sky_darkened_blocks.begin(), sky_darkened_blocks.end(),
        [chunk](const Entry& e) { return e.chunk == chunk; }), sky_darkened_blocks.end());

    for (auto it = changed_nuggets.begin(); it != changed_nuggets.end();) {
        if (*it && (*it)->parent == chunk) {
            it = changed_nuggets.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<Chunk*> LightEngine::consume_changed_chunks() {
    std::unordered_set<Chunk*> unique_chunks;
    for (ChunkNugget* nugget : changed_nuggets) {
        if (nugget && nugget->parent) {
            unique_chunks.insert(nugget->parent);
        }
    }
    changed_nuggets.clear();
    return std::vector<Chunk*>(unique_chunks.begin(), unique_chunks.end());
}

void LightEngine::clear_changed_nuggets() {
    changed_nuggets.clear();
}

void LightEngine::mark_nugget_changed(Chunk& chunk, uint32_t index) {
    const int y = index_y(index);
    if (y < 0 || y >= HEIGHT) {
        return;
    }
    changed_nuggets.insert(&chunk.nuggets[y / static_cast<int>(ChunkNugget::SIZE)]);
}

bool LightEngine::has_pending() const {
    return has_dark_pending() || has_light_pending() ||
           has_sky_dark_pending() || has_sky_light_pending();
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

bool LightEngine::has_sky_dark_pending() const {
    for (const auto& list : sky_dark_lists.fast) if (!list.empty()) return true;
    for (const auto& list : sky_dark_lists.slow) if (!list.empty()) return true;
    return false;
}

bool LightEngine::has_sky_light_pending() const {
    for (const auto& list : sky_light_lists.fast) if (!list.empty()) return true;
    for (const auto& list : sky_light_lists.slow) if (!list.empty()) return true;
    return false;
}

void LightEngine::clear_queues() {
    auto reset_dark = [](std::array<std::vector<Entry>, LEVEL_COUNT>& lists) {
        for (auto& list : lists) {
            for (const Entry& entry : list) {
                entry.chunk->mark_light_dirty(entry.index, false);
            }
            list.clear();
        }
    };

    auto reset_light = [](std::array<std::vector<Entry>, LEVEL_COUNT>& lists) {
        for (int level = 1; level <= LEVEL_COUNT; ++level) {
            for (const Entry& entry : lists[level - 1]) {
                entry.chunk->mark_light_dirty(entry.index, false);
                if (entry.chunk->light_queue_level[entry.index] == level) {
                    entry.chunk->light_queue_level[entry.index] = 0;
                }
            }
            lists[level - 1].clear();
        }
    };

    auto reset_sky_dark = [](std::array<std::vector<Entry>, LEVEL_COUNT>& lists) {
        for (auto& list : lists) {
            for (const Entry& entry : list) {
                entry.chunk->mark_sky_light_dirty(entry.index, false);
            }
            list.clear();
        }
    };

    auto reset_sky_light = [](std::array<std::vector<Entry>, LEVEL_COUNT>& lists) {
        for (int level = 1; level <= LEVEL_COUNT; ++level) {
            for (const Entry& entry : lists[level - 1]) {
                entry.chunk->mark_sky_light_dirty(entry.index, false);
                if (entry.chunk->sky_light_queue_level[entry.index] == level) {
                    entry.chunk->sky_light_queue_level[entry.index] = 0;
                }
            }
            lists[level - 1].clear();
        }
    };

    reset_dark(dark_lists.fast);
    reset_dark(dark_lists.slow);
    reset_light(light_lists.fast);
    reset_light(light_lists.slow);
    reset_sky_dark(sky_dark_lists.fast);
    reset_sky_dark(sky_dark_lists.slow);
    reset_sky_light(sky_light_lists.fast);
    reset_sky_light(sky_light_lists.slow);
    darkened_blocks.clear();
    sky_darkened_blocks.clear();
}


// ============================================================================
// Затемнение
// ============================================================================

void LightEngine::process_dark_entry(const Entry& entry, bool fast) {
    Chunk& chunk = *entry.chunk;
    uint32_t index = entry.index;

    uint8_t light = chunk.light_data[index];
    chunk.mark_light_dirty(index, false);
    if (light == 0) return;

    if (fast) {
        int y = index_y(index);
        fill_full_opacity_fast(chunk, index, y);

        const uint8_t* light_data = chunk.light_data.data();
        const uint8_t* opacity_data = chunk.opacity_data.data();

        for (int i = 0; i < 27; ++i) {
            if (i == CENTER) continue;
            const Offset& o = OFFSETS[i];
            int ny = y + o.dy;
            if (ny < 0 || ny >= HEIGHT) continue;

            uint32_t nindex = index + o.delta;
            uint8_t nlight = light_data[nindex];
            if (nlight == 0) continue;

            int n_opacity = opacity_data[nindex];
            int expected_forward = static_cast<int>(light) - full_opacity[i];
            int expected_reverse = static_cast<int>(light) - o.dist - n_opacity;
            int expected = std::max(expected_forward, expected_reverse);
            if (nlight <= expected) schedule_dark(chunk, nindex);
        }
    } else {
        std::array<Chunk*, 27> chunks;
        std::array<uint32_t, 27> indices;
        resolve_neighborhood(chunk, index_x(index), index_y(index), index_z(index),
                             chunks, indices);
        fill_full_opacity_slow(chunk, index, chunks, indices);

        for (int i = 0; i < 27; ++i) {
            if (i == CENTER || !chunks[i]) continue;
            Chunk& nchunk = *chunks[i];
            uint32_t nindex = indices[i];

            uint8_t nlight = nchunk.light_data[nindex];
            if (nlight == 0) continue;

            int n_opacity = nchunk.opacity_data[nindex];
            int expected_forward = static_cast<int>(light) - full_opacity[i];
            int expected_reverse = static_cast<int>(light) - OFFSETS[i].dist - n_opacity;
            int expected = std::max(expected_forward, expected_reverse);
            if (nlight <= expected) schedule_dark(nchunk, nindex);
        }
    }

    chunk.light_data[index] = 0;
    mark_nugget_changed(chunk, index);
    darkened_blocks.push_back(entry);
}

void LightEngine::seed_after_darkening() {
    for (const Entry& entry : darkened_blocks) {
        Chunk& chunk = *entry.chunk;
        uint32_t index = entry.index;

        if (chunk.light_data[index] != 0) continue;

        int center_opacity = chunk.opacity_data[index];
        int best = 0;

        if (can_use_fast(index)) {
            int y = index_y(index);
            const uint8_t* light_data = chunk.light_data.data();
            for (int i = 0; i < 27; ++i) {
                if (i == CENTER) continue;
                const Offset& o = OFFSETS[i];
                int ny = y + o.dy;
                if (ny < 0 || ny >= HEIGHT) continue;
                int candidate = static_cast<int>(light_data[index + o.delta]) -
                                o.dist - center_opacity;
                if (candidate > best) best = candidate;
            }
        } else {
            std::array<Chunk*, 27> chunks;
            std::array<uint32_t, 27> indices;
            resolve_neighborhood(chunk, index_x(index), index_y(index), index_z(index),
                                 chunks, indices);
            for (int i = 0; i < 27; ++i) {
                if (i == CENTER || !chunks[i]) continue;
                int candidate = static_cast<int>(chunks[i]->light_data[indices[i]]) -
                                OFFSETS[i].dist - center_opacity;
                if (candidate > best) best = candidate;
            }
        }

        int emission_level = emission(chunk, index);
        int seed = std::max(best, emission_level);

        if (seed > 0) {
            schedule_light(chunk, index, static_cast<uint8_t>(std::min(seed, 255)));
        }
    }
    darkened_blocks.clear();
}

// ============================================================================
// Небесный свет: затемнение и распространение
// ============================================================================

void LightEngine::process_sky_dark_entry(const Entry& entry, bool fast) {
    Chunk& chunk = *entry.chunk;
    uint32_t index = entry.index;

    uint8_t light = chunk.sky_light_data[index];
    chunk.mark_sky_light_dirty(index, false);
    // Любая отложенная запись на распространение для этой клетки устарела:
    // после затемнения свет будет восстановлен через seed_sky_after_darkening.
    chunk.sky_light_queue_level[index] = 0;
    if (light == 0) {
        // Служебная запись после отмены отложенного увеличения: клетку
        // всё равно нужно пересчитать в seed_sky_after_darkening().
        sky_darkened_blocks.push_back(entry);
        return;
    }

    if (fast) {
        int y = index_y(index);
        fill_full_opacity_fast(chunk, index, y);

        const uint8_t* light_data = chunk.sky_light_data.data();
        const uint8_t* opacity_data = chunk.opacity_data.data();

        for (int i = 0; i < 27; ++i) {
            if (i == CENTER) continue;
            const Offset& o = OFFSETS[i];
            int ny = y + o.dy;
            if (ny < 0 || ny >= HEIGHT) continue;

            uint32_t nindex = index + o.delta;
            uint8_t nlight = light_data[nindex];
            if (nlight == 0) continue;

            int n_opacity = opacity_data[nindex];
            int expected = static_cast<int>(light) - full_opacity[i];
            // Для рёбер и углов full_opacity описывает соседние стороны,
            // но не саму клетку назначения. Её opacity нужно вычесть
            // отдельно, иначе полупрозрачный блок пересветится.
            if (o.dist != 8) expected -= n_opacity;
            if (nlight <= expected) schedule_sky_dark(chunk, nindex);
        }
    } else {
        std::array<Chunk*, 27> chunks;
        std::array<uint32_t, 27> indices;
        resolve_neighborhood(chunk, index_x(index), index_y(index), index_z(index),
                             chunks, indices);
        fill_full_opacity_slow(chunk, index, chunks, indices);

        for (int i = 0; i < 27; ++i) {
            if (i == CENTER || !chunks[i]) continue;
            Chunk& nchunk = *chunks[i];
            uint32_t nindex = indices[i];

            uint8_t nlight = nchunk.sky_light_data[nindex];
            if (nlight == 0) continue;

            int n_opacity = nchunk.opacity_data[nindex];
            int expected = static_cast<int>(light) - full_opacity[i];
            if (OFFSETS[i].dist != 8) expected -= n_opacity;
            if (nlight <= expected) schedule_sky_dark(nchunk, nindex);
        }
    }

    chunk.sky_light_data[index] = 0;
    mark_nugget_changed(chunk, index);
    sky_darkened_blocks.push_back(entry);
}

void LightEngine::seed_sky_after_darkening() {
    for (const Entry& entry : sky_darkened_blocks) {
        Chunk& chunk = *entry.chunk;
        uint32_t index = entry.index;

        if (chunk.sky_light_data[index] != 0) continue;

        int center_opacity = chunk.opacity_data[index];
        int best = 0;

        if (can_use_fast(index)) {
            int y = index_y(index);
            fill_full_opacity_fast(chunk, index, y);
            const uint8_t* light_data = chunk.sky_light_data.data();
            for (int i = 0; i < 27; ++i) {
                if (i == CENTER) continue;
                const Offset& o = OFFSETS[i];
                int ny = y + o.dy;
                if (ny < 0 || ny >= HEIGHT) continue;
                int candidate = static_cast<int>(light_data[index + o.delta]) -
                                center_opacity -
                                (o.dist == 8 ? o.dist : full_opacity[i]);
                if (candidate > best) best = candidate;
            }
        } else {
            std::array<Chunk*, 27> chunks;
            std::array<uint32_t, 27> indices;
            resolve_neighborhood(chunk, index_x(index), index_y(index), index_z(index),
                                 chunks, indices);
            fill_full_opacity_slow(chunk, index, chunks, indices);
            for (int i = 0; i < 27; ++i) {
                if (i == CENTER || !chunks[i]) continue;
                const Offset& o = OFFSETS[i];
                int candidate = static_cast<int>(chunks[i]->sky_light_data[indices[i]]) -
                                center_opacity -
                                (o.dist == 8 ? o.dist : full_opacity[i]);
                if (candidate > best) best = candidate;
            }
        }

        // Прямой свет неба — самостоятельный источник, который не должен
        // зависеть от бокового света. Поэтому берём максимум из бокового
        // распространения и прямого уровня столбца.
        const int direct = sky_column_level(
            chunk, index_x(index), index_y(index), index_z(index));
        int seed = std::max(best, direct);

        if (seed > 0) {
            schedule_sky_light(chunk, index,
                               static_cast<uint8_t>(std::min(seed, 255)));
        }
    }
    sky_darkened_blocks.clear();
}

void LightEngine::remove_sky_light() {
    while (has_sky_dark_pending()) {
        for (int level = LEVEL_COUNT; level >= 1; --level) {
            auto& fast_list = sky_dark_lists.fast[level - 1];
            for (size_t i = 0; i < fast_list.size(); ++i) {
                process_sky_dark_entry(fast_list[i], true);
            }
            fast_list.clear();

            auto& slow_list = sky_dark_lists.slow[level - 1];
            for (size_t i = 0; i < slow_list.size(); ++i) {
                process_sky_dark_entry(slow_list[i], false);
            }
            slow_list.clear();
        }
    }

    seed_sky_after_darkening();
}

void LightEngine::process_sky_light_entry(const Entry& entry, int level, bool fast) {
    Chunk& chunk = *entry.chunk;
    uint32_t index = entry.index;

    uint8_t queued = chunk.sky_light_queue_level[index];
    // Обрабатываем только актуальную запись. Более низкие записи могли
    // устареть после затемнения или после распространения более яркого
    // света; более высокие ещё ждут своей очереди.
    if (queued != level) {
        if (queued == 0) {
            chunk.mark_sky_light_dirty(index, false);
        }
        return;
    }

    uint8_t current = chunk.sky_light_data[index];
    chunk.sky_light_queue_level[index] = 0;
    chunk.mark_sky_light_dirty(index, false);

    // Небесный свет только добавляет яркость: прямое значение столбца
    // не может быть погашено более слабым боковым светом. Уровень из
    // очереди тоже является нижней границей (для прямого света это
    // значение столбца, для распространения — яркость соседа).
    int new_light = std::max(static_cast<int>(current), level);
    // Собственная opacity клетки ограничивает свет сверху. Для боковых
    // переходов она уже входит в full_opacity, для рёбер и углов вычитается
    // отдельно при распространении; этот clamp остаётся страховкой.
    new_light = std::min(new_light,
                         255 - static_cast<int>(chunk.opacity_data[index]));

    new_light = std::max(0, std::min(new_light, 255));
    const uint8_t new_light_u8 = static_cast<uint8_t>(new_light);
    if (chunk.sky_light_data[index] != new_light_u8) {
        mark_nugget_changed(chunk, index);
    }
    chunk.sky_light_data[index] = new_light_u8;

    if (fast) {
        int y = index_y(index);
        fill_full_opacity_fast(chunk, index, y);

        uint8_t* light_mut = chunk.sky_light_data.data();
        const uint8_t* opacity_mut = chunk.opacity_data.data();
        for (int i = 0; i < 27; ++i) {
            if (i == CENTER) continue;
            const Offset& o = OFFSETS[i];
            int ny = y + o.dy;
            if (ny < 0 || ny >= HEIGHT) continue;

            uint32_t nindex = index + o.delta;
            int target = new_light - full_opacity[i];
            if (o.dist != 8) target -= opacity_mut[nindex];
            if (target <= 0) continue;
            if (light_mut[nindex] < target) {
                schedule_sky_light(chunk, nindex,
                                   static_cast<uint8_t>(std::min(target, 255)));
            }
        }
    } else {
        std::array<Chunk*, 27> chunks;
        std::array<uint32_t, 27> indices;
        resolve_neighborhood(chunk, index_x(index), index_y(index), index_z(index),
                             chunks, indices);
        fill_full_opacity_slow(chunk, index, chunks, indices);

        for (int i = 0; i < 27; ++i) {
            if (i == CENTER || !chunks[i]) continue;
            Chunk& nchunk = *chunks[i];
            uint32_t nindex = indices[i];
            int target = new_light - full_opacity[i];
            if (OFFSETS[i].dist != 8) target -= nchunk.opacity_data[nindex];
            if (target <= 0) continue;

            if (nchunk.sky_light_data[nindex] < target) {
                schedule_sky_light(nchunk, nindex,
                                   static_cast<uint8_t>(std::min(target, 255)));
            }
        }
    }
}

void LightEngine::propagate_sky_light() {
    while (has_sky_light_pending()) {
        for (int level = LEVEL_COUNT; level >= 1; --level) {
            auto& fast_list = sky_light_lists.fast[level - 1];
            for (size_t i = 0; i < fast_list.size(); ++i) {
                process_sky_light_entry(fast_list[i], level, true);
            }
            fast_list.clear();

            auto& slow_list = sky_light_lists.slow[level - 1];
            for (size_t i = 0; i < slow_list.size(); ++i) {
                process_sky_light_entry(slow_list[i], level, false);
            }
            slow_list.clear();
        }
    }
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
    uint32_t index = entry.index;

    uint8_t queued = chunk.light_queue_level[index];
    if (level < queued) return; // есть запись с более высоким приоритетом

    uint8_t current = chunk.light_data[index];
    if (queued == level) {
        chunk.light_queue_level[index] = 0;
        chunk.mark_light_dirty(index, false);
    } else {
        // Записи в очереди уже нет: если свет не ниже её уровня, работа сделана.
        chunk.mark_light_dirty(index, false);
        if (current >= level) return;
    }

    if (fast) {
        int y = index_y(index);
        fill_full_opacity_fast(chunk, index, y);

        int center_opacity = full_opacity[CENTER];
        int new_light = emission(chunk, index);

        const uint8_t* light_data = chunk.light_data.data();
        for (int i = 0; i < 27; ++i) {
            if (i == CENTER) continue;
            const Offset& o = OFFSETS[i];
            int ny = y + o.dy;
            if (ny < 0 || ny >= HEIGHT) continue;
            int candidate = static_cast<int>(light_data[index + o.delta]) -
                            o.dist - center_opacity;
            if (candidate > new_light) new_light = candidate;
        }

        new_light = std::max(0, std::min(new_light, 255));
        const uint8_t new_light_u8 = static_cast<uint8_t>(new_light);
        if (chunk.light_data[index] != new_light_u8) {
            mark_nugget_changed(chunk, index);
        }
        chunk.light_data[index] = new_light_u8;

        uint8_t* light_mut = chunk.light_data.data();
        for (int i = 0; i < 27; ++i) {
            if (i == CENTER) continue;
            int target = new_light - full_opacity[i];
            if (target <= 0) continue;
            const Offset& o = OFFSETS[i];
            int ny = y + o.dy;
            if (ny < 0 || ny >= HEIGHT) continue;

            uint32_t nindex = index + o.delta;
            if (light_mut[nindex] < target) {
                schedule_light(chunk, nindex,
                               static_cast<uint8_t>(std::min(target, 255)));
            }
        }
    } else {
        std::array<Chunk*, 27> chunks;
        std::array<uint32_t, 27> indices;
        resolve_neighborhood(chunk, index_x(index), index_y(index), index_z(index),
                             chunks, indices);
        fill_full_opacity_slow(chunk, index, chunks, indices);

        int center_opacity = full_opacity[CENTER];
        int new_light = emission(chunk, index);

        for (int i = 0; i < 27; ++i) {
            if (i == CENTER || !chunks[i]) continue;
            int candidate = static_cast<int>(chunks[i]->light_data[indices[i]]) -
                            OFFSETS[i].dist - center_opacity;
            if (candidate > new_light) new_light = candidate;
        }

        new_light = std::max(0, std::min(new_light, 255));
        const uint8_t new_light_u8 = static_cast<uint8_t>(new_light);
        if (chunk.light_data[index] != new_light_u8) {
            mark_nugget_changed(chunk, index);
        }
        chunk.light_data[index] = new_light_u8;

        for (int i = 0; i < 27; ++i) {
            if (i == CENTER || !chunks[i]) continue;
            int target = new_light - full_opacity[i];
            if (target <= 0) continue;

            Chunk& nchunk = *chunks[i];
            uint32_t nindex = indices[i];
            if (nchunk.light_data[nindex] < target) {
                schedule_light(nchunk, nindex,
                               static_cast<uint8_t>(std::min(target, 255)));
            }
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
    remove_sky_light();
    propagate_sky_light();
}
