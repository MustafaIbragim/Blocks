#include <memory>

#include <pybind11/pybind11.h>

#include <btBulletDynamicsCommon.h>

#include "vec.h"


namespace py = pybind11;

PYBIND11_MODULE(bullet_module, m) {
    py::module_::import("vec_module");

    m.doc() = "Shared Bullet collision-shape types for block and voxel shapes";

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
}
