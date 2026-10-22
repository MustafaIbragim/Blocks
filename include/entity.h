#pragma once

#include <cstdint>
#include "vec.h"


class Entity {
public:
    Transform transform;
    
    Entity() : transform(Transform::identity()) {}
    Entity(const Transform& transform) : transform(transform) {}
    
    virtual void update(uint32_t tick) {}
    
    virtual ~Entity() = default;
};


