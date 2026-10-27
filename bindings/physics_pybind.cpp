#include <memory>

#include <pybind11/pybind11.h>

#include <btBulletDynamicsCommon.h>

#include "block.h"
#include "physics_world.h"
#include "vec.h"
#include "world.h"


namespace py = pybind11;

PYBIND11_MODULE(physics_module, m) {
    py::module_::import("vec_module");
    py::module_::import("block_module");
    py::module_::import("world_module");
    py::module_::import("bullet_module");
    py::module_::import("voxel_collision_shape_module");

    m.doc() = "Bullet Physics integration for the voxel world";

    // Совместимость: типы Bullet и VoxelCollisionShape теперь живут в
    // отдельных модулях, но остаются доступны через physics_module.
    py::module_ bullet = py::module_::import("bullet_module");
    py::module_ voxel = py::module_::import("voxel_collision_shape_module");
    m.attr("CollisionShape") = bullet.attr("CollisionShape");
    m.attr("ConcaveShape") = bullet.attr("ConcaveShape");
    m.attr("BoxShape") = bullet.attr("BoxShape");
    m.attr("make_box_shape") = bullet.attr("make_box_shape");
    m.attr("VoxelCollisionShape") = voxel.attr("VoxelCollisionShape");

    py::class_<PhysicsWorld>(m, "PhysicsWorld")
        .def(py::init<>())
        // Статические collision objects чанков
        .def("add_chunk", &PhysicsWorld::add_chunk,
             py::arg("world"), py::arg("chunk_pos"))
        .def("remove_chunk", &PhysicsWorld::remove_chunk, py::arg("chunk_pos"))
        .def("has_chunk", &PhysicsWorld::has_chunk, py::arg("chunk_pos"))
        .def("clear_chunks", &PhysicsWorld::clear_chunks)
        // Динамические тела
        .def("add_dynamic_box", &PhysicsWorld::add_dynamic_box,
             py::arg("half_extents"), py::arg("mass"), py::arg("position"))
        .def("add_dynamic_sphere", &PhysicsWorld::add_dynamic_sphere,
             py::arg("radius"), py::arg("mass"), py::arg("position"))
        .def("add_dynamic_capsule", &PhysicsWorld::add_dynamic_capsule,
             py::arg("radius"), py::arg("cylinder_height"),
             py::arg("mass"), py::arg("position"))
        .def("remove_body", &PhysicsWorld::remove_body, py::arg("body_id"))
        .def("has_body", &PhysicsWorld::has_body, py::arg("body_id"))
        .def("get_body_position", &PhysicsWorld::get_body_position, py::arg("body_id"))
        .def("get_body_rotation", &PhysicsWorld::get_body_rotation, py::arg("body_id"))
        .def("get_body_linear_velocity", &PhysicsWorld::get_body_linear_velocity,
             py::arg("body_id"))
        .def("get_body_gravity", &PhysicsWorld::get_body_gravity,
             py::arg("body_id"))
        .def("set_body_position", &PhysicsWorld::set_body_position,
             py::arg("body_id"), py::arg("position"))
        .def("set_body_linear_velocity", &PhysicsWorld::set_body_linear_velocity,
             py::arg("body_id"), py::arg("velocity"))
        .def("set_body_gravity", &PhysicsWorld::set_body_gravity,
             py::arg("body_id"), py::arg("gravity"))
        .def("set_body_angular_factor", &PhysicsWorld::set_body_angular_factor,
             py::arg("body_id"), py::arg("factor"))
        .def("get_body_angular_factor", &PhysicsWorld::get_body_angular_factor,
             py::arg("body_id"))
        .def("set_body_collisions_enabled", &PhysicsWorld::set_body_collisions_enabled,
             py::arg("body_id"), py::arg("enabled"))
        .def("get_body_collisions_enabled", &PhysicsWorld::get_body_collisions_enabled,
             py::arg("body_id"))
        .def("activate_body", &PhysicsWorld::activate_body, py::arg("body_id"))
        // Мир
        .def("set_gravity", &PhysicsWorld::set_gravity, py::arg("gravity"))
        .def("step", &PhysicsWorld::step,
             py::arg("delta_time"),
             py::arg("max_substeps") = 1,
             py::arg("fixed_time_step") = 1.0f / 60.0f)
        .def("get_num_collision_objects", &PhysicsWorld::get_num_collision_objects)
        .def("get_num_manifolds", &PhysicsWorld::get_num_manifolds);

    m.def("set_block_collision",
          [](Block& block, std::shared_ptr<btCollisionShape> shape) {
              block.set_collision_shape(std::move(shape));
          },
          py::arg("block"), py::arg("shape"));
    m.def("get_block_collision",
          [](const Block& block) { return block.get_collision_shape(); },
          py::arg("block"));
    m.def("clear_block_collision",
          [](Block& block) { block.clear_collision_shape(); },
          py::arg("block"));
    m.def("block_has_collision",
          [](const Block& block) { return block.has_collision(); },
          py::arg("block"));
}
