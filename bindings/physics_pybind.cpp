#include <memory>

#include <pybind11/pybind11.h>

#include <btBulletDynamicsCommon.h>

#include "block.h"
#include "physics_world.h"
#include "vec.h"
#include "voxel_collision_shape.h"
#include "world.h"


namespace py = pybind11;

PYBIND11_MODULE(physics_module, m) {
    py::module_::import("vec_module");
    py::module_::import("block_module");
    py::module_::import("world_module");

    m.doc() = "Bullet Physics integration for the voxel world";

    // Базовые Bullet-типы нужны, чтобы из Python можно было назначать
    // блокам собственные collision shapes (flyweight: один shape на тип).
    py::class_<btCollisionShape, std::shared_ptr<btCollisionShape>>(m, "CollisionShape")
        .def("get_margin", &btCollisionShape::getMargin)
        .def("set_margin", &btCollisionShape::setMargin, py::arg("margin"))
        .def("get_name", &btCollisionShape::getName);

    py::class_<btConcaveShape, btCollisionShape, std::shared_ptr<btConcaveShape>>(
        m, "ConcaveShape");

    py::class_<btBoxShape, btCollisionShape, std::shared_ptr<btBoxShape>>(m, "BoxShape")
        .def_property_readonly("half_extents", [](const btBoxShape& box) {
            const btVector3 h = box.getHalfExtentsWithoutMargin();
            return Vec3f(h.x(), h.y(), h.z());
        });

    py::class_<VoxelCollisionShape, btConcaveShape, std::shared_ptr<VoxelCollisionShape>>(
        m, "VoxelCollisionShape")
        .def(py::init<World&, const Vec2i&>(),
             py::arg("world"), py::arg("chunk_pos"))
        .def_property_readonly("world_origin", [](const VoxelCollisionShape& shape) {
            const btVector3& origin = shape.getWorldOrigin();
            return Vec3f(origin.x(), origin.y(), origin.z());
        })
        .def_property_readonly("local_min", [](const VoxelCollisionShape& shape) {
            const btVector3& v = shape.getLocalMin();
            return Vec3f(v.x(), v.y(), v.z());
        })
        .def_property_readonly("local_max", [](const VoxelCollisionShape& shape) {
            const btVector3& v = shape.getLocalMax();
            return Vec3f(v.x(), v.y(), v.z());
        })
        .def_property_readonly("world_min", [](const VoxelCollisionShape& shape) {
            const btVector3 v = shape.getWorldMin();
            return Vec3f(v.x(), v.y(), v.z());
        })
        .def_property_readonly("world_max", [](const VoxelCollisionShape& shape) {
            const btVector3 v = shape.getWorldMax();
            return Vec3f(v.x(), v.y(), v.z());
        })
        .def("get_name", &VoxelCollisionShape::getName);

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

    // Один shared_ptr<btCollisionShape> можно назначить всем блокам одного типа.
    m.def("make_box_shape",
          [](float half_x, float half_y, float half_z) {
              auto shape = std::make_shared<btBoxShape>(
                  btVector3(btScalar(half_x), btScalar(half_y), btScalar(half_z)));
              shape->setMargin(btScalar(0.0));
              return shape;
          },
          py::arg("half_x") = 0.5f,
          py::arg("half_y") = 0.5f,
          py::arg("half_z") = 0.5f);

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
