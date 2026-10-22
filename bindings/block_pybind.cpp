#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "block.h"


namespace py = pybind11;

class PyBlock : public Block {
public:
    using Block::Block;
    
    void on_place(const Vec3i& pos) const override {
        PYBIND11_OVERRIDE(void, Block, on_place, pos);
    }
    void on_break(const Vec3i& pos) const override {
        PYBIND11_OVERRIDE(void, Block, on_break, pos);
    }
    bool on_interact(const Vec3i& pos) const override {
        PYBIND11_OVERRIDE(bool, Block, on_interact, pos);
    }
    void on_replace(const Vec3i& pos) const override {
        PYBIND11_OVERRIDE(void, Block, on_replace, pos);
    }
    void on_changed(const Vec3i& pos) const override {
        PYBIND11_OVERRIDE(void, Block, on_changed, pos);
    }
    void on_tick(const Vec3i& pos, uint64_t tick) const override {
        PYBIND11_OVERRIDE(void, Block, on_tick, pos, tick);
    }
    void on_slow_tick(const Vec3i& pos, uint64_t tick) const override {
        PYBIND11_OVERRIDE(void, Block, on_slow_tick, pos, tick);
    }
};


PYBIND11_MODULE(block_module, m) {
    py::module_::import("vec_module");
    py::module_::import("mesh_module");
    
    m.doc() = "Block module";
    
    py::class_<Block, PyBlock, std::shared_ptr<Block>>(m, "Block")
        .def(py::init<>())
        .def(py::init<uint16_t, const std::string&>(), py::arg("id"), py::arg("name"))
        .def_readwrite("id", &Block::id)
        .def_readwrite("opacity", &Block::opacity)
        .def_readwrite("light", &Block::light)
        .def_readwrite("status", &Block::status)
        .def_readwrite("name", &Block::name)
        .def("set_face_texture", &Block::set_face_texture, py::arg("face"), py::arg("texture_id"))
        .def("get_face_texture", &Block::get_face_texture, py::arg("face"))
        .def("set_inner_mesh", &Block::set_inner_mesh, py::arg("mesh"))
        .def("get_inner_mesh", &Block::get_inner_mesh)
        .def("is_solid", &Block::is_solid)
        .def("on_place", &Block::on_place, py::arg("pos"))
        .def("on_break", &Block::on_break, py::arg("pos"))
        .def("on_interact", &Block::on_interact, py::arg("pos"))
        .def("on_replace", &Block::on_replace, py::arg("pos"))
        .def("on_changed", &Block::on_changed, py::arg("pos"))
        .def("on_tick", &Block::on_tick, py::arg("pos"), py::arg("tick"))
        .def("on_slow_tick", &Block::on_slow_tick, py::arg("pos"), py::arg("tick"))
        .def("__repr__", [](const Block& b) {
            return "Block(id=" + std::to_string(b.id) + 
                   ", name='" + b.name + "')";
        });
    
    py::class_<BlockManager, std::shared_ptr<BlockManager>>(m, "BlockManager")
        .def(py::init<>())
        .def("register_block", &BlockManager::register_block, py::arg("block"))
        .def("get_block", &BlockManager::get_block, py::arg("id"))
        .def("has_block", &BlockManager::has_block, py::arg("id"))
        .def("size", &BlockManager::size)
        .def("get_air_id", &BlockManager::get_air_id)
        .def("__repr__", [](const BlockManager& bm) {
            return "BlockManager(size=" + std::to_string(bm.size()) + ")";
        });
}


