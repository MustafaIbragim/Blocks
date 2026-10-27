#include <memory>
#include <string>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "block.h"
#include "collision.h"
#include "mesh.h"
#include "vec.h"
#include "world.h"


namespace py = pybind11;

class PyTriggerCollision : public TriggerCollision {
public:
    using TriggerCollision::TriggerCollision;

    void on_enter(CollisionObject& other) override {
        PYBIND11_OVERRIDE(void, TriggerCollision, on_enter, other);
    }

    void on_exit(CollisionObject& other) override {
        PYBIND11_OVERRIDE(void, TriggerCollision, on_exit, other);
    }
};


PYBIND11_MODULE(collision_module, m) {
    py::module_::import("vec_module");
    py::module_::import("mesh_module");
    py::module_::import("world_module");

    m.doc() = "Axis-aligned collision engine for the voxel world";

    py::enum_<MeshPurpose>(m, "MeshPurpose")
        .value("PHYSICS", MeshPurpose::Physics)
        .value("RENDER_ONLY", MeshPurpose::RenderOnly)
        .export_values();

    py::enum_<CollisionType>(m, "CollisionType")
        .value("STATIC", CollisionType::Static)
        .value("ENTITY", CollisionType::Entity)
        .value("FRAGMENT", CollisionType::Fragment)
        .value("TRIGGER", CollisionType::Trigger)
        .export_values();

    // ------------------------------------------------------------------
    // AABB
    // ------------------------------------------------------------------
    py::class_<Aabb>(m, "Aabb")
        .def(py::init<>())
        .def(py::init<const Vec3f&, const Vec3f&>(),
             py::arg("min"), py::arg("max"))
        .def_static("from_center_half", &Aabb::from_center_half,
                    py::arg("center"), py::arg("half_extents"))
        .def_static("invalid", &Aabb::invalid)
        .def_readwrite("min", &Aabb::min)
        .def_readwrite("max", &Aabb::max)
        .def("valid", &Aabb::valid)
        .def("center", &Aabb::center)
        .def("half_extents", &Aabb::half_extents)
        .def("overlaps", &Aabb::overlaps, py::arg("other"))
        .def("touches", [](const Aabb& self, const Aabb& other,
                           float epsilon) {
                 return self.touches(other, nullptr, epsilon);
             },
             py::arg("other"), py::arg("epsilon") = 1e-4f)
        .def("contact_normal", [](const Aabb& self, const Aabb& other,
                                  float epsilon) {
                 Vec3f normal;
                 self.touches(other, &normal, epsilon);
                 return normal;
             },
             py::arg("other"), py::arg("epsilon") = 1e-4f)
        .def("penetration", &Aabb::penetration, py::arg("other"))
        .def("translated", &Aabb::translated, py::arg("delta"))
        .def("expanded", static_cast<Aabb (Aabb::*)(float) const>(&Aabb::expanded),
             py::arg("amount"))
        .def("expanded",
             static_cast<Aabb (Aabb::*)(const Vec3f&) const>(&Aabb::expanded),
             py::arg("amount"))
        .def("__repr__", [](const Aabb& box) {
            return "Aabb(min=(" + std::to_string(box.min.x) + ", " +
                   std::to_string(box.min.y) + ", " +
                   std::to_string(box.min.z) + "), max=(" +
                   std::to_string(box.max.x) + ", " +
                   std::to_string(box.max.y) + ", " +
                   std::to_string(box.max.z) + "))";
        });

    m.def("aabb_overlap", &aabb_overlap, py::arg("a"), py::arg("b"));
    m.def("aabb_touch", &aabb_touch, py::arg("a"), py::arg("b"),
          py::arg("epsilon") = 1e-4f);
    m.def("aabb_contact_normal", &aabb_contact_normal,
          py::arg("a"), py::arg("b"), py::arg("epsilon") = 1e-4f);
    m.def("aabb_penetration", &aabb_penetration,
          py::arg("moving"), py::arg("other"));

    // ------------------------------------------------------------------
    // CollisionMesh: физические данные, отдельно от графического Mesh
    // ------------------------------------------------------------------
    py::class_<CollisionMesh, std::shared_ptr<CollisionMesh>>(m, "CollisionMesh")
        .def(py::init<>())
        .def(py::init<MeshPurpose>(), py::arg("purpose"))
        .def_readwrite("purpose", &CollisionMesh::purpose)
        .def("physics_used", &CollisionMesh::physics_used)
        .def("empty", &CollisionMesh::empty)
        .def("add_box", static_cast<void (CollisionMesh::*)(const Aabb&)>(
                            &CollisionMesh::add_box),
             py::arg("box"))
        .def("add_box", static_cast<void (CollisionMesh::*)(const Vec3f&,
                                                            const Vec3f&)>(
                            &CollisionMesh::add_box),
             py::arg("min"), py::arg("max"))
        .def("add_box_center", &CollisionMesh::add_box_center,
             py::arg("center"), py::arg("half_extents"))
        .def("add_unit_cube", &CollisionMesh::add_unit_cube)
        .def("clear", &CollisionMesh::clear)
        .def("bounds", &CollisionMesh::bounds)
        .def("world_bounds", &CollisionMesh::world_bounds, py::arg("origin"))
        .def_property_readonly("boxes", [](const CollisionMesh& mesh) {
            return mesh.boxes;
        })
        .def_static("unit_cube", &CollisionMesh::unit_cube)
        .def_static("box",
                    static_cast<std::shared_ptr<CollisionMesh> (*)(
                        const Vec3f&, const Vec3f&, MeshPurpose)>(
                        &CollisionMesh::box),
                    py::arg("min"), py::arg("max"),
                    py::arg("purpose") = MeshPurpose::Physics)
        .def_static("box",
                    static_cast<std::shared_ptr<CollisionMesh> (*)(
                        const Aabb&, MeshPurpose)>(&CollisionMesh::box),
                    py::arg("local_box"),
                    py::arg("purpose") = MeshPurpose::Physics)
        .def_static("box_aabb",
                    static_cast<std::shared_ptr<CollisionMesh> (*)(
                        const Aabb&, MeshPurpose)>(&CollisionMesh::box),
                    py::arg("local_box"),
                    py::arg("purpose") = MeshPurpose::Physics)
        .def_static("from_mesh", &CollisionMesh::from_mesh,
                    py::arg("mesh"),
                    py::arg("purpose") = MeshPurpose::Physics)
        .def("__repr__", [](const CollisionMesh& mesh) {
            return "CollisionMesh(boxes=" + std::to_string(mesh.boxes.size()) +
                   ", physics=" + std::string(mesh.physics_used() ? "true"
                                                                  : "false") +
                   ")";
        });

    // ------------------------------------------------------------------
    // Иерархия объектов
    // ------------------------------------------------------------------
    py::class_<CollisionObject, std::shared_ptr<CollisionObject>>(m, "CollisionObject")
        .def_readonly("id", &CollisionObject::id)
        .def_readwrite("enabled", &CollisionObject::enabled)
        .def("type", &CollisionObject::type)
        .def("get_bounds", &CollisionObject::get_bounds)
        .def("is_static", &CollisionObject::is_static)
        .def("is_entity", &CollisionObject::is_entity)
        .def("is_fragment", &CollisionObject::is_fragment)
        .def("is_trigger", &CollisionObject::is_trigger);

    py::class_<StaticCollision, CollisionObject, std::shared_ptr<StaticCollision>>(
        m, "StaticCollision")
        .def(py::init<>())
        .def(py::init<const Vec3f&, std::shared_ptr<CollisionMesh>>(),
             py::arg("position"), py::arg("mesh"))
        .def_readwrite("position", &StaticCollision::position)
        .def_readwrite("mesh", &StaticCollision::mesh)
        .def("blocks_physics", &StaticCollision::blocks_physics);

    py::class_<DynamicCollision, CollisionObject, std::shared_ptr<DynamicCollision>>(m, "DynamicCollision")
        .def_readwrite("position", &DynamicCollision::position)
        .def_readwrite("half_extents", &DynamicCollision::half_extents)
        .def_readwrite("velocity", &DynamicCollision::velocity)
        .def_readwrite("mass", &DynamicCollision::mass)
        .def_readwrite("collisions_enabled", &DynamicCollision::collisions_enabled)
        .def_readwrite("gravity_enabled", &DynamicCollision::gravity_enabled)
        .def_readonly("grounded", &DynamicCollision::grounded)
        .def_readonly("walled", &DynamicCollision::walled)
        .def("is_grounded",
             [](const DynamicCollision& body) { return body.grounded; })
        .def("is_walled",
             [](const DynamicCollision& body) { return body.walled; })
        .def_readwrite("interpolation_ticks", &DynamicCollision::interpolation_ticks)
        .def("get_bounds", &DynamicCollision::get_bounds)
        .def("get_swept_bounds", &DynamicCollision::get_swept_bounds,
             py::arg("delta"))
        .def("set_position", &DynamicCollision::set_position,
             py::arg("position"), py::arg("reset_history") = true)
        .def("set_half_extents", &DynamicCollision::set_half_extents,
             py::arg("half_extents"))
        .def("record_history", &DynamicCollision::record_history)
        .def("reset_history", &DynamicCollision::reset_history)
        .def("set_interpolation_ticks", &DynamicCollision::set_interpolation_ticks,
             py::arg("value"))
        .def("history_size", &DynamicCollision::history_size)
        .def("get_render_position", &DynamicCollision::get_render_position,
             py::arg("alpha") = 0.0f)
        .def("inverse_mass", &DynamicCollision::inverse_mass);

    py::class_<EntityCollision, DynamicCollision, std::shared_ptr<EntityCollision>>(m, "EntityCollision")
        .def(py::init<>())
        .def(py::init<const Vec3f&, const Vec3f&, float>(),
             py::arg("half_extents"), py::arg("position"),
             py::arg("mass") = 1.0f);

    py::class_<FragmentCollision, DynamicCollision, std::shared_ptr<FragmentCollision>>(m, "FragmentCollision")
        .def(py::init<>())
        .def(py::init<const Vec3f&, const Vec3f&, float>(),
             py::arg("half_extents"), py::arg("position"),
             py::arg("mass") = 0.1f)
        .def_readwrite("source_id", &FragmentCollision::source_id);

    py::class_<TriggerCollision, CollisionObject, PyTriggerCollision,
               std::shared_ptr<TriggerCollision>>(m, "TriggerCollision")
        .def(py::init<>())
        .def(py::init<const Vec3f&, const Vec3f&, const std::string&>(),
             py::arg("position"), py::arg("half_extents"),
             py::arg("name") = "")
        .def_readwrite("position", &TriggerCollision::position)
        .def_readwrite("half_extents", &TriggerCollision::half_extents)
        .def_readwrite("name", &TriggerCollision::name)
        .def_readonly("inside", &TriggerCollision::inside)
        .def_readonly("entered", &TriggerCollision::entered)
        .def_readonly("exited", &TriggerCollision::exited)
        .def("get_bounds", &TriggerCollision::get_bounds)
        .def("contains_id", &TriggerCollision::contains_id, py::arg("object_id"))
        .def("on_enter", &TriggerCollision::on_enter, py::arg("other"))
        .def("on_exit", &TriggerCollision::on_exit, py::arg("other"));

    // ------------------------------------------------------------------
    // CollisionWorld
    // ------------------------------------------------------------------
    py::class_<CollisionWorld>(m, "CollisionWorld")
        .def(py::init<>())
        .def("set_world", &CollisionWorld::set_world, py::arg("world"))
        .def("clear_world", &CollisionWorld::clear_world)
        .def("add_chunk", &CollisionWorld::add_chunk,
             py::arg("world"), py::arg("chunk_pos"))
        .def("remove_chunk", &CollisionWorld::remove_chunk,
             py::arg("chunk_pos"))
        .def("has_chunk", &CollisionWorld::has_chunk, py::arg("chunk_pos"))
        .def("clear_chunks", &CollisionWorld::clear_chunks)

        .def("add_static", &CollisionWorld::add_static, py::arg("object"))
        .def("add_static_box",
             static_cast<uint32_t (CollisionWorld::*)(const Vec3f&,
                                                      const Vec3f&)>(
                 &CollisionWorld::add_static_box),
             py::arg("min"), py::arg("max"))
        .def("add_static_box",
             static_cast<uint32_t (CollisionWorld::*)(const Aabb&)>(
                 &CollisionWorld::add_static_box),
             py::arg("box"))
        .def("add_static_aabb",
             static_cast<uint32_t (CollisionWorld::*)(const Aabb&)>(
                 &CollisionWorld::add_static_box),
             py::arg("box"))
        .def("get_static", &CollisionWorld::get_static,
             py::arg("object_id"), py::return_value_policy::reference_internal)
        .def("remove_static", &CollisionWorld::remove_static,
             py::arg("object_id"))
        .def("clear_statics", &CollisionWorld::clear_statics)

        .def("add_entity",
             static_cast<uint32_t (CollisionWorld::*)(const Vec3f&,
                                                      const Vec3f&, float)>(
                 &CollisionWorld::add_entity),
             py::arg("half_extents"), py::arg("position"),
             py::arg("mass") = 1.0f)
        .def("add_entity",
             static_cast<uint32_t (CollisionWorld::*)(EntityCollision&)>(
                 &CollisionWorld::add_entity),
             py::arg("body"))
        .def("get_entity", &CollisionWorld::get_entity,
             py::arg("object_id"), py::return_value_policy::reference_internal)
        .def("remove_entity", &CollisionWorld::remove_entity,
             py::arg("object_id"))
        .def("clear_entities", &CollisionWorld::clear_entities)

        .def("add_fragment",
             static_cast<uint32_t (CollisionWorld::*)(const Vec3f&,
                                                      const Vec3f&, float)>(
                 &CollisionWorld::add_fragment),
             py::arg("half_extents"), py::arg("position"),
             py::arg("mass") = 0.1f)
        .def("add_fragment",
             static_cast<uint32_t (CollisionWorld::*)(FragmentCollision&)>(
                 &CollisionWorld::add_fragment),
             py::arg("body"))
        .def("get_fragment", &CollisionWorld::get_fragment,
             py::arg("object_id"), py::return_value_policy::reference_internal)
        .def("remove_fragment", &CollisionWorld::remove_fragment,
             py::arg("object_id"))
        .def("clear_fragments", &CollisionWorld::clear_fragments)

        .def("add_trigger", &CollisionWorld::add_trigger, py::arg("object"))
        .def("add_trigger_box", &CollisionWorld::add_trigger_box,
             py::arg("position"), py::arg("half_extents"),
             py::arg("name") = "")
        .def("get_trigger", &CollisionWorld::get_trigger,
             py::arg("object_id"), py::return_value_policy::reference_internal)
        .def("remove_trigger", &CollisionWorld::remove_trigger,
             py::arg("object_id"))
        .def("clear_triggers", &CollisionWorld::clear_triggers)
        .def("clear_all", &CollisionWorld::clear_all)

        .def("set_gravity", &CollisionWorld::set_gravity, py::arg("gravity"))
        .def("get_gravity", &CollisionWorld::get_gravity)
        .def("step", &CollisionWorld::step,
             py::arg("delta_time"),
             py::arg("max_substeps") = 8,
             py::arg("fixed_time_step") = 1.0f / 60.0f)
        .def("step_fragments", &CollisionWorld::step_fragments,
             py::arg("delta_time"))

        .def("get_num_collision_objects",
             &CollisionWorld::get_num_collision_objects)
        .def("get_num_contacts", &CollisionWorld::get_num_contacts)
        .def("get_num_manifolds", &CollisionWorld::get_num_manifolds)

        .def("get_entity_position", &CollisionWorld::get_entity_position,
             py::arg("object_id"))
        .def("get_entity_render_position",
             &CollisionWorld::get_entity_render_position,
             py::arg("object_id"), py::arg("alpha") = 0.0f)
        .def("get_entity_velocity", &CollisionWorld::get_entity_velocity,
             py::arg("object_id"))
        .def("is_entity_grounded", &CollisionWorld::is_entity_grounded,
             py::arg("object_id"))
        .def("is_entity_walled", &CollisionWorld::is_entity_walled,
             py::arg("object_id"))
        .def("set_entity_position", &CollisionWorld::set_entity_position,
             py::arg("object_id"), py::arg("position"));

    // ------------------------------------------------------------------
    // Связь Block <-> CollisionMesh
    // ------------------------------------------------------------------
    m.def("set_block_collision_mesh",
          [](Block& block, std::shared_ptr<CollisionMesh> mesh) {
              block.set_collision_mesh(std::move(mesh));
          },
          py::arg("block"), py::arg("mesh"));
    m.def("get_block_collision_mesh",
          [](const Block& block, uint16_t nbt) {
              return block.get_collision_mesh(nbt);
          },
          py::arg("block"), py::arg("nbt") = 0);
    m.def("clear_block_collision_mesh",
          [](Block& block) { block.clear_collision_mesh(); },
          py::arg("block"));
    m.def("block_has_collision",
          [](const Block& block, uint16_t nbt) {
              return block.has_collision(nbt);
          },
          py::arg("block"), py::arg("nbt") = 0);

    m.def("step_fragment_against_world", &step_fragment_against_world,
          py::arg("world"), py::arg("body"), py::arg("delta_time"),
          py::arg("gravity") = Vec3f(0.0f, -20.0f, 0.0f));
}
