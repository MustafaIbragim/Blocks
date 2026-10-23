#pragma once

#include <cstdint>

#include "entity.h"
#include "view_cam.h"


// Простая сущность-«болванка» для игрока/камеры.
// Владеет ViewCam и синхронизирует её со своим Transform.
class Dummy : public Entity {
public:
    ViewCam view;

    // Состояние, которое удобно переключать из теста/игры.
    bool collisions_enabled = true;
    bool gravity_enabled = true;

    Dummy();
    Dummy(const Vec3f& position, const Vec3r& rotation);

    void set_view(const ViewCam& value) { view = value; }
    ViewCam& get_view() { return view; }
    const ViewCam& get_view() const { return view; }

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
