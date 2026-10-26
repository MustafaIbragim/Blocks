#include "voxel_collision_shape.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionShapes/btShapeHull.h>
#include <BulletCollision/CollisionShapes/btTriangleCallback.h>

#include "block.h"
#include "chunk.h"
#include "world.h"


namespace {

constexpr btScalar kScaleEpsilon = btScalar(1e-6);

inline int floor_to_int(btScalar value) {
    return static_cast<int>(std::floor(value));
}

// Максимальный индекс блока, который ещё пересекает [.., value].
// Для value = 32.0 это 31, для value = 32.1 — 32.
inline int ceil_to_block(btScalar value) {
    return static_cast<int>(std::ceil(value)) - 1;
}

inline int floor_div(int value, int divisor) {
    int q = value / divisor;
    int r = value % divisor;
    if (r != 0 && ((r < 0) != (divisor < 0))) {
        --q;
    }
    return q;
}

inline int64_t chunk_key(int x, int z) {
    return (static_cast<int64_t>(static_cast<uint32_t>(x)) << 32) |
           static_cast<uint32_t>(z);
}

inline bool aabb_overlap(const btVector3& min_a, const btVector3& max_a,
                         const btVector3& min_b, const btVector3& max_b) {
    return min_a.x() <= max_b.x() && max_a.x() >= min_b.x() &&
           min_a.y() <= max_b.y() && max_a.y() >= min_b.y() &&
           min_a.z() <= max_b.z() && max_a.z() >= min_b.z();
}

inline bool triangle_overlaps_aabb(const btVector3& a,
                                   const btVector3& b,
                                   const btVector3& c,
                                   const btVector3& aabb_min,
                                   const btVector3& aabb_max) {
    btVector3 tri_min = a;
    btVector3 tri_max = a;
    tri_min.setMin(b);
    tri_min.setMin(c);
    tri_max.setMax(b);
    tri_max.setMax(c);
    return aabb_overlap(tri_min, tri_max, aabb_min, aabb_max);
}

void transform_aabb(const btTransform& transform,
                    const btVector3& local_min,
                    const btVector3& local_max,
                    btVector3& out_min,
                    btVector3& out_max) {
    btVector3 corners[8] = {
        btVector3(local_min.x(), local_min.y(), local_min.z()),
        btVector3(local_max.x(), local_min.y(), local_min.z()),
        btVector3(local_max.x(), local_max.y(), local_min.z()),
        btVector3(local_min.x(), local_max.y(), local_min.z()),
        btVector3(local_min.x(), local_min.y(), local_max.z()),
        btVector3(local_max.x(), local_min.y(), local_max.z()),
        btVector3(local_max.x(), local_max.y(), local_max.z()),
        btVector3(local_min.x(), local_max.y(), local_max.z())
    };

    out_min = transform * corners[0];
    out_max = out_min;
    for (int i = 1; i < 8; ++i) {
        btVector3 p = transform * corners[i];
        out_min.setMin(p);
        out_max.setMax(p);
    }
}

inline btVector3 safe_scale(const btVector3& scale) {
    return btVector3(
        scale.x() > kScaleEpsilon ? scale.x() : btScalar(1.0),
        scale.y() > kScaleEpsilon ? scale.y() : btScalar(1.0),
        scale.z() > kScaleEpsilon ? scale.z() : btScalar(1.0));
}

} // namespace


VoxelCollisionShape::VoxelCollisionShape(World& world_ref, const Vec2i& chunk_pos)
    : world(&world_ref),
      local_min(btScalar(-1), btScalar(0), btScalar(-1)),
      local_max(btScalar(Chunk::SIZE_XZ+1), btScalar(Chunk::HEIGHT), btScalar(Chunk::SIZE_XZ+1)),
      world_origin(btScalar(chunk_pos.x * static_cast<int32_t>(Chunk::SIZE_XZ)),
                   btScalar(0),
                   btScalar(chunk_pos.y * static_cast<int32_t>(Chunk::SIZE_XZ))),
      local_scaling(btScalar(1), btScalar(1), btScalar(1)) {
    // CUSTOM_CONCAVE_SHAPE_TYPE в этой версии Bullet совпадает с SDF-shape,
    // поэтому используем обычный concave triangle-mesh тип. Благодаря этому
    // dispatcher выбирает btConvexConcaveCollisionAlgorithm и вызывает
    // processAllTriangles.
    m_shapeType = TRIANGLE_MESH_SHAPE_PROXYTYPE;
    setMargin(btScalar(0.0));
}

