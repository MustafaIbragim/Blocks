#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "dummy.h"
#include "view_cam.h"


namespace py = pybind11;

PYBIND11_MODULE(dummy_module, m) {
    py::module_::import("vec_module");
    py::module_::import("entity_module");
    py::module_::import("renderer_module");

    m.doc() = "Dummy entity owning a ViewCam";

    py::class_<Dummy, Entity, std::shared_ptr<Dummy>>(m, "Dummy")
        .def_readonly_static("BLOCK_SIZE", &Dummy::BLOCK_SIZE)
        .def_readonly_static("BODY_RADIUS", &Dummy::BODY_RADIUS)
        .def_readonly_static("BODY_HEIGHT", &Dummy::BODY_HEIGHT)
        .def_readonly_static("CAPSULE_HEIGHT", &Dummy::CAPSULE_HEIGHT)
        .def_readonly_static("EYE_OFFSET", &Dummy::EYE_OFFSET)
        .def_readonly_static("MAX_PITCH", &Dummy::MAX_PITCH)
        .def(py::init<>())
        .def(py::init<const Vec3f&, const Vec3r&>(),
             py::arg("position"), py::arg("rotation"))
        .def_readwrite("view", &Dummy::view)
        .def_readwrite("collisions_enabled", &Dummy::collisions_enabled)
        .def_readwrite("gravity_enabled", &Dummy::gravity_enabled)
        .def("set_view", &Dummy::set_view, py::arg("view"))
        .def("set_yaw", &Dummy::set_yaw, py::arg("yaw"))
        .def("get_yaw", &Dummy::get_yaw)
        .def("rotate_yaw", &Dummy::rotate_yaw, py::arg("delta"))
        .def("set_pitch", &Dummy::set_pitch, py::arg("pitch"))
        .def("get_pitch", &Dummy::get_pitch)
        .def("rotate_pitch", &Dummy::rotate_pitch, py::arg("delta"))
        .def("get_eye_position", &Dummy::get_eye_position)
        .def_property("yaw", &Dummy::get_yaw, &Dummy::set_yaw)
        .def_property("pitch", &Dummy::get_pitch, &Dummy::set_pitch)
        .def("sync_view_from_transform", &Dummy::sync_view_from_transform)
        .def("sync_transform_from_view", &Dummy::sync_transform_from_view)
        .def("set_collisions_enabled", &Dummy::set_collisions_enabled,
             py::arg("enabled"))
        .def("set_gravity_enabled", &Dummy::set_gravity_enabled,
             py::arg("enabled"))
        .def("toggle_physics", &Dummy::toggle_physics)
        .def("update", &Dummy::update, py::arg("tick"))
        .def("__repr__", [](const Dummy& dummy) {
            return "Dummy(pos=(" + std::to_string(dummy.view.position.x) + ", " +
                   std::to_string(dummy.view.position.y) + ", " +
                   std::to_string(dummy.view.position.z) + "))";
        });
}
