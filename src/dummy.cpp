#include "dummy.h"


Dummy::Dummy() = default;

Dummy::Dummy(const Vec3f& position, const Vec3r& rotation) {
    transform.position = position;
    transform.rotation = rotation;
    sync_view_from_transform();
}

void Dummy::sync_view_from_transform() {
    view.position = transform.position;
    view.rotation = transform.rotation;
}

void Dummy::sync_transform_from_view() {
    transform.position = view.position;
    transform.rotation = view.rotation;
}

void Dummy::update(uint32_t tick) {
    (void)tick;
    sync_view_from_transform();
}