VoxelCollisionShape::VoxelCollisionShape(World& world_ref,
                                         const btVector3& local_min_in,
                                         const btVector3& local_max_in,
                                         const btVector3& world_origin_in)
    : world(&world_ref),
      local_min(local_min_in),
      local_max(local_max_in),
      world_origin(world_origin_in),
      local_scaling(btScalar(1), btScalar(1), btScalar(1)) {
    btVector3 real_min = local_min_in;
    btVector3 real_max = local_max_in;
    real_min.setMin(local_max_in);
    real_max.setMax(local_min_in);
    local_min = real_min;
    local_max = real_max;

    m_shapeType = TRIANGLE_MESH_SHAPE_PROXYTYPE;
    setMargin(btScalar(0.0));
}

void VoxelCollisionShape::setLocalScaling(const btVector3& scaling) {
    // Отрицательное/нулевое масштабирование для воксельной сетки не имеет
    // смысла: по каждой оси используем положительный масштаб.
    local_scaling = safe_scale(scaling);
}

const btVector3& VoxelCollisionShape::getLocalScaling() const {
    return local_scaling;
}

void VoxelCollisionShape::calculateLocalInertia(btScalar mass, btVector3& inertia) const {
    (void)mass;
    // Concave shape всегда статическая.
    inertia.setValue(btScalar(0), btScalar(0), btScalar(0));
}

void VoxelCollisionShape::getAabb(const btTransform& t,
                                  btVector3& aabbMin,
                                  btVector3& aabbMax) const {
    btVector3 scaled_min = local_min * local_scaling;
    btVector3 scaled_max = local_max * local_scaling;

    btVector3 real_min = scaled_min;
    btVector3 real_max = scaled_max;
    real_min.setMin(scaled_max);
    real_max.setMax(scaled_min);

    transform_aabb(t, real_min, real_max, aabbMin, aabbMax);

    btScalar margin = getMargin();
    if (margin > btScalar(0)) {
        btVector3 expansion(margin, margin, margin);
        aabbMin -= expansion;
        aabbMax += expansion;
    }
}

Vec3i VoxelCollisionShape::getChunkWorldOrigin() const {
    return Vec3i(static_cast<int32_t>(std::lround(world_origin.x())),
                 static_cast<int32_t>(std::lround(world_origin.y())),
                 static_cast<int32_t>(std::lround(world_origin.z())));
}

