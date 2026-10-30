#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>

#include "dummy.h"
#include "item.h"
#include "view_cam.h"
#include "world.h"


namespace py = pybind11;

PYBIND11_MODULE(dummy_module, m) {
    py::module_::import("vec_module");
    py::module_::import("entity_module");
    py::module_::import("renderer_module");
    py::module_::import("collision_module");
    py::module_::import("movement_module");

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
        .def_property(
            "collisions_enabled",
            [](const Dummy& dummy) { return dummy.collisions_enabled; },
            [](Dummy& dummy, bool value) {
                dummy.set_collisions_enabled(value);
            })
        .def_property(
            "gravity_enabled",
            [](const Dummy& dummy) { return dummy.gravity_enabled; },
            [](Dummy& dummy, bool value) {
                dummy.set_gravity_enabled(value);
            })
        .def_property_readonly(
            "body", [](Dummy& dummy) -> EntityCollision& { return dummy.body; },
            py::return_value_policy::reference_internal)
        .def_property_readonly(
            "mover", [](Dummy& dummy) -> GroundMover& { return dummy.mover; },
            py::return_value_policy::reference_internal)
        .def_readwrite("step_height", &Dummy::step_height)
        .def("move_to",
             [](Dummy& self, World& world, const Vec3i& from,
                const Vec3i& to, float duration,
                std::optional<float> step_height) {
                 return self.move_to(world, from, to, duration, step_height);
             },
             py::arg("world"), py::arg("start"), py::arg("to"),
             py::arg("duration"), py::arg("step_height") = py::none())
        .def("move_to",
             [](Dummy& self, World& world, const Vec3i& to,
                float duration, std::optional<float> step_height) {
                 return self.move_to(world, to, duration, step_height);
             },
             py::arg("world"), py::arg("to"), py::arg("duration"),
             py::arg("step_height") = py::none())
        .def("update_movement", &Dummy::update_movement,
             py::arg("world"), py::arg("delta_time"))
        .def("move_towards",
             [](Dummy& self, World& world, const Vec3f& direction,
                float speed, float delta_time,
                std::optional<float> step_height) {
                 self.move_towards(world, direction, speed, delta_time,
                                   step_height);
             },
             py::arg("world"), py::arg("direction"), py::arg("speed"),
             py::arg("delta_time"), py::arg("step_height") = py::none())
        .def("stop_movement", &Dummy::stop_movement)
        .def("is_moving", &Dummy::is_moving)
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
        .def("sync_body_from_transform", &Dummy::sync_body_from_transform)
        .def("sync_transform_from_body", &Dummy::sync_transform_from_body)
        .def("sync_view_from_body", &Dummy::sync_view_from_body,
             py::arg("alpha") = 0.0f)
        .def("set_collisions_enabled", &Dummy::set_collisions_enabled,
             py::arg("enabled"))
        .def("set_gravity_enabled", &Dummy::set_gravity_enabled,
             py::arg("enabled"))
        .def("toggle_physics", &Dummy::toggle_physics)
        .def("update", &Dummy::update,
             py::arg("tick"), py::arg("gravity") = 0.0f)
        .def("__repr__", [](const Dummy& dummy) {
            return "Dummy(pos=(" + std::to_string(dummy.view.position.x) + ", " +
                   std::to_string(dummy.view.position.y) + ", " +
                   std::to_string(dummy.view.position.z) + "))";
        });

    py::class_<DummyPickupTrigger, TriggerCollision,
               std::shared_ptr<DummyPickupTrigger>>(m, "DummyPickupTrigger")
        .def(py::init<>())
        .def(py::init<Dummy&, World&, Inventory&, uint32_t, float>(),
             py::arg("dummy"), py::arg("world"), py::arg("inventory"),
             py::arg("preferred_slot") = UINT32_MAX,
             py::arg("radius") = 4.0f)
        .def_readwrite("dummy", &DummyPickupTrigger::dummy)
        .def_readwrite("world", &DummyPickupTrigger::world)
        .def_readwrite("inventory", &DummyPickupTrigger::inventory)
        .def_readwrite("preferred_slot", &DummyPickupTrigger::preferred_slot)
        .def("sync_bounds", &DummyPickupTrigger::sync_bounds);
}
