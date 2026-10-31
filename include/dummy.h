#pragma once

#include <cstdint>

#include "collision.h"
#include "entity.h"
#include "movement.h"
#include "view_cam.h"


class World;
class Inventory;


// Простая сущность-«болванка» для игрока/камеры.
// Корпус всегда стоит вертикально и вращается только вокруг Y (yaw),
// а ViewCam дополнительно хранит pitch для обзора от первого лица.
class Dummy : public Entity {
public:
    // Один блок = 1.0 мировой единицы = 0.5 метра. Манекен задан в
    // мировых единицах: 1.2 x 3.6 x 1.2 => 0.6 x 1.8 x 0.6 метра.
    static constexpr float BLOCK_SIZE = 1.0f;
    static constexpr float BODY_RADIUS = 0.6f * BLOCK_SIZE;   // 0.6
    static constexpr float BODY_HEIGHT = 3.6f * BLOCK_SIZE;   // 3.6 (полная высота)
    // Высота цилиндрической части старой капсулы. Оставлена для
    // совместимости; новый движок использует AABB.
    static constexpr float CAPSULE_HEIGHT = BODY_HEIGHT - 2.0f * BODY_RADIUS;
    // Глаза чуть ниже верхушки капсулы.
    static constexpr float EYE_OFFSET = BODY_HEIGHT * 0.5f - 0.15f;
    static constexpr float MAX_PITCH = 1.5f;

    ViewCam view;

    // Физическое тело Dummy. Это обычный EntityCollision с AABB
    // (BODY_RADIUS * 2) x BODY_HEIGHT x (BODY_RADIUS * 2). Его можно
    // зарегистрировать в CollisionWorld::add_entity(body).
    EntityCollision body;

    // Перемещение по блокам: поиск пути, шаг на ступеньку и движение
    // вдоль пути. Mover только задаёт скорость/приподнимает тело; реальное
    // перемещение выполняет CollisionWorld.
    GroundMover mover;
    float step_height = 0.6f;

    // Состояние, которое удобно переключать из теста/игры.
    bool collisions_enabled = true;
    bool gravity_enabled = true;

    Dummy();
    Dummy(const Vec3f& position, const Vec3r& rotation);

    void set_view(const ViewCam& value) { view = value; }
    ViewCam& get_view() { return view; }
    const ViewCam& get_view() const { return view; }

    // Yaw корпуса. Pitch в transform не хранится — корпус не наклоняется.
    void set_yaw(float yaw);
    float get_yaw() const { return transform.rotation.x; }
    void rotate_yaw(float delta) { set_yaw(get_yaw() + delta); }

    // Pitch только для камеры/взгляда, корпус остаётся вертикальным.
    void set_pitch(float pitch);
    float get_pitch() const { return view.rotation.y; }
    void rotate_pitch(float delta) { set_pitch(get_pitch() + delta); }

    // Позиция глаз (view.position) относительно центра капсулы.
    Vec3f get_eye_position() const { return view.position; }

    void sync_view_from_transform();
    void sync_transform_from_view();

    // Синхронизация с физическим телом.
    void sync_body_from_transform();
    void sync_transform_from_body();
    // Для Dummy графика отстаёт только по Y камеры: X/Z и поворот берутся
    // из текущей физической позиции, а Y — из истории интерполяции.
    void sync_view_from_body(float alpha = 0.0f);

    void set_collisions_enabled(bool enabled) {
        collisions_enabled = enabled;
        body.collisions_enabled = enabled;
    }
    void set_gravity_enabled(bool enabled) {
        gravity_enabled = enabled;
        body.gravity_enabled = enabled;
    }
    void toggle_physics() {
        collisions_enabled = !collisions_enabled;
        gravity_enabled = !gravity_enabled;
        body.collisions_enabled = collisions_enabled;
        body.gravity_enabled = gravity_enabled;
    }

    // Перемещение из блока from в блок to. Скорость постоянная и задаётся
    // mover.speed; duration оставлен для совместимости и не влияет на неё.
    // Если путь непроходим, команда не выдаётся и возвращается false.
    bool move_to(World& world,
                 const Vec3i& from,
                 const Vec3i& to,
                 float duration,
                 std::optional<float> step_height = std::nullopt);
    bool move_to(World& world,
                 const Vec3i& to,
                 float duration,
                 std::optional<float> step_height = std::nullopt);

    // Продвинуть путь на delta_time. Возвращает true, пока движение идёт.
    bool update_movement(World& world, float delta_time);

    // Ручное движение в заданном направлении (для ввода игрока). Перед
    // выдачей горизонтальной скорости пробует забраться на ступеньку.
    void move_towards(World& world,
                      const Vec3f& direction,
                      float speed,
                      float delta_time,
                      std::optional<float> step_height = std::nullopt);

    void stop_movement();
    bool is_moving() const { return mover.is_moving(); }

    void update(uint32_t tick, float gravity = 0.0f) override;
};


// Пример триггера, который использует интерфейс Dummy и подбирает
// предметы, попадающие в его область. Это не физическое препятствие:
// TriggerCollision только сообщает о вхождении/выхождении.
class DummyPickupTrigger : public TriggerCollision {
public:
    Dummy* dummy = nullptr;
    World* world = nullptr;
    Inventory* inventory = nullptr;
    uint32_t preferred_slot = UINT32_MAX;

    DummyPickupTrigger();
    DummyPickupTrigger(Dummy& dummy, World& world, Inventory& inventory,
                       uint32_t preferred_slot = UINT32_MAX,
                       float radius = 4.0f);

    void sync_bounds();
    void on_enter(CollisionObject& other) override;
};
