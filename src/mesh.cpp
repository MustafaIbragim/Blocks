#include "mesh.h"


uint32_t Mesh::add_vertex(float x, float y, float z, float u, float v, int32_t layer, 
                          uint8_t anim_data, uint8_t block_light, uint8_t sky_light) {
    Vertex vertex;
    vertex.x = x; vertex.y = y; vertex.z = z;
    vertex.u = u; vertex.v = v;
    vertex.layer = layer;
    vertex.anim_data = anim_data;
    vertex.block_light = block_light;
    vertex.sky_light = sky_light;
    vertex.padding = 0;
    
    vertices.push_back(vertex);
    return static_cast<uint32_t>(vertices.size() - 1);
}

uint32_t Mesh::add_vertex(const Vec3f& pos, const Vec2f& uv, int32_t layer, 
                          uint8_t anim_data, uint8_t block_light, uint8_t sky_light) {
    return add_vertex(pos.x, pos.y, pos.z, uv.x, uv.y, layer, anim_data, block_light, sky_light);
}

uint32_t Mesh::add_vertex(const Vertex& vertex) {
    vertices.push_back(vertex);
    return static_cast<uint32_t>(vertices.size() - 1);
}

void Mesh::add_triangle(uint32_t i0, uint32_t i1, uint32_t i2) {
    indices.push_back(i0);
    indices.push_back(i1);
    indices.push_back(i2);
}

void Mesh::add_triangle(const Vec3i& indices_vec) {
    indices.push_back(static_cast<uint32_t>(indices_vec.x));
    indices.push_back(static_cast<uint32_t>(indices_vec.y));
    indices.push_back(static_cast<uint32_t>(indices_vec.z));
}

void Mesh::clear() {
    vertices.clear();
    indices.clear();
}


