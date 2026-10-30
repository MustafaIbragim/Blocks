#include <optional>
#include <string>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "collision.h"
#include "movement.h"
#include "vec.h"
#include "world.h"


namespace py = pybind11;

PYBIND11_MODULE(movement_module, m) {
    // Не импортируем world_module/collision_module здесь: item_module и
    // dummy_module сами импортируют movement_module, и жёсткий импорт
    // world_module создал бы цикл. Типы World/DynamicCollision уже
    // регистрируются соответствующими модулями до вызова методов.
    py::module_::import("vec_module");

    m.doc() = "Block-to-block pathfinding and movement for ground/flying entities";

    py::class_<PathNode>(m, "PathNode")
        .def(py::init<>())
        .def(py::init<const Vec3i&, float>(),
             py::arg("block"), py::arg("feet_y"))
        .def_readwrite("block", &PathNode::block)
        .def_readwrite("feet_y", &PathNode::feet_y)
        .def("feet_position", &PathNode::feet_position)
        .def("body_center", &PathNode::body_center,
             py::arg("half_extents"))
        .def("__eq__", &PathNode::operator==, py::is_operator())
        .def("__ne__", &PathNode::operator!=, py::is_operator())
        .def("__repr__", [](const PathNode& node) {
            return "PathNode(block=(" + std::to_string(node.block.x) + ", " +
                   std::to_string(node.block.y) + ", " +
                   std::to_string(node.block.z) + "), feet_y=" +
                   std::to_string(node.feet_y) + ")";
        });

    py::class_<PathMover>(m, "PathMover")
        .def("find_path",
             [](PathMover& self, World& world, const Vec3i& start,
                const Vec3i& target) {
                 return self.find_path(world, start, target);
             },
             py::arg("world"), py::arg("start"), py::arg("target"))
        .def("find_path",
             [](PathMover& self, World& world, const Vec3i& start,
                const Vec3i& target, float step_height) {
                 return self.find_path(world, start, target, step_height);
             },
             py::arg("world"), py::arg("start"), py::arg("target"),
             py::arg("step_height"))
        .def("find_path",
             [](PathMover& self, World& world, const Vec3i& start,
                const Vec3i& target, const Vec3f& half_extents) {
                 return self.find_path(world, start, target, half_extents);
             },
             py::arg("world"), py::arg("start"), py::arg("target"),
             py::arg("half_extents"))
        .def("find_path",
             [](PathMover& self, World& world, const Vec3i& start,
                const Vec3i& target, const Vec3f& half_extents,
                float step_height) {
                 return self.find_path(world, start, target, half_extents,
                                       step_height);
             },
             py::arg("world"), py::arg("start"), py::arg("target"),
             py::arg("half_extents"), py::arg("step_height"))
        .def("move_to",
             [](PathMover& self, World& world, DynamicCollision& body,
                const Vec3i& start, const Vec3i& target, float duration,
                std::optional<float> step_height, bool snap_to_start) {
                 return self.move_to(world, body, start, target, duration,
                                     step_height, snap_to_start);
             },
             py::arg("world"), py::arg("body"), py::arg("start"),
             py::arg("target"), py::arg("duration"),
             py::arg("step_height") = py::none(),
             py::arg("snap_to_start") = true)
        .def("move_to",
             [](PathMover& self, World& world, DynamicCollision& body,
                const Vec3i& target, float duration,
                std::optional<float> step_height, bool snap_to_start) {
                 return self.move_to(world, body, target, duration,
                                     step_height, snap_to_start);
             },
             py::arg("world"), py::arg("body"), py::arg("target"),
             py::arg("duration"), py::arg("step_height") = py::none(),
             py::arg("snap_to_start") = true)
        .def("update", &PathMover::update,
             py::arg("world"), py::arg("body"), py::arg("delta_time"))
        .def("stop", &PathMover::stop)
        .def("clear_path", &PathMover::clear_path)
        .def("has_path", &PathMover::has_path)
        .def("is_moving", &PathMover::is_moving)
        .def("current_index", &PathMover::current_index)
        .def("current_node", &PathMover::current_node)
        .def("target_node", &PathMover::target_node)
        .def_property_readonly("path", &PathMover::get_path)
        .def_property_readonly("path_blocks", &PathMover::get_path_blocks)
        .def_property_readonly("path_length", &PathMover::path_length)
        .def_property("speed", &PathMover::speed, &PathMover::set_speed)
        .def_property("arrival_radius", &PathMover::arrival_radius,
                      &PathMover::set_arrival_radius)
        .def_property("step_height", &PathMover::step_height,
                      &PathMover::set_step_height)
        .def_property("half_extents", &PathMover::half_extents,
                      &PathMover::set_half_extents)
        .def_static("current_block", &PathMover::current_block,
                    py::arg("body"));

    py::class_<GroundMover, PathMover>(m, "GroundMover")
        .def(py::init<>())
        .def(py::init<float>(), py::arg("step_height"))
        .def(py::init<float, float, float>(),
             py::arg("body_radius"), py::arg("body_height"),
             py::arg("step_height") = 0.6f)
        .def_static("body_free", &GroundMover::body_free,
                    py::arg("world"), py::arg("feet_position"),
                    py::arg("half_extents"))
        .def_static("support_height",
                    [](World& world, int x, int z,
                       const Vec3f& half_extents, float max_y,
                       float max_drop) -> std::optional<float> {
                        float height = 0.0f;
                        if (!GroundMover::support_height(
                                world, x, z, half_extents, max_y,
                                max_drop, height)) {
                            return std::nullopt;
                        }
                        return height;
                    },
                    py::arg("world"), py::arg("x"), py::arg("z"),
                    py::arg("half_extents"), py::arg("max_y"),
                    py::arg("max_drop"))
        .def_static("try_step_up",
                    [](World& world, DynamicCollision& body,
                       const Vec3f& desired_delta, float step_height) {
                        return GroundMover::try_step_up(
                            world, body, desired_delta, step_height);
                    },
                    py::arg("world"), py::arg("body"),
                    py::arg("desired_delta"), py::arg("step_height"));

    py::class_<FlyMover, PathMover>(m, "FlyMover")
        .def(py::init<>())
        .def(py::init<float, float>(),
             py::arg("body_radius"), py::arg("body_height"));
}
