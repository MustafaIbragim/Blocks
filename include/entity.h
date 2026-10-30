#pragma once

#include <cstdint>
#include "vec.h"


class Entity {
public:
    Transform transform;

    // Универсальная база и для статических, и для динамических сущностей.
    // Статическая сущность может выставить dynamic=false и не участвовать
    // в физическом/игровом обновлении.
    bool dynamic = true;

    // Скорость движения (мировых единиц в секунду) и коэффициент
    // квадратичного сопротивления: сила/замедление ~ drag * v * |v|.
    // Конкретные подсистемы (движение, физика, предметы) используют эти
    // поля по своему усмотрению.
    float speed = 0.0f;
    float drag = 0.0f;

    // Значение гравитации, переданное в последний update(). Сама сущность
    // решает, как его применить (например, через своё физическое тело).
    float gravity = 0.0f;

    Entity() : transform(Transform::identity()) {}
    Entity(const Transform& transform) : transform(transform) {}

    virtual void update(uint32_t tick, float gravity = 0.0f) {
        (void)tick;
        this->gravity = gravity;
    }

    virtual ~Entity() = default;
};


