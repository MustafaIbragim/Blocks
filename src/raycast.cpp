#include "raycast.h"

#include <cmath>
#include <limits>

#include "world.h"


namespace {

float safe_floor(float value) {
    return std::floor(value);
}

} // namespace


RaycastHit raycast_voxel(World& world,
                         const Vec3f& origin,
                         const Vec3f& direction,
                         float max_distance) {
    RaycastHit result;

    Vec3f dir = direction.normalized();
    if (dir.length() <= 1e-8f || max_distance < 0.0f) {
        return result;
    }

    int32_t x = static_cast<int32_t>(safe_floor(origin.x));
    int32_t y = static_cast<int32_t>(safe_floor(origin.y));
    int32_t z = static_cast<int32_t>(safe_floor(origin.z));

    const int32_t step_x = dir.x > 0.0f ? 1 : (dir.x < 0.0f ? -1 : 0);
    const int32_t step_y = dir.y > 0.0f ? 1 : (dir.y < 0.0f ? -1 : 0);
    const int32_t step_z = dir.z > 0.0f ? 1 : (dir.z < 0.0f ? -1 : 0);

    constexpr float inf = std::numeric_limits<float>::infinity();
    const float t_delta_x = step_x != 0 ? std::abs(1.0f / dir.x) : inf;
    const float t_delta_y = step_y != 0 ? std::abs(1.0f / dir.y) : inf;
    const float t_delta_z = step_z != 0 ? std::abs(1.0f / dir.z) : inf;

    float t_max_x = step_x != 0
        ? ((step_x > 0 ? (x + 1 - origin.x) : (origin.x - x)) * t_delta_x)
        : inf;
    float t_max_y = step_y != 0
        ? ((step_y > 0 ? (y + 1 - origin.y) : (origin.y - y)) * t_delta_y)
        : inf;
    float t_max_z = step_z != 0
        ? ((step_z > 0 ? (z + 1 - origin.z) : (origin.z - z)) * t_delta_z)
        : inf;

    Vec3i normal(0, 0, 0);
    float distance = 0.0f;

    while (distance <= max_distance) {
        std::shared_ptr<Block> block = world.get_block(Vec3i(x, y, z));
        if (block && block->id != 0) {
            result.hit = true;
            result.block = Vec3i(x, y, z);
            result.normal = normal;
            result.nbt = world.get_nbt(Vec3i(x, y, z));
            result.distance = distance;
            return result;
        }

        if (t_max_x < t_max_y) {
            if (t_max_x < t_max_z) {
                if (t_max_x > max_distance) break;
                x += step_x;
                distance = t_max_x;
                t_max_x += t_delta_x;
                normal = Vec3i(-step_x, 0, 0);
            } else {
                if (t_max_z > max_distance) break;
                z += step_z;
                distance = t_max_z;
                t_max_z += t_delta_z;
                normal = Vec3i(0, 0, -step_z);
            }
        } else {
            if (t_max_y < t_max_z) {
                if (t_max_y > max_distance) break;
                y += step_y;
                distance = t_max_y;
                t_max_y += t_delta_y;
                normal = Vec3i(0, -step_y, 0);
            } else {
                if (t_max_z > max_distance) break;
                z += step_z;
                distance = t_max_z;
                t_max_z += t_delta_z;
                normal = Vec3i(0, 0, -step_z);
            }
        }
    }

    return result;
}


ViewDirections::ViewDirections(const Vec3r& rotation)
    : rotation_(rotation) {}

Vec3f ViewDirections::raw_forward() const {
    const float yaw = rotation_.x;
    const float pitch = rotation_.y;
    const float cp = std::cos(pitch);
    return Vec3f(std::cos(yaw) * cp,
                 std::sin(pitch),
                 std::sin(yaw) * cp).normalized();
}

Vec3f ViewDirections::raw_right() const {
    const float yaw = rotation_.x;
    return Vec3f(-std::sin(yaw), 0.0f, std::cos(yaw)).normalized();
}

Vec3f ViewDirections::raw_up() const {
    return raw_right().cross(raw_forward()).normalized();
}

Vec3f ViewDirections::ignore_axis(const Vec3f& value, Axis axis) {
    Vec3f result = value;
    switch (axis) {
        case Axis::X: result.x = 0.0f; break;
        case Axis::Y: result.y = 0.0f; break;
        case Axis::Z: result.z = 0.0f; break;
        case Axis::NONE: break;
    }
    if (result.length() <= 1e-8f) {
        return Vec3f::zero();
    }
    return result.normalized();
}

Vec3f ViewDirections::create_forward(Axis axis) const {
    return axis == Axis::NONE ? raw_forward() : ignore_axis(raw_forward(), axis);
}

Vec3f ViewDirections::create_back(Axis axis) const {
    const Vec3f value = raw_forward() * -1.0f;
    return axis == Axis::NONE ? value : ignore_axis(value, axis);
}

Vec3f ViewDirections::create_right(Axis axis) const {
    return axis == Axis::NONE ? raw_right() : ignore_axis(raw_right(), axis);
}

Vec3f ViewDirections::create_left(Axis axis) const {
    const Vec3f value = raw_right() * -1.0f;
    return axis == Axis::NONE ? value : ignore_axis(value, axis);
}

Vec3f ViewDirections::create_up(Axis axis) const {
    return axis == Axis::NONE ? raw_up() : ignore_axis(raw_up(), axis);
}

Vec3f ViewDirections::create_down(Axis axis) const {
    const Vec3f value = raw_up() * -1.0f;
    return axis == Axis::NONE ? value : ignore_axis(value, axis);
}

Vec3f ViewDirections::create_forward_flat() const {
    const float yaw = rotation_.x;
    return Vec3f(std::cos(yaw), 0.0f, std::sin(yaw)).normalized();
}

Vec3f ViewDirections::create_back_flat() const {
    return create_forward_flat() * -1.0f;
}

Vec3f ViewDirections::create_right_flat() const {
    return raw_right();
}

Vec3f ViewDirections::create_left_flat() const {
    return raw_right() * -1.0f;
}
