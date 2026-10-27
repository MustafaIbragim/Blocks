#pragma once

#include <cstdint>
#include "vec.h"


class World;


// Результат трассировки луча по воксельной сетке.
struct RaycastHit {
    bool hit = false;
    Vec3i block;             // блок, в который попал луч
    Vec3i normal;            // нормаль грани (для установки блока)
    uint16_t nbt = 0;        // NBT блока в точке попадания
    float distance = 0.0f;   // расстояние до входа в блок

    explicit operator bool() const { return hit; }
};


// Трассировка луча по вокселям (Amanatides & Woo).
// origin задаётся в мировых координатах, direction не обязан быть
// нормализованным. max_distance ограничивает длину луча.
RaycastHit raycast_voxel(World& world,
                         const Vec3f& origin,
                         const Vec3f& direction,
                         float max_distance = 8.0f);


enum class Axis : uint8_t {
    NONE = 0,
    X = 1,
    Y = 2,
    Z = 3,
};


// Класс для получения единичных векторов из углов поворота.
//
// Соглашение по поворотам совпадает с Renderer/ViewCam:
//   rotation.x = yaw   (поворот вокруг Y)
//   rotation.y = pitch (наклон вверх/вниз)
//   rotation.z = roll  (для направлений не используется)
class ViewDirections {
public:
    explicit ViewDirections(const Vec3r& rotation);

    void set_rotation(const Vec3r& rotation) { rotation_ = rotation; }
    const Vec3r& get_rotation() const { return rotation_; }

    // Полные единичные векторы взгляда.
    Vec3f create_forward(Axis ignore_axis = Axis::NONE) const;
    Vec3f create_back(Axis ignore_axis = Axis::NONE) const;
    Vec3f create_right(Axis ignore_axis = Axis::NONE) const;
    Vec3f create_left(Axis ignore_axis = Axis::NONE) const;
    Vec3f create_up(Axis ignore_axis = Axis::NONE) const;
    Vec3f create_down(Axis ignore_axis = Axis::NONE) const;

    // Векторы, лежащие в горизонтальной плоскости (игнорируется Y).
    // Удобны для движения персонажа.
    Vec3f create_forward_flat() const;
    Vec3f create_back_flat() const;
    Vec3f create_right_flat() const;
    Vec3f create_left_flat() const;

    // Частые варианты игнорирования конкретной оси.
    Vec3f create_forward_ignore_x() const { return create_forward(Axis::X); }
    Vec3f create_forward_ignore_y() const { return create_forward(Axis::Y); }
    Vec3f create_forward_ignore_z() const { return create_forward(Axis::Z); }
    Vec3f create_right_ignore_x() const { return create_right(Axis::X); }
    Vec3f create_right_ignore_y() const { return create_right(Axis::Y); }
    Vec3f create_right_ignore_z() const { return create_right(Axis::Z); }
    Vec3f create_up_ignore_x() const { return create_up(Axis::X); }
    Vec3f create_up_ignore_y() const { return create_up(Axis::Y); }
    Vec3f create_up_ignore_z() const { return create_up(Axis::Z); }

private:
    Vec3r rotation_;

    Vec3f raw_forward() const;
    Vec3f raw_right() const;
    Vec3f raw_up() const;
    static Vec3f ignore_axis(const Vec3f& value, Axis axis);
};
