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
    void mark_neighborhood(Chunk& chunk, const Vec3i& local_pos);

    // Поставить на обновление источники света в чанке (при загрузке чанка).
    void mark_chunk_sources(Chunk& chunk);

    // Поставить на обновление освещённые блоки на границе чанка.
    // Нужно, когда рядом загрузился новый чанк и свет должен пересчитаться.
    void mark_chunk_border(Chunk& chunk);

    // Убрать из очередей ссылки на удалённый чанк.
    void forget_chunk(Chunk* chunk);

    bool has_pending() const;
    bool has_dark_pending() const;
    bool has_light_pending() const;
    void clear_queues();

private:
    struct Entry {
        Chunk* chunk = nullptr;
        Vec3i local;
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

    // --- full_opacity ---
    void fill_full_opacity_fast(const Chunk& chunk, int x, int y, int z);
    void fill_full_opacity_slow(const Chunk& chunk, int x, int y, int z);

    // --- fast/slow доступ к данным ---
    static uint8_t opacity_fast(const Chunk& chunk, int x, int y, int z);
    uint8_t opacity_slow(const Chunk& chunk, int x, int y, int z) const;

    // Найти блок по локальной координате, возможно в соседнем чанке.
    bool locate_neighbor(const Chunk& base, int x, int y, int z,
                         Chunk*& out_chunk, Vec3i& out_local) const;

    static bool can_use_fast(int x, int y, int z);

    // --- очереди ---
    void push_dark(Chunk& chunk, const Vec3i& local, uint8_t level);
    void push_light(Chunk& chunk, const Vec3i& local, uint8_t level);
    void schedule_dark(Chunk& chunk, const Vec3i& local);
    void schedule_light(Chunk& chunk, const Vec3i& local, uint8_t level);
    void schedule_light_source(Chunk& chunk, const Vec3i& local);

    // --- обработка ---
    void process_dark_entry(const Entry& entry, bool fast);
    void process_light_entry(const Entry& entry, int level, bool fast);
    void seed_after_darkening();

    static int offset_index(int dx, int dy, int dz);
    static void index_offset(int index, int& dx, int& dy, int& dz);
    static int offset_distance(int dx, int dy, int dz);
};
