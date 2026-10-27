#include <pybind11/pybind11.h>

#include "view_cam.h"


namespace py = pybind11;

PYBIND11_MODULE(view_cam_module, m) {
    py::module_::import("vec_module");

    m.doc() = "Camera view parameters shared by renderer, camera and dummy";

    py::class_<ViewCam>(m, "ViewCam")
        .def(py::init<>())
        .def_readwrite("position", &ViewCam::position)
        .def_readwrite("rotation", &ViewCam::rotation)
        .def_readwrite("fov", &ViewCam::fov)
        .def_readwrite("near_plane", &ViewCam::near_plane)
        .def_readwrite("far_plane", &ViewCam::far_plane)
        .def("__repr__", [](const ViewCam& cam) {
            return "ViewCam(pos=(" + std::to_string(cam.position.x) + ", " +
                   std::to_string(cam.position.y) + ", " +
                   std::to_string(cam.position.z) + "))";
        });
}
