#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "camera.h"
#include "render.h"


namespace py = pybind11;

PYBIND11_MODULE(camera_module, m) {
    py::module_::import("vec_module");
    py::module_::import("entity_module");
    py::module_::import("graphics_module");
    
    m.doc() = "Camera module";
    
    py::class_<Camera, Entity, std::shared_ptr<Camera>>(m, "Camera")
        .def(py::init<>())
        .def(py::init<Render*, const Vec3f&, const Vec3r&>(),
             py::arg("renderer"), py::arg("position"), py::arg("rotation"))
        .def_readwrite("fov", &Camera::fov)
        .def_readwrite("aspect_ratio", &Camera::aspect_ratio)
        .def_readwrite("near_plane", &Camera::near_plane)
        .def_readwrite("far_plane", &Camera::far_plane)
        .def("update_matrices", &Camera::update_matrices)
        .def("set_renderer", &Camera::set_renderer, py::arg("renderer"))
        .def("get_view_matrix", &Camera::get_view_matrix, py::return_value_policy::reference)
        .def("get_projection_matrix", &Camera::get_projection_matrix, py::return_value_policy::reference)
        .def("__repr__", [](const Camera& c) {
            return "Camera(pos=(" + std::to_string(c.transform.position.x) + ", " +
                   std::to_string(c.transform.position.y) + ", " +
                   std::to_string(c.transform.position.z) + "))";
        });
}


