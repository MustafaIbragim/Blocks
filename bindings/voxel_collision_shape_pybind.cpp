#include <memory>

#include <pybind11/pybind11.h>

#include "vec.h"
#include "voxel_collision_shape.h"
#include "world.h"


namespace py = pybind11;

PYBIND11_MODULE(voxel_collision_shape_module, m) {
    py::module_::import("vec_module");
    py::module_::import("world_module");
    py::module_::import("bullet_module");

    m.doc() = "Voxel-backed Bullet collision shape";

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
}
