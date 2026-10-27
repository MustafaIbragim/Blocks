#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "raycast.h"
#include "vec.h"
#include "world.h"


namespace py = pybind11;

PYBIND11_MODULE(raycast_module, m) {
    py::module_::import("vec_module");
    py::module_::import("world_module");

    m.doc() = "Voxel raycast and view-direction helpers";

    py::enum_<Axis>(m, "Axis")
        .value("NONE", Axis::NONE)
        .value("X", Axis::X)
        .value("Y", Axis::Y)
        .value("Z", Axis::Z)
        .export_values();

    py::class_<RaycastHit>(m, "RaycastHit")
        .def_readonly("hit", &RaycastHit::hit)
        .def_readonly("block", &RaycastHit::block)
        .def_readonly("normal", &RaycastHit::normal)
        .def_readonly("nbt", &RaycastHit::nbt)
        .def_readonly("distance", &RaycastHit::distance)
        .def("__bool__", [](const RaycastHit& hit) { return hit.hit; })
        .def("__repr__", [](const RaycastHit& hit) {
            if (!hit.hit) {
                return std::string("RaycastHit(hit=False)");
            }
            return "RaycastHit(block=(" + std::to_string(hit.block.x) + ", " +
                   std::to_string(hit.block.y) + ", " +
                   std::to_string(hit.block.z) + "), normal=(" +
                   std::to_string(hit.normal.x) + ", " +
                   std::to_string(hit.normal.y) + ", " +
                   std::to_string(hit.normal.z) + "))";
        });

    py::class_<ViewDirections>(m, "ViewDirections")
        .def(py::init<const Vec3r&>(), py::arg("rotation"))
        .def("set_rotation", &ViewDirections::set_rotation, py::arg("rotation"))
        .def_property_readonly("rotation",
             [](const ViewDirections& self) -> const Vec3r& {
                 return self.get_rotation();
             },
             py::return_value_policy::reference_internal)
        .def("create_forward", &ViewDirections::create_forward,
             py::arg("ignore_axis") = Axis::NONE)
        .def("create_back", &ViewDirections::create_back,
             py::arg("ignore_axis") = Axis::NONE)
        .def("create_right", &ViewDirections::create_right,
             py::arg("ignore_axis") = Axis::NONE)
        .def("create_left", &ViewDirections::create_left,
             py::arg("ignore_axis") = Axis::NONE)
        .def("create_up", &ViewDirections::create_up,
             py::arg("ignore_axis") = Axis::NONE)
        .def("create_down", &ViewDirections::create_down,
             py::arg("ignore_axis") = Axis::NONE)
        .def("create_forward_flat", &ViewDirections::create_forward_flat)
        .def("create_back_flat", &ViewDirections::create_back_flat)
        .def("create_right_flat", &ViewDirections::create_right_flat)
        .def("create_left_flat", &ViewDirections::create_left_flat)
        .def("create_forward_ignore_x", &ViewDirections::create_forward_ignore_x)
        .def("create_forward_ignore_y", &ViewDirections::create_forward_ignore_y)
        .def("create_forward_ignore_z", &ViewDirections::create_forward_ignore_z)
        .def("create_right_ignore_x", &ViewDirections::create_right_ignore_x)
        .def("create_right_ignore_y", &ViewDirections::create_right_ignore_y)
        .def("create_right_ignore_z", &ViewDirections::create_right_ignore_z)
        .def("create_up_ignore_x", &ViewDirections::create_up_ignore_x)
        .def("create_up_ignore_y", &ViewDirections::create_up_ignore_y)
        .def("create_up_ignore_z", &ViewDirections::create_up_ignore_z)
        .def("__repr__", [](const ViewDirections& self) {
            return "ViewDirections(yaw=" + std::to_string(self.get_rotation().x) +
                   ", pitch=" + std::to_string(self.get_rotation().y) + ")";
        });

    m.def("raycast_voxel", &raycast_voxel,
          py::arg("world"), py::arg("origin"), py::arg("direction"),
          py::arg("max_distance") = 8.0f);
}
