#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "camera.h"
#include "renderer.h"
#include "view_cam.h"


namespace py = pybind11;

PYBIND11_MODULE(camera_module, m) {
    py::module_::import("vec_module");
    py::module_::import("entity_module");
    py::module_::import("renderer_module");

    m.doc() = "Camera entity with ViewCam and Renderer reference";

    py::class_<Camera, Entity, std::shared_ptr<Camera>>(m, "Camera")
        .def(py::init<>())
        .def(py::init<Renderer*>(), py::arg("renderer"))
        .def(py::init<Renderer*, const ViewCam&>(),
             py::arg("renderer"), py::arg("view"))
        .def_readwrite("view", &Camera::view)
        .def_readwrite("renderer", &Camera::renderer)
        .def("set_renderer", &Camera::set_renderer, py::arg("renderer"))
        .def("get_renderer", &Camera::get_renderer,
             py::return_value_policy::reference)
        .def("set_view", &Camera::set_view, py::arg("view"))
        .def("sync_view_from_transform", &Camera::sync_view_from_transform)
        .def("sync_transform_from_view", &Camera::sync_transform_from_view)
        .def("update", &Camera::update,
             py::arg("tick"), py::arg("gravity") = 0.0f)
        .def("render", &Camera::render,
             py::arg("current_tick"), py::arg("min_light") = 0.1f,
             py::arg("day_light") = 1.0f,
             "Рисует сцену. day_light: 0 — ночь, 1 — день.")
        .def("__repr__", [](const Camera& camera) {
            return "Camera(pos=(" + std::to_string(camera.view.position.x) + ", " +
                   std::to_string(camera.view.position.y) + ", " +
                   std::to_string(camera.view.position.z) + "))";
        });
}
