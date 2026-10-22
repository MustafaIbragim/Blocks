#pragma once

#include <cstdint>
#include <vector>
#include "vec.h"


class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    uint32_t add_vertex(float x, float y, float z, float u, float v, int32_t layer, 
                        uint8_t anim_data, uint8_t block_light = 255, uint8_t sky_light = 0);
    
    uint32_t add_vertex(const Vec3f& pos, const Vec2f& uv, int32_t layer, 
                        uint8_t anim_data, uint8_t block_light = 255, uint8_t sky_light = 0);
    
    uint32_t add_vertex(const Vertex& vertex);
    
    void add_triangle(uint32_t i0, uint32_t i1, uint32_t i2);
    void add_triangle(const Vec3i& indices_vec);
    
    void clear();
    
    uint32_t vertex_count() const { return static_cast<uint32_t>(vertices.size()); }
    uint32_t index_count() const { return static_cast<uint32_t>(indices.size()); }
    uint32_t triangle_count() const { return static_cast<uint32_t>(indices.size() / 3); }
};


