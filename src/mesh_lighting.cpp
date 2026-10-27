#include "mesh_lighting.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "chunk.h"
#include "mesh.h"
#include "world.h"


namespace {

constexpr int kMaxSearchRadius = 8;

struct Candidate {
    float distance = 0.0f;
    uint8_t light = 0;
};

uint8_t read_light(const World& world, int32_t x, int32_t y, int32_t z) {
    if (y < 0 || y >= static_cast<int32_t>(Chunk::HEIGHT)) {
        return 0;
    }

    const Vec2i chunk_pos = World::world_to_chunk_coords(Vec3i(x, y, z));
    const Chunk* chunk = world.get_chunk(chunk_pos);
    if (!chunk) {
        return 0;
    }

    const Vec3i local = World::world_to_local_coords(Vec3i(x, y, z));
    return chunk->light_data[Chunk::coords_to_global_index(
        static_cast<uint8_t>(local.x),
        static_cast<uint16_t>(local.y),
        static_cast<uint8_t>(local.z))];
}

} // namespace


namespace MeshLighting {

uint8_t sample_vertex_light(const World& world,
                            const Vec3f& world_position,
                            int search_radius) {
    search_radius = std::max(0, std::min(search_radius, kMaxSearchRadius));

    const int32_t center_x = static_cast<int32_t>(
        std::floor(world_position.x));
    const int32_t center_y = static_cast<int32_t>(
        std::floor(world_position.y));
    const int32_t center_z = static_cast<int32_t>(
        std::floor(world_position.z));

    Candidate nearest[MAX_SOURCES];
    int count = 0;

    for (int32_t dz = -search_radius; dz <= search_radius; ++dz) {
        for (int32_t dy = -search_radius; dy <= search_radius; ++dy) {
            for (int32_t dx = -search_radius; dx <= search_radius; ++dx) {
                const int32_t x = center_x + dx;
                const int32_t y = center_y + dy;
                const int32_t z = center_z + dz;

                const uint8_t light = read_light(world, x, y, z);
                if (light == 0) {
                    continue;
                }

                // Центр клетки, а не её угол: вершина на границе блоков
                // должна получать вклад от обеих сторон.
                const float cell_x = static_cast<float>(x) + 0.5f;
                const float cell_y = static_cast<float>(y) + 0.5f;
                const float cell_z = static_cast<float>(z) + 0.5f;
                const float distance =
                    std::abs(cell_x - world_position.x) +
                    std::abs(cell_y - world_position.y) +
                    std::abs(cell_z - world_position.z);

                // Вставка с сохранением порядка по расстоянию. Так хранятся
                // только четыре ближайших кандидата, без сортировки всего
                // окружения.
                int insert_at = 0;
                while (insert_at < count &&
                       nearest[insert_at].distance <= distance) {
                    ++insert_at;
                }
                if (insert_at >= MAX_SOURCES) {
                    continue;
                }

                const int last = std::min(count, MAX_SOURCES - 1);
                for (int i = last; i > insert_at; --i) {
                    nearest[i] = nearest[i - 1];
                }
                nearest[insert_at].distance = distance;
                nearest[insert_at].light = light;
                if (count < MAX_SOURCES) {
                    ++count;
                }
            }
        }
    }

    if (count == 0) {
        return 0;
    }

    float weighted_sum = 0.0f;
    float weight_sum = 0.0f;
    for (int i = 0; i < count; ++i) {
        // Линейный обратный вес: без корней и степеней.
        const float weight = 1.0f / (1.0f + nearest[i].distance);
        weighted_sum += static_cast<float>(nearest[i].light) * weight;
        weight_sum += weight;
    }

    if (weight_sum <= 0.0f) {
        return 0;
    }

    const float value = weighted_sum / weight_sum;
    return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, value)));
}

bool update_mesh_lighting(const World& world,
                          Mesh& mesh,
                          const Vec3f& origin,
                          int search_radius) {
    bool changed = false;

    for (Vertex& vertex : mesh.vertices) {
        const Vec3f world_position(
            origin.x + vertex.x,
            origin.y + vertex.y,
            origin.z + vertex.z);
        const uint8_t light = sample_vertex_light(
            world, world_position, search_radius);
        if (vertex.block_light != light) {
            vertex.block_light = light;
            changed = true;
        }
    }

    if (changed) {
        ++mesh.light_revision;
    }
    return changed;
}

} // namespace MeshLighting
