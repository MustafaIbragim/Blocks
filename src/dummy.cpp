#include "dummy.h"

#include <algorithm>

#include "world.h"


Dummy::Dummy() {
    body.set_half_extents(Vec3f(BODY_RADIUS, BODY_HEIGHT * 0.5f, BODY_RADIUS));
    body.position = transform.position;
    body.mass = 70.0f;
    body.reset_history();

    dynamic = true;
    speed = 8.0f;
    drag = 0.0f;
    mover.set_half_extents(Vec3f(BODY_RADIUS, BODY_HEIGHT * 0.5f, BODY_RADIUS));
    mover.set_step_height(step_height);

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

    dynamic = true;
    speed = 8.0f;
    drag = 0.0f;
    mover.set_half_extents(Vec3f(BODY_RADIUS, BODY_HEIGHT * 0.5f, BODY_RADIUS));
    mover.set_step_height(step_height);

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

bool Dummy::move_to(World& world,
                    const Vec3i& from,
                    const Vec3i& to,
                    float duration,
                    std::optional<float> step_height) {
    const float height = step_height.value_or(this->step_height);
    this->step_height = height;
    mover.set_step_height(height);

    const bool ok = mover.move_to(world, body, from, to, duration, height,
                                  true);
    if (ok) {
        sync_transform_from_body();
    }
    return ok;
}

bool Dummy::move_to(World& world,
                    const Vec3i& to,
                    float duration,
                    std::optional<float> step_height) {
    const Vec3i from = PathMover::current_block(body);
    return move_to(world, from, to, duration, step_height);
}

bool Dummy::update_movement(World& world, float delta_time) {
    const bool moving = mover.update(world, body, delta_time);
    sync_transform_from_body();
    return moving;
}

void Dummy::move_towards(World& world,
                         const Vec3f& direction,
                         float speed,
                         float delta_time,
                         std::optional<float> step_height) {
    const float height = step_height.value_or(this->step_height);
    this->step_height = height;
    mover.set_step_height(height);
    // Ручное управление отменяет текущий путь, иначе update_movement()
    // продолжил бы перезаписывать скорость.
    mover.stop();
    this->speed = speed;

    Vec3f horizontal(direction.x, 0.0f, direction.z);
    const float length = horizontal.length();
    if (length > 1e-6f) {
        horizontal = horizontal / length;
    } else {
        horizontal = Vec3f::zero();
    }

    const Vec3f desired(horizontal.x * speed * delta_time,
                        0.0f,
                        horizontal.z * speed * delta_time);
    GroundMover::try_step_up(world, body, desired, height);

    body.velocity.x = horizontal.x * speed;
    body.velocity.z = horizontal.z * speed;
}

void Dummy::stop_movement() {
    mover.stop();
    body.velocity.x = 0.0f;
    body.velocity.z = 0.0f;
}

void Dummy::update(uint32_t tick, float gravity) {
    Entity::update(tick, gravity);
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
