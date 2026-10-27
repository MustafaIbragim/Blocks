#include <pybind11/pybind11.h>

#include "mesh.h"
#include "mesh_lighting.h"
#include "world.h"


namespace py = pybind11;

PYBIND11_MODULE(mesh_lighting_module, m) {
    py::module_::import("vec_module");
    py::module_::import("world_module");
    py::module_::import("mesh_module");

    m.doc() = "Пересчёт освещения мешей сущностей по свету воксельного мира";

    m.attr("DEFAULT_SEARCH_RADIUS") = MeshLighting::DEFAULT_SEARCH_RADIUS;
    m.attr("MAX_SOURCES") = MeshLighting::MAX_SOURCES;

    m.def("sample_vertex_light", &MeshLighting::sample_vertex_light,
          py::arg("world"), py::arg("position"),
          py::arg("search_radius") = MeshLighting::DEFAULT_SEARCH_RADIUS,
          "Освещённость точки мира (0..255) по до 4 ближайшим светам");

    m.def("update_mesh_lighting", &MeshLighting::update_mesh_lighting,
          py::arg("world"), py::arg("mesh"), py::arg("origin"),
          py::arg("search_radius") = MeshLighting::DEFAULT_SEARCH_RADIUS,
          "Пересчитывает block_light вершин меша; origin — центр его "
          "локальных координат в мире");
}
