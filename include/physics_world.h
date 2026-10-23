#pragma once

#include <cstdint>
#include <memory>

#include "vec.h"


class World;
class btDiscreteDynamicsWorld;

// Небольшая обвязка над btDiscreteDynamicsWorld для воксельного мира.
// На каждый загруженный чанк создаётся один статический btCollisionObject
// с VoxelCollisionShape. Сами блоки не копируются: shape читает их из World.
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Добавляет/удаляет статический collision object чанка.
    void add_chunk(World& world, const Vec2i& chunk_pos);
    void remove_chunk(const Vec2i& chunk_pos);
    bool has_chunk(const Vec2i& chunk_pos) const;
    void clear_chunks();

    // Динамические тела. Возвращают стабильный id внутри PhysicsWorld.
    uint32_t add_dynamic_box(const Vec3f& half_extents,
                             float mass,
                             const Vec3f& position);
    uint32_t add_dynamic_sphere(float radius,
                                float mass,
                                const Vec3f& position);
    void remove_body(uint32_t body_id);
    bool has_body(uint32_t body_id) const;

    Vec3f get_body_position(uint32_t body_id) const;
    Vec4f get_body_rotation(uint32_t body_id) const;
    Vec3f get_body_linear_velocity(uint32_t body_id) const;
    Vec3f get_body_gravity(uint32_t body_id) const;
    void set_body_position(uint32_t body_id, const Vec3f& position);
    void set_body_linear_velocity(uint32_t body_id, const Vec3f& velocity);
    void set_body_gravity(uint32_t body_id, const Vec3f& gravity);

    // Разрешает/запрещает контактный отклик тела с остальным миром.
    void set_body_collisions_enabled(uint32_t body_id, bool enabled);
    bool get_body_collisions_enabled(uint32_t body_id) const;
    void activate_body(uint32_t body_id);

    void set_gravity(const Vec3f& gravity);
    void step(float delta_time,
              int max_substeps = 1,
              float fixed_time_step = 1.0f / 60.0f);

    int get_num_collision_objects() const;
    int get_num_manifolds() const;

    // Для C++-кода, которому нужен доступ к нативному Bullet-миру.
    btDiscreteDynamicsWorld* get_native_world() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
