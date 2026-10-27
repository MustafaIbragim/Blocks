#pragma once

#include <cstdint>

#include "vec.h"


class Mesh;
class World;


// Пересчёт освещения произвольного меша по воксельному свету мира.
//
// Модуль не зависит от Item/Dummy и подходит для любой сущности: достаточно
// передать её меш и мировую позицию его локального начала координат. Для
// каждой вершины берутся до четырёх ближайших ненулевых значений
// Chunk::light_data. Расстояние — манхэттенское (без sqrt и pow), вес —
// 1 / (1 + distance). Итог — взвешенное среднее, записанное в
// Vertex::block_light.
namespace MeshLighting {

// Радиус поиска в блоках по каждой оси. Пяти/семи соседних клеток обычно
// достаточно: в освещённой области ближайшие ненулевые значения находятся
// рядом с вершиной, а в полной темноте источников всё равно нет.
constexpr int DEFAULT_SEARCH_RADIUS = 3;

// До четырёх ближайших источников на вершину.
constexpr int MAX_SOURCES = 4;

// Освещённость одной точки мира (0..255).
uint8_t sample_vertex_light(const World& world,
                            const Vec3f& world_position,
                            int search_radius = DEFAULT_SEARCH_RADIUS);

// Пересчитывает block_light всех вершин. origin — мировая позиция локального
// (0, 0, 0) меша. Возвращает true, если хотя бы одно значение изменилось.
// При изменении увеличивает Mesh::light_revision, чтобы рендерер мог
// перезалить GPU-буфер.
bool update_mesh_lighting(const World& world,
                          Mesh& mesh,
                          const Vec3f& origin,
                          int search_radius = DEFAULT_SEARCH_RADIUS);

} // namespace MeshLighting
