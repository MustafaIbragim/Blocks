#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "mesh.h"


namespace py = pybind11;

PYBIND11_MODULE(mesh_module, m) {
    py::module_::import("vec_module");
    
    m.doc() = "Mesh module";
    
    py::class_<Mesh, std::shared_ptr<Mesh>>(m, "Mesh")
        .def(py::init<>())
        
        .def("add_vertex", static_cast<uint32_t (Mesh::*)(float, float, float, float, float, int32_t, uint8_t, uint8_t, uint8_t)>(&Mesh::add_vertex),
             py::arg("x"), py::arg("y"), py::arg("z"),
             py::arg("u"), py::arg("v"),
             py::arg("layer"), py::arg("anim_data"),
             py::arg("block_light") = 255, py::arg("sky_light") = 0)
        
        .def("add_vertex", static_cast<uint32_t (Mesh::*)(const Vec3f&, const Vec2f&, int32_t, uint8_t, uint8_t, uint8_t)>(&Mesh::add_vertex),
             py::arg("pos"), py::arg("uv"),
             py::arg("layer"), py::arg("anim_data"),
             py::arg("block_light") = 255, py::arg("sky_light") = 0)
        
        .def("add_vertex", static_cast<uint32_t (Mesh::*)(const Vertex&)>(&Mesh::add_vertex),
             py::arg("vertex"))
        
        .def("add_triangle", static_cast<void (Mesh::*)(uint32_t, uint32_t, uint32_t)>(&Mesh::add_triangle),
             py::arg("i0"), py::arg("i1"), py::arg("i2"))
        
        .def("add_triangle", static_cast<void (Mesh::*)(const Vec3i&)>(&Mesh::add_triangle),
             py::arg("indices_vec"))
        
        .def("clear", &Mesh::clear)
        
        .def_property_readonly("vertex_count", &Mesh::vertex_count)
        .def_property_readonly("index_count", &Mesh::index_count)
        .def_property_readonly("triangle_count", &Mesh::triangle_count)
        
        .def_property_readonly("vertices_np", [](Mesh& self) {
            size_t count = self.vertices.size();
            std::vector<py::ssize_t> shape = {static_cast<py::ssize_t>(count), 7};
            std::vector<py::ssize_t> strides = {7 * sizeof(float), sizeof(float)};
            
            return py::array_t<float>(
                shape,
                strides,
                reinterpret_cast<float*>(self.vertices.data()),
                py::capsule(&self)
            );
        })
        
        .def("__repr__", [](const Mesh& mesh) {
            return "Mesh(vertices=" + std::to_string(mesh.vertex_count()) + 
                   ", triangles=" + std::to_string(mesh.triangle_count()) + ")";
        });
}


