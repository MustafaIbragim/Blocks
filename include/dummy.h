#pragma once

#include <cstdint>

#include "entity.h"
#include "view_cam.h"


// Простая сущность-«болванка» для игрока/камеры.
// Корпус всегда стоит вертикально и вращается только вокруг Y (yaw),
// а ViewCam дополнительно хранит pitch для обзора от первого лица.
class Dummy : public Entity {
public:
    // Размер блока в мировых единицах. Манекен задан в блоках:
    // 1.2 x 3.6 x 1.2 блока => 0.6 x 1.8 x 0.6 мировых единицы.
    static constexpr float BLOCK_SIZE = 0.5f;
    static constexpr float BODY_RADIUS = 0.6f * BLOCK_SIZE;   // 0.3
    static constexpr float BODY_HEIGHT = 3.6f * BLOCK_SIZE;   // 1.8 (полная высота)
    // Bullet задаёт высоту только цилиндрической части капсулы.
    static constexpr float CAPSULE_HEIGHT = BODY_HEIGHT - 2.0f * BODY_RADIUS;
    // Глаза чуть ниже верхушки капсулы.
    static constexpr float EYE_OFFSET = BODY_HEIGHT * 0.5f - 0.15f;
    static constexpr float MAX_PITCH = 1.5f;

    ViewCam view;

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

    void set_collisions_enabled(bool enabled) { collisions_enabled = enabled; }
    void set_gravity_enabled(bool enabled) { gravity_enabled = enabled; }
    void toggle_physics() {
        collisions_enabled = !collisions_enabled;
        gravity_enabled = !gravity_enabled;
    }

    void update(uint32_t tick) override;
};
