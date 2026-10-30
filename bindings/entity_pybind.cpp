#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "entity.h"


namespace py = pybind11;

class PyEntity : public Entity {
public:
    using Entity::Entity;
    void update(uint32_t tick, float gravity) override {
        PYBIND11_OVERRIDE(void, Entity, update, tick, gravity);
    }
};

PYBIND11_MODULE(entity_module, m) {
    py::module_::import("vec_module");
    
    m.doc() = "Entity module";
    
    py::class_<Entity, PyEntity, std::shared_ptr<Entity>>(m, "Entity")
        .def(py::init<>())
        .def(py::init<const Transform&>(), py::arg("transform"))
        .def_readwrite("transform", &Entity::transform)
        .def_readwrite("dynamic", &Entity::dynamic)
        .def_readwrite("speed", &Entity::speed)
        .def_readwrite("drag", &Entity::drag)
        .def_readwrite("gravity", &Entity::gravity)
        .def("update", &Entity::update,
             py::arg("tick"), py::arg("gravity") = 0.0f)
        .def("__repr__", [](const Entity& e) {
            return "Entity(pos=(" + std::to_string(e.transform.position.x) + ", " +
                   std::to_string(e.transform.position.y) + ", " +
                   std::to_string(e.transform.position.z) + "))";
        });
}