void VoxelCollisionShape::processAllTriangles(btTriangleCallback* callback,
                                              const btVector3& aabbMin,
                                              const btVector3& aabbMax) const {
    if (!callback || !world) {
        return;
    }

    if (aabbMin.x() > aabbMax.x() ||
        aabbMin.y() > aabbMax.y() ||
        aabbMin.z() > aabbMax.z()) {
        return;
    }

    // AABB приходит в локальных (масштабированных) координатах shape.
    // Переводим его в немасштабированные координаты блоков.
    btVector3 query_min(
        aabbMin.x() / local_scaling.x(),
        aabbMin.y() / local_scaling.y(),
        aabbMin.z() / local_scaling.z());
    btVector3 query_max(
        aabbMax.x() / local_scaling.x(),
        aabbMax.y() / local_scaling.y(),
        aabbMax.z() / local_scaling.z());

    // Ограничиваем обход собственной областью shape.
    query_min.setMax(local_min);
    query_max.setMin(local_max);
    if (query_min.x() > query_max.x() ||
        query_min.y() > query_max.y() ||
        query_min.z() > query_max.z()) {
        return;
    }

    // Локальные координаты объекта -> мировые координаты блоков.
    btVector3 world_min = query_min + world_origin;
    btVector3 world_max = query_max + world_origin;

    int x0 = floor_to_int(world_min.x());
    int x1 = ceil_to_block(world_max.x());
    int y0 = std::max(0, floor_to_int(world_min.y()));
    int y1 = std::min(static_cast<int>(Chunk::HEIGHT) - 1,
                      ceil_to_block(world_max.y()));
    int z0 = floor_to_int(world_min.z());
    int z1 = ceil_to_block(world_max.z());

    if (x0 > x1 || y0 > y1 || z0 > z1) {
        return;
    }

    const int chunk_size = static_cast<int>(Chunk::SIZE_XZ);

    int chunk_x0 = floor_div(x0, chunk_size);
    int chunk_x1 = floor_div(x1, chunk_size);
    int chunk_z0 = floor_div(z0, chunk_size);
    int chunk_z1 = floor_div(z1, chunk_size);

    std::unordered_map<int64_t, int> loaded_chunks;
    for (const auto& pos : world->chunk_positions()) {
        loaded_chunks.insert({chunk_key(pos.x, pos.y), 1});
    }

    const Block& barrier = *world->get_block_mgr().get_barrier_block();
    const bool has_barrier = barrier.has_collision();

    for (int chunk_z = chunk_z0; chunk_z <= chunk_z1; ++chunk_z) {
        for (int chunk_x = chunk_x0; chunk_x <= chunk_x1; ++chunk_x) {
            int base_x = chunk_x * chunk_size;
            int base_z = chunk_z * chunk_size;

            int local_x0 = std::max(x0 - base_x, 0);
            int local_x1 = std::min(x1 - base_x, chunk_size - 1);
            int local_z0 = std::max(z0 - base_z, 0);
            int local_z1 = std::min(z1 - base_z, chunk_size - 1);

            const bool chunk_loaded =
                loaded_chunks.count(chunk_key(chunk_x, chunk_z)) != 0;
            const Chunk* chunk = chunk_loaded
                ? world->get_chunk_ptr(Vec2i(chunk_x, chunk_z))
                : nullptr;

            // Незагруженный чанк — сплошная стена барьеров по всей его
            // высоте: физика должна останавливать сущности на краю
            // загруженной части мира (см. World::get_block).
            if (!chunk_loaded) {
                if (!has_barrier) {
                    continue;
                }
                for (int y = y0; y <= y1; ++y) {
                    for (int local_z = local_z0; local_z <= local_z1; ++local_z) {
                        for (int local_x = local_x0; local_x <= local_x1; ++local_x) {
                            Vec3i world_pos(base_x + local_x, y, base_z + local_z);
                            int triangle_index = 0;
                            emitBlockTriangles(barrier, world_pos, callback,
                                               query_min, query_max,
                                               aabbMin, aabbMax,
                                               triangle_index);
                        }
                    }
                }
                continue;
            }

            for (int y = y0; y <= y1; ++y) {
                for (int local_z = local_z0; local_z <= local_z1; ++local_z) {
                    for (int local_x = local_x0; local_x <= local_x1; ++local_x) {
                        uint16_t block_id = chunk->get_block_id(
                            static_cast<uint8_t>(local_x),
                            static_cast<uint16_t>(y),
                            static_cast<uint8_t>(local_z));

                        std::shared_ptr<Block> block;
                        if (block_id == 0) {
                            // Воздух внутри загруженного чанка может быть
                            // «виртуальным барьером» (например, над
                            // столбцом воздуха у границы мира) — сверяемся
                            // с World::get_block, чтобы логика и физика
                            // видели одинаковые блоки.
                            block = world->get_block(Vec3i(base_x + local_x, y,
                                                           base_z + local_z));
                        } else {
                            block = world->get_block_mgr().get_block(block_id);
                        }
                        if (!block || !block->has_collision()) {
                            continue;
                        }

                        Vec3i world_pos(base_x + local_x, y, base_z + local_z);
                        int triangle_index = 0;
                        emitBlockTriangles(*block, world_pos, callback,
                                           query_min, query_max,
                                           aabbMin, aabbMax,
                                           triangle_index);
                    }
                }
            }
        }
    }
}

