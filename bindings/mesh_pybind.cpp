#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "mesh.h"


namespace py = pybind11;

PYBIND11_MODULE(mesh_module, m) {
    py::module_::import("vec_module");

    m.doc() = "Mesh module";

    py::class_<FaceData>(m, "FaceData")
        .def(py::init<>())
        .def_readwrite("u_0", &FaceData::u_0)
        .def_readwrite("u_1", &FaceData::u_1)
        .def_readwrite("u_2", &FaceData::u_2)
        .def_readwrite("v_0", &FaceData::v_0)
        .def_readwrite("v_1", &FaceData::v_1)
        .def_readwrite("v_2", &FaceData::v_2)
        .def_readwrite("layer", &FaceData::layer)
        .def_readwrite("anim_data", &FaceData::anim_data)
        .def("__repr__", [](const FaceData& f) {
            return "FaceData(layer=" + std::to_string(f.layer) +
                   ", anim_data=" + std::to_string(f.anim_data) + ")";
        });

    py::class_<Mesh, std::shared_ptr<Mesh>>(m, "Mesh")
        .def(py::init<>())

        // Вершина: только позиция и свет (uv/текстура теперь принадлежат граням)
        .def("add_vertex", static_cast<uint32_t (Mesh::*)(float, float, float)>(&Mesh::add_vertex),
             py::arg("x"), py::arg("y"), py::arg("z"))

        .def("add_vertex", static_cast<uint32_t (Mesh::*)(const Vec3f&, uint8_t, uint8_t)>(&Mesh::add_vertex),
             py::arg("pos"), py::arg("block_light") = 255, py::arg("sky_light") = 0)

        .def("add_vertex", static_cast<uint32_t (Mesh::*)(const Vertex&)>(&Mesh::add_vertex),
             py::arg("vertex"))

        // Треугольник с данными грани
        .def("add_triangle", static_cast<void (Mesh::*)(uint32_t, uint32_t, uint32_t, const FaceData&)>(&Mesh::add_triangle),
             py::arg("i0"), py::arg("i1"), py::arg("i2"), py::arg("face"))

        .def("add_triangle", static_cast<void (Mesh::*)(const Vec3i&, const FaceData&)>(&Mesh::add_triangle),
             py::arg("indices_vec"), py::arg("face"))

        // Совместимость со старым API: face создаётся с дефолтными значениями
        .def("add_triangle", static_cast<void (Mesh::*)(uint32_t, uint32_t, uint32_t)>(&Mesh::add_triangle),
             py::arg("i0"), py::arg("i1"), py::arg("i2"))

        .def("add_triangle", static_cast<void (Mesh::*)(const Vec3i&)>(&Mesh::add_triangle),
             py::arg("indices_vec"))

        .def("clear", &Mesh::clear)

        .def_property_readonly("vertex_count", &Mesh::vertex_count)
        .def_property_readonly("index_count", &Mesh::index_count)
        .def_property_readonly("triangle_count", &Mesh::triangle_count)
        .def_property_readonly("face_count", &Mesh::face_count)

        // Нулевая копия вершин: (N, 4) float32 — x, y, z, block_light
        .def_property_readonly("vertices_np", [](Mesh& self) {
            size_t count = self.vertices.size();
            std::vector<py::ssize_t> shape = {static_cast<py::ssize_t>(count), 4};
            std::vector<py::ssize_t> strides = {static_cast<py::ssize_t>(sizeof(Vertex)), sizeof(float)};

            return py::array_t<float>(
                shape,
                strides,
                reinterpret_cast<float*>(self.vertices.data()),
                py::capsule(&self)
            );
        }, "Массив вершин (N, 4): x, y, z, block_light. Без копирования.")

        // Нулевая копия граней: (F, 8) float32 — u_0..u_2, v_0..v_2, layer, anim_data
        .def_property_readonly("faces_np", [](Mesh& self) {
            size_t count = self.faces.size();
            std::vector<py::ssize_t> shape = {static_cast<py::ssize_t>(count), 8};
            std::vector<py::ssize_t> strides = {static_cast<py::ssize_t>(sizeof(FaceData)), sizeof(float)};

            return py::array_t<float>(
                shape,
                strides,
                reinterpret_cast<float*>(self.faces.data()),
                py::capsule(&self)
            );
        }, "Массив граней (F, 8): u_0,u_1,u_2,v_0,v_1,v_2,layer,anim_data. Без копирования.")

        .def("__repr__", [](const Mesh& mesh) {
            return "Mesh(vertices=" + std::to_string(mesh.vertex_count()) +
                   ", triangles=" + std::to_string(mesh.triangle_count()) +
                   ", faces=" + std::to_string(mesh.face_count()) + ")";
        });
}
