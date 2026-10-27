#include "dummy.h"

#include <algorithm>

#include "world.h"


Dummy::Dummy() {
    body.set_half_extents(Vec3f(BODY_RADIUS, BODY_HEIGHT * 0.5f, BODY_RADIUS));
    body.position = transform.position;
    body.mass = 70.0f;
    body.reset_history();
    sync_view_from_transform();
}

Dummy::Dummy(const Vec3f& position, const Vec3r& rotation) {
    transform.position = position;
    // rotation.x — yaw корпуса, rotation.y — начальный pitch взгляда.
    transform.rotation = Vec3r(rotation.x, 0.0f, 0.0f);
    view.rotation = Vec3r(rotation.x, rotation.y, 0.0f);

    body.set_half_extents(Vec3f(BODY_RADIUS, BODY_HEIGHT * 0.5f, BODY_RADIUS));
    body.position = position;
    body.mass = 70.0f;
    body.reset_history();

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

    body.set_position(transform.position);
}

void Dummy::sync_body_from_transform() {
    body.set_position(transform.position);
    body.velocity = Vec3f::zero();
}

void Dummy::sync_transform_from_body() {
    transform.position = body.position;
    transform.rotation.y = 0.0f;
    transform.rotation.z = 0.0f;
    sync_view_from_transform();
}

void Dummy::sync_view_from_body(float alpha) {
    transform.position = body.position;
    transform.rotation.y = 0.0f;
    transform.rotation.z = 0.0f;

    // Камера Dummy отстаёт только по Y, чтобы ступеньки не дёргали обзор.
    // X/Z и yaw остаются текущими, иначе управление ощущалось бы с задержкой.
    Vec3f render_position = body.get_render_position(alpha);
    view.position = Vec3f(
        body.position.x,
        render_position.y + EYE_OFFSET,
        body.position.z);
    view.rotation.x = transform.rotation.x;
    set_pitch(view.rotation.y);
}

void Dummy::update(uint32_t tick) {
    (void)tick;
    sync_view_from_transform();
}


DummyPickupTrigger::DummyPickupTrigger() = default;

DummyPickupTrigger::DummyPickupTrigger(Dummy& dummy,
                                       World& world,
                                       Inventory& inventory,
                                       uint32_t preferred_slot,
                                       float radius)
    : dummy(&dummy),
      world(&world),
      inventory(&inventory),
      preferred_slot(preferred_slot) {
    half_extents = Vec3f(radius, radius, radius);
    sync_bounds();
}

void DummyPickupTrigger::sync_bounds() {
    if (dummy) {
        position = dummy->transform.position;
    }
}

void DummyPickupTrigger::on_enter(CollisionObject& other) {
    if (!world || !inventory) {
        return;
    }
    if (dynamic_cast<FragmentCollision*>(&other) == nullptr) {
        return;
    }

    // Фрагмент не обязан быть item-entity, поэтому используем безопасный
    // поиск ближайшего предмета в окрестности. Это демонстрация того, как
    // триггер может использовать интерфейс Dummy/World, не становясь
    // физическим препятствием.
    const float radius = std::max(half_extents.x,
                                  std::max(half_extents.y, half_extents.z));
    world->pickup_nearest_item(other.get_bounds().center(), radius,
                               *inventory, preferred_slot);
}
