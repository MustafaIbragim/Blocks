#include "mesh.h"


uint32_t Mesh::add_vertex(float x, float y, float z) {
    Vertex vertex;
    vertex.x = x; vertex.y = y; vertex.z = z;
    vertex.block_light = 255;
    vertex.sky_light = 0;
    vertex.padding = 0;

    vertices.push_back(vertex);
    return static_cast<uint32_t>(vertices.size() - 1);
}

uint32_t Mesh::add_vertex(const Vec3f& pos, uint8_t block_light, uint8_t sky_light) {
    Vertex vertex;
    vertex.x = pos.x; vertex.y = pos.y; vertex.z = pos.z;
    vertex.block_light = block_light;
    vertex.sky_light = sky_light;
    vertex.padding = 0;

    vertices.push_back(vertex);
    return static_cast<uint32_t>(vertices.size() - 1);
}

uint32_t Mesh::add_vertex(const Vertex& vertex) {
    vertices.push_back(vertex);
    return static_cast<uint32_t>(vertices.size() - 1);
}

// Совместимость со старым API: uv/layer/anim больше не принадлежат вершине.
uint32_t Mesh::add_vertex(float x, float y, float z, float /*u*/, float /*v*/,
                          int32_t /*layer*/, uint8_t /*anim_data*/,
                          uint8_t block_light, uint8_t sky_light) {
    Vertex vertex;
    vertex.x = x; vertex.y = y; vertex.z = z;
    vertex.block_light = block_light;
    vertex.sky_light = sky_light;
    vertex.padding = 0;

    vertices.push_back(vertex);
    return static_cast<uint32_t>(vertices.size() - 1);
}

uint32_t Mesh::add_vertex(const Vec3f& pos, const Vec2f& /*uv*/, int32_t /*layer*/,
                          uint8_t /*anim_data*/, uint8_t block_light, uint8_t sky_light) {
    return add_vertex(pos.x, pos.y, pos.z, 0.0f, 0.0f, 0, 0, block_light, sky_light);
}

void Mesh::add_triangle(uint32_t i0, uint32_t i1, uint32_t i2, const FaceData& face) {
    indices.push_back(i0);
    indices.push_back(i1);
    indices.push_back(i2);
    faces.push_back(face);
}

void Mesh::add_triangle(const Vec3i& indices_vec, const FaceData& face) {
    add_triangle(static_cast<uint32_t>(indices_vec.x),
                 static_cast<uint32_t>(indices_vec.y),
                 static_cast<uint32_t>(indices_vec.z), face);
}

void Mesh::add_triangle(uint32_t i0, uint32_t i1, uint32_t i2) {
    FaceData face{};
    face.u_0 = 0.0f; face.v_0 = 0.0f;
    face.u_1 = 1.0f; face.v_1 = 0.0f;
    face.u_2 = 1.0f; face.v_2 = 1.0f;
    face.layer = 0;
    face.anim_data = 0;
    add_triangle(i0, i1, i2, face);
}

void Mesh::add_triangle(const Vec3i& indices_vec) {
    add_triangle(static_cast<uint32_t>(indices_vec.x),
                 static_cast<uint32_t>(indices_vec.y),
                 static_cast<uint32_t>(indices_vec.z));
}

void Mesh::clear() {
    vertices.clear();
    indices.clear();
    faces.clear();
}
