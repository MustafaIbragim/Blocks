#include "dummy.h"

#include <algorithm>


Dummy::Dummy() {
    sync_view_from_transform();
}

Dummy::Dummy(const Vec3f& position, const Vec3r& rotation) {
    transform.position = position;
    // rotation.x — yaw корпуса, rotation.y — начальный pitch взгляда.
    transform.rotation = Vec3r(rotation.x, 0.0f, 0.0f);
    view.rotation = Vec3r(rotation.x, rotation.y, 0.0f);
    sync_view_from_transform();
}

void Dummy::set_yaw(float yaw) {
    transform.rotation = Vec3r(yaw, 0.0f, 0.0f);
    view.rotation.x = transform.rotation.x;
    view.rotation.z = 0.0f;
}

void Dummy::set_pitch(float pitch) {
    view.rotation.y = std::clamp(pitch, -MAX_PITCH, MAX_PITCH);
    view.rotation.z = 0.0f;
}

void Dummy::sync_view_from_transform() {
    // Корпус существует только вертикально: гасим любые pitch/roll.
    transform.rotation.y = 0.0f;
    transform.rotation.z = 0.0f;

    view.position = transform.position + Vec3f(0.0f, EYE_OFFSET, 0.0f);
    view.rotation.x = transform.rotation.x;
    set_pitch(view.rotation.y);
}

void Dummy::sync_transform_from_view() {
    transform.position = view.position - Vec3f(0.0f, EYE_OFFSET, 0.0f);
    transform.rotation.x = view.rotation.x;
    transform.rotation.y = 0.0f;
    transform.rotation.z = 0.0f;
    set_pitch(view.rotation.y);
}

void Dummy::update(uint32_t tick) {
    (void)tick;
    sync_view_from_transform();
}
