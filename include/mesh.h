#pragma once

#include <cstdint>
#include <vector>
#include "vec.h"


// Mesh — CPU-представление геометрии.
//
// Вершины (Vertex) хранят только позицию и освещение и могут переиспользоваться
// несколькими гранями. Данные о текстуре (UV трёх углов треугольника, слой и
// анимация) хранятся в массиве faces (FaceData); i-я грань соответствует
// треугольнику indices[3*i .. 3*i+2].
class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<FaceData> faces;

    // Увеличивается при пересчёте освещения вершин. Рендерер использует это
    // как версию GPU-кэша, чтобы не заливать неизменившийся меш каждый кадр.
    uint32_t light_revision = 0;

    // Простые перегрузки: UV по умолчанию (0..1), layer/anim = 0.
    uint32_t add_vertex(float x, float y, float z);
    uint32_t add_vertex(const Vec3f& pos, uint8_t block_light = 255, uint8_t sky_light = 0);
    uint32_t add_vertex(const Vertex& vertex);

    // Перегрузка для совместимости со старым кодом: uv/layer/anim игнорируются
    // на уровне вершины (теперь это принадлежит граням).
    uint32_t add_vertex(float x, float y, float z, float u, float v, int32_t layer,
                        uint8_t anim_data, uint8_t block_light = 255, uint8_t sky_light = 0);
    uint32_t add_vertex(const Vec3f& pos, const Vec2f& uv, int32_t layer,
                        uint8_t anim_data, uint8_t block_light = 255, uint8_t sky_light = 0);

    // Треугольник с данными грани (uv берётся из face, а не из вершин).
    void add_triangle(uint32_t i0, uint32_t i1, uint32_t i2, const FaceData& face);
    void add_triangle(const Vec3i& indices_vec, const FaceData& face);

    // Устаревшие перегрузки: грань создаётся с дефолтными uv (0..1), layer = 0.
    void add_triangle(uint32_t i0, uint32_t i1, uint32_t i2);
    void add_triangle(const Vec3i& indices_vec);

    void clear();

    uint32_t vertex_count() const { return static_cast<uint32_t>(vertices.size()); }
    uint32_t index_count() const { return static_cast<uint32_t>(indices.size()); }
    uint32_t triangle_count() const { return static_cast<uint32_t>(indices.size() / 3); }
    uint32_t face_count() const { return static_cast<uint32_t>(faces.size()); }
};
