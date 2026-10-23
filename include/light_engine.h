#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "vec.h"
#include "chunk.h"

class World;


// Освещение воксельного мира.
//
// Алгоритм состоит из двух проходов:
//   1. remove_light()  - снятие старого света (затемнение);
//   2. propagate_light() - распространение света от оставшихся источников.
//
// Для каждого прохода используется по 510 очередей: 255 уровней яркости
// (1..255, уровень 0 не нужен) x 2 версии (fast/slow). fast работает только
// внутри одного чанка, slow умеет читать соседние чанки.
//
// Горячие циклы (обработка одной воксельной клетки) не используют
// world.get_chunk()/shared_ptr: соседние чанки берутся из уже готовых
// указателей Chunk::horizontal_neighbors, а яркость блока — из BlockManager
// без создания shared_ptr. Смещения 3x3x3 и расстояния закешированы.
class LightEngine {
public:
    static constexpr int LEVEL_COUNT = 255;
    static constexpr int CENTER = 13;  // index(0, 0, 0) в массиве 3x3x3

    explicit LightEngine(World& world);

    // Полный цикл: сначала затемнение, затем освещение.
    void update();

    // Только затемнение.
    void remove_light();
    // Только распространение света.
    void propagate_light();

    // Поставить на обновление сам блок и 26 его соседей.
    // Консервативный вариант (совместимость со старым API).
    void mark_neighborhood(Chunk& chunk, const Vec3i& local_pos);

    // Обновляет только те клетки, где свет действительно мог измениться.
    // old_* — состояние блока до изменения, new_* — после.
    void mark_block_changed(Chunk& chunk, const Vec3i& local_pos,
                            uint8_t old_opacity, uint8_t new_opacity,
                            uint8_t old_light, uint8_t new_light);

    // Поставить на обновление источники света в чанке (при загрузке чанка).
    void mark_chunk_sources(Chunk& chunk);

    // Поставить на пересчёт освещённые блоки на границе чанка.
    // allow_decrease == true нужен при удалении/замене чанка: свет соседей
    // мог уменьшиться. При обычной загрузке (новых блоков раньше не было)
    // свет может только увеличиться, поэтому затемнение не требуется.
    void mark_chunk_border(Chunk& chunk, bool allow_decrease = false);

    // Убрать из очередей ссылки на удалённый чанк.
    void forget_chunk(Chunk* chunk);

    bool has_pending() const;
    bool has_dark_pending() const;
    bool has_light_pending() const;
    void clear_queues();

private:
    // Плоский индекс клетки в массивах чанка вместо Vec3i: меньше памяти
    // в очередях и никаких пересчётов координат при постановке в очередь.
    struct Entry {
        Chunk* chunk = nullptr;
        uint32_t index = 0;
    };

    struct LevelLists {
        std::array<std::vector<Entry>, LEVEL_COUNT> fast;
        std::array<std::vector<Entry>, LEVEL_COUNT> slow;
    };

    World& world;

    LevelLists dark_lists;
    LevelLists light_lists;

    // Блоки, погашенные в текущем проходе. Семена для освещения ставим
    // после завершения всего затемнения, когда финальное поле света известно.
    std::vector<Entry> darkened_blocks;

    // full_opacity для 3x3x3 вокруг текущего блока.
    std::array<uint8_t, 27> full_opacity;

    // --- доступ к соседям ---
    void resolve_neighborhood(const Chunk& base, int x, int y, int z,
                              std::array<Chunk*, 27>& out_chunks,
                              std::array<uint32_t, 27>& out_indices) const;
    static bool can_use_fast(uint32_t index);

    // --- full_opacity ---
    void fill_full_opacity_fast(const Chunk& chunk, uint32_t index, int y);
    void fill_full_opacity_slow(const Chunk& chunk, uint32_t center_index,
                                const std::array<Chunk*, 27>& chunks,
                                const std::array<uint32_t, 27>& indices);

    // --- эмиссия блока ---
    uint8_t emission(const Chunk& chunk, uint32_t index) const;

    // Текущая яркость клетки с учётом окружения (для семени распространения).
    uint8_t compute_light_fast(const Chunk& chunk, uint32_t index) const;
    uint8_t compute_light_slow(const std::array<Chunk*, 27>& chunks,
                               const std::array<uint32_t, 27>& indices) const;

    // --- очереди ---
    void push_dark(Chunk& chunk, uint32_t index, uint8_t level);
    void push_light(Chunk& chunk, uint32_t index, uint8_t level);
    void schedule_dark(Chunk& chunk, uint32_t index);
    void schedule_light(Chunk& chunk, uint32_t index, uint8_t level);
    void schedule_light_source(Chunk& chunk, uint32_t index);

    // --- обработка ---
    void process_dark_entry(const Entry& entry, bool fast);
    void process_light_entry(const Entry& entry, int level, bool fast);
    void seed_after_darkening();
};