void VoxelCollisionShape::emitBlockTriangles(const Block& block,
                                             const Vec3i& world_block_pos,
                                             btTriangleCallback* callback,
                                             const btVector3& query_min,
                                             const btVector3& query_max,
                                             const btVector3& output_min,
                                             const btVector3& output_max,
                                             int& triangle_index) const {
    btVector3 block_min(
        btScalar(world_block_pos.x) - world_origin.x(),
        btScalar(world_block_pos.y) - world_origin.y(),
        btScalar(world_block_pos.z) - world_origin.z());
    btVector3 block_max = block_min + btVector3(btScalar(1), btScalar(1), btScalar(1));

    // Дополнительная проверка пересечения с учётом мировой позиции блока.
    if (!aabb_overlap(block_min, block_max, query_min, query_max)) {
        return;
    }

    btTransform block_transform;
    block_transform.setIdentity();
    block_transform.setOrigin(block_min + btVector3(btScalar(0.5), btScalar(0.5), btScalar(0.5)));

    emitShapeTriangles(*block.collision, block_transform, callback,
                       query_min, query_max,
                       output_min, output_max,
                       static_cast<int>(block.id), triangle_index);
}

void VoxelCollisionShape::emitShapeTriangles(const btCollisionShape& shape,
                                             const btTransform& local_to_object,
                                             btTriangleCallback* callback,
                                             const btVector3& query_min,
                                             const btVector3& query_max,
                                             const btVector3& output_min,
                                             const btVector3& output_max,
                                             int part_id,
                                             int& triangle_index) const {
    const int shape_type = shape.getShapeType();

    // Стандартный куб (все обычные блоки) — самый частый путь.
    if (shape_type == BOX_SHAPE_PROXYTYPE) {
        const btBoxShape& box = static_cast<const btBoxShape&>(shape);
        btVector3 h = box.getHalfExtentsWithMargin();

        btVector3 v[8] = {
            btVector3(-h.x(), -h.y(), -h.z()),
            btVector3( h.x(), -h.y(), -h.z()),
            btVector3( h.x(), -h.y(),  h.z()),
            btVector3(-h.x(), -h.y(),  h.z()),
            btVector3(-h.x(),  h.y(), -h.z()),
            btVector3( h.x(),  h.y(), -h.z()),
            btVector3( h.x(),  h.y(),  h.z()),
            btVector3(-h.x(),  h.y(),  h.z())
        };

        for (int i = 0; i < 8; ++i) {
            v[i] = local_to_object * v[i];
        }

        static const int indices[12][3] = {
            {0, 1, 2}, {0, 2, 3}, // -Y
            {4, 6, 5}, {4, 7, 6}, // +Y
            {0, 4, 5}, {0, 5, 1}, // -Z
            {3, 2, 6}, {3, 6, 7}, // +Z
            {0, 3, 7}, {0, 7, 4}, // -X
            {1, 5, 6}, {1, 6, 2}  // +X
        };

        for (int i = 0; i < 12; ++i) {
            emitTriangle(callback,
                         v[indices[i][0]], v[indices[i][1]], v[indices[i][2]],
                         output_min, output_max, part_id, triangle_index);
        }
        return;
    }

    if (shape_type == COMPOUND_SHAPE_PROXYTYPE) {
        const btCompoundShape& compound = static_cast<const btCompoundShape&>(shape);
        for (int i = 0; i < compound.getNumChildShapes(); ++i) {
            emitShapeTriangles(*compound.getChildShape(i),
                               local_to_object * compound.getChildTransform(i),
                               callback, query_min, query_max,
                               output_min, output_max,
                               part_id, triangle_index);
        }
        return;
    }

    if (shape.isConcave()) {
        const btConcaveShape& concave = static_cast<const btConcaveShape&>(shape);

        // processAllTriangles у вложенной concave shape ожидает AABB в её
        // собственных координатах.
        btTransform object_to_shape = local_to_object.inverse();
        btVector3 child_min;
        btVector3 child_max;
        transform_aabb(object_to_shape, query_min, query_max, child_min, child_max);

        struct TriangleCollector : public btTriangleCallback {
            btTransform transform;
            std::vector<btVector3> vertices;

            void processTriangle(btVector3* triangle, int, int) override {
                vertices.push_back(transform * triangle[0]);
                vertices.push_back(transform * triangle[1]);
                vertices.push_back(transform * triangle[2]);
            }
        } collector;

        collector.transform = local_to_object;
        concave.processAllTriangles(&collector, child_min, child_max);

        for (size_t i = 0; i + 2 < collector.vertices.size(); i += 3) {
            emitTriangle(callback,
                         collector.vertices[i],
                         collector.vertices[i + 1],
                         collector.vertices[i + 2],
                         output_min, output_max, part_id, triangle_index);
        }
        return;
    }

    if (shape.isConvex()) {
        const btConvexShape& convex = static_cast<const btConvexShape&>(shape);
        btShapeHull hull(&convex);
        if (hull.buildHull(convex.getMargin())) {
            const btVector3* vertices = hull.getVertexPointer();
            const unsigned int* indices = hull.getIndexPointer();
            int count = hull.numTriangles();

            for (int i = 0; i < count; ++i) {
                btVector3 a = local_to_object * vertices[indices[i * 3 + 0]];
                btVector3 b = local_to_object * vertices[indices[i * 3 + 1]];
                btVector3 c = local_to_object * vertices[indices[i * 3 + 2]];
                emitTriangle(callback, a, b, c,
                             output_min, output_max, part_id, triangle_index);
            }
        }
        return;
    }

    // Неизвестный тип shape — консервативно используем его AABB.
    btVector3 box_min;
    btVector3 box_max;
    shape.getAabb(local_to_object, box_min, box_max);
    btVector3 v[8] = {
        btVector3(box_min.x(), box_min.y(), box_min.z()),
        btVector3(box_max.x(), box_min.y(), box_min.z()),
        btVector3(box_max.x(), box_max.y(), box_min.z()),
        btVector3(box_min.x(), box_max.y(), box_min.z()),
        btVector3(box_min.x(), box_min.y(), box_max.z()),
        btVector3(box_max.x(), box_min.y(), box_max.z()),
        btVector3(box_max.x(), box_max.y(), box_max.z()),
        btVector3(box_min.x(), box_max.y(), box_max.z())
    };
    static const int indices[12][3] = {
        {0, 1, 2}, {0, 2, 3},
        {4, 6, 5}, {4, 7, 6},
        {0, 4, 5}, {0, 5, 1},
        {3, 2, 6}, {3, 6, 7},
        {0, 3, 7}, {0, 7, 4},
        {1, 5, 6}, {1, 6, 2}
    };
    for (int i = 0; i < 12; ++i) {
        emitTriangle(callback,
                     v[indices[i][0]], v[indices[i][1]], v[indices[i][2]],
                     output_min, output_max, part_id, triangle_index);
    }
}

void VoxelCollisionShape::emitTriangle(btTriangleCallback* callback,
                                       const btVector3& a,
                                       const btVector3& b,
                                       const btVector3& c,
                                       const btVector3& output_min,
                                       const btVector3& output_max,
                                       int part_id,
                                       int& triangle_index) const {
    btVector3 scaled_a = a * local_scaling;
    btVector3 scaled_b = b * local_scaling;
    btVector3 scaled_c = c * local_scaling;

    if (!triangle_overlaps_aabb(scaled_a, scaled_b, scaled_c, output_min, output_max)) {
        return;
    }

    btVector3 triangle[3] = {scaled_a, scaled_b, scaled_c};
    callback->processTriangle(triangle, part_id, triangle_index++);
}
