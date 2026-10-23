#pragma once

#include <cstdint>
#include <memory>

#include <BulletCollision/CollisionShapes/btConcaveShape.h>
#include <LinearMath/btVector3.h>

#include "vec.h"


class World;
class Block;

// Неподвижная collision shape воксельного мира.
//
// Форма не хранит копию блоков: processAllTriangles каждый раз читает
// актуальные данные из World/Chunk. Поэтому изменение блоков не требует
// пересоздания btCollisionObject.
//
// Shape строится в локальных координатах объекта. Для чанка локальные
// координаты совпадают с локальными координатами Chunk, а сам
// btCollisionObject должен быть поставлен в мировую позицию чанка
// (см. PhysicsWorld::add_chunk).
class VoxelCollisionShape : public btConcaveShape {
public:
    // Форма для одного чанка: [0..32) x [0..HEIGHT) x [0..32).
    VoxelCollisionShape(World& world, const Vec2i& chunk_pos);

    // Произвольная область в локальных координатах объекта.
    VoxelCollisionShape(World& world,
                        const btVector3& local_min,
                        const btVector3& local_max,
                        const btVector3& world_origin = btVector3(0, 0, 0));

    ~VoxelCollisionShape() override = default;

    // Основной метод: обходит только блоки, попавшие в AABB, и отдаёт
    // треугольники их collision shapes в callback Bullet.
    void processAllTriangles(btTriangleCallback* callback,
                             const btVector3& aabbMin,
                             const btVector3& aabbMax) const override;

    void getAabb(const btTransform& t,
                 btVector3& aabbMin,
                 btVector3& aabbMax) const override;

    void setLocalScaling(const btVector3& scaling) override;
    const btVector3& getLocalScaling() const override;

    void calculateLocalInertia(btScalar mass, btVector3& inertia) const override;

    const char* getName() const override { return "VoxelCollisionShape"; }

    World& getWorld() const { return *world; }

    // Мировая позиция локального нуля shape.
    const btVector3& getWorldOrigin() const { return world_origin; }

    const btVector3& getLocalMin() const { return local_min; }
    const btVector3& getLocalMax() const { return local_max; }

    btVector3 getWorldMin() const { return local_min * local_scaling + world_origin; }
    btVector3 getWorldMax() const { return local_max * local_scaling + world_origin; }

    // Смещение блока в мировых координатах (используется для отладки/тестов).
    Vec3i getChunkWorldOrigin() const;

private:
    void emitBlockTriangles(const Block& block,
                            const Vec3i& world_block_pos,
                            btTriangleCallback* callback,
                            const btVector3& query_min,
                            const btVector3& query_max,
                            const btVector3& output_min,
                            const btVector3& output_max,
                            int& triangle_index) const;

    void emitShapeTriangles(const btCollisionShape& shape,
                            const btTransform& local_to_object,
                            btTriangleCallback* callback,
                            const btVector3& query_min,
                            const btVector3& query_max,
                            const btVector3& output_min,
                            const btVector3& output_max,
                            int part_id,
                            int& triangle_index) const;

    void emitTriangle(btTriangleCallback* callback,
                      const btVector3& a,
                      const btVector3& b,
                      const btVector3& c,
                      const btVector3& output_min,
                      const btVector3& output_max,
                      int part_id,
                      int& triangle_index) const;

    World* world;
    btVector3 local_min;
    btVector3 local_max;
    btVector3 world_origin;
    btVector3 local_scaling;
};
