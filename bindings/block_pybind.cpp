#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "block.h"


namespace py = pybind11;

class PyBlock : public Block {
public:
    using Block::Block;
    
    uint16_t on_place(const Vec3i& pos, uint16_t nbt) const override {
        PYBIND11_OVERRIDE(uint16_t, Block, on_place, pos, nbt);
    }
    uint16_t on_break(const Vec3i& pos, uint16_t nbt) const override {
        PYBIND11_OVERRIDE(uint16_t, Block, on_break, pos, nbt);
    }
    bool on_interact(const Vec3i& pos, uint16_t nbt) const override {
        PYBIND11_OVERRIDE(bool, Block, on_interact, pos, nbt);
    }
    uint16_t on_replace(const Vec3i& pos, uint16_t nbt) const override {
        PYBIND11_OVERRIDE(uint16_t, Block, on_replace, pos, nbt);
    }
    uint16_t on_changed(const Vec3i& pos, uint16_t nbt) const override {
        PYBIND11_OVERRIDE(uint16_t, Block, on_changed, pos, nbt);
    }
    uint16_t on_tick(const Vec3i& pos, uint64_t tick,
                     uint16_t nbt) const override {
        PYBIND11_OVERRIDE(uint16_t, Block, on_tick, pos, tick, nbt);
    }
    uint16_t on_slow_tick(const Vec3i& pos, uint64_t tick,
                          uint16_t nbt) const override {
        PYBIND11_OVERRIDE(uint16_t, Block, on_slow_tick, pos, tick, nbt);
    }

    uint16_t get_face_texture(uint8_t face, uint16_t nbt) const override {
        PYBIND11_OVERRIDE(uint16_t, Block, get_face_texture, face, nbt);
    }
    std::shared_ptr<Mesh> get_inner_mesh(uint16_t nbt) const override {
        PYBIND11_OVERRIDE(std::shared_ptr<Mesh>, Block, get_inner_mesh, nbt);
    }
    BlockMeshData build_geometry(
        const Vec3i& world_pos,
        uint16_t nbt,
        const std::array<BlockNeighbor, Block::NEIGHBOR_COUNT>& neighbors,
        const std::array<uint8_t, Block::LIGHT_CUBE_SIZE>& light_cube
    ) const override {
        PYBIND11_OVERRIDE(BlockMeshData, Block, build_geometry,
                          world_pos, nbt, neighbors, light_cube);
    }
    BlockMeshData build_cube_geometry(
        const Vec3i& world_pos,
        uint16_t nbt,
        const std::array<BlockNeighbor, Block::NEIGHBOR_COUNT>& neighbors,
        const std::array<uint8_t, Block::LIGHT_CUBE_SIZE>& light_cube
    ) const override {
        PYBIND11_OVERRIDE(BlockMeshData, Block, build_cube_geometry,
                          world_pos, nbt, neighbors, light_cube);
    }
    bool is_solid(uint16_t nbt) const override {
        PYBIND11_OVERRIDE(bool, Block, is_solid, nbt);
    }
    std::shared_ptr<CollisionMesh> get_collision_mesh(
        uint16_t nbt) const override {
        PYBIND11_OVERRIDE(std::shared_ptr<CollisionMesh>, Block,
                          get_collision_mesh, nbt);
    }
    bool has_collision(uint16_t nbt) const override {
        PYBIND11_OVERRIDE(bool, Block, has_collision, nbt);
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
        .def("get_face_texture", &Block::get_face_texture,
             py::arg("face"), py::arg("nbt") = 0)
        .def("set_inner_mesh", &Block::set_inner_mesh, py::arg("mesh"))
        .def("get_inner_mesh", &Block::get_inner_mesh, py::arg("nbt") = 0)
        .def_readonly_static("NEIGHBOR_COUNT", &Block::NEIGHBOR_COUNT)
        .def_readonly_static("LIGHT_CUBE_SIZE", &Block::LIGHT_CUBE_SIZE)
        .def_static("light_cube_index", &Block::light_cube_index,
                    py::arg("dx"), py::arg("dy"), py::arg("dz"))
        .def("build_geometry", &Block::build_geometry,
             py::arg("world_pos"), py::arg("nbt"),
             py::arg("neighbors"), py::arg("light_cube"),
             "Строит геометрию блока по 6 соседям, позиции и кубу света 3x3x3")
        .def("build_cube_geometry", &Block::build_cube_geometry,
             py::arg("world_pos"), py::arg("nbt"),
             py::arg("neighbors"), py::arg("light_cube"),
             "Готовая геометрия куба с отсечением граней и светом вершин")
        .def_static("vertex_light_from_cube", &Block::vertex_light_from_cube,
                    py::arg("light_cube"), py::arg("cx"), py::arg("cy"), py::arg("cz"),
                    "Средний свет угла куба (cx, cy, cz ∈ {0,1})")
        .def("build_collision_mesh_from_inner",
             &Block::build_collision_mesh_from_inner,
             "Строит физический CollisionMesh из визуального inner-меша")
        .def("get_collision_mesh", &Block::get_collision_mesh,
             py::arg("nbt") = 0)
        .def("set_collision_mesh", &Block::set_collision_mesh,
             py::arg("mesh"))
        .def("clear_collision_mesh", &Block::clear_collision_mesh)
        .def("is_solid", &Block::is_solid, py::arg("nbt") = 0)
        .def("has_collision", &Block::has_collision, py::arg("nbt") = 0)
        .def("on_place", &Block::on_place, py::arg("pos"), py::arg("nbt") = 0)
        .def("on_break", &Block::on_break, py::arg("pos"), py::arg("nbt") = 0)
        .def("on_interact", &Block::on_interact, py::arg("pos"), py::arg("nbt") = 0)
        .def("on_replace", &Block::on_replace, py::arg("pos"), py::arg("nbt") = 0)
        .def("on_changed", &Block::on_changed, py::arg("pos"), py::arg("nbt") = 0)
        .def("on_tick", &Block::on_tick,
             py::arg("pos"), py::arg("tick"), py::arg("nbt") = 0)
        .def("on_slow_tick", &Block::on_slow_tick,
             py::arg("pos"), py::arg("tick"), py::arg("nbt") = 0)
        .def("__repr__", [](const Block& b) {
            return "Block(id=" + std::to_string(b.id) + 
                   ", name='" + b.name + "')";
        });

    py::class_<BlockNeighbor>(m, "BlockNeighbor")
        .def(py::init<>())
        .def(py::init<std::shared_ptr<Block>, uint16_t, bool>(),
             py::arg("block"), py::arg("nbt") = 0, py::arg("is_cube") = false)
        .def_readwrite("block", &BlockNeighbor::block)
        .def_readwrite("nbt", &BlockNeighbor::nbt)
        .def_readwrite("is_cube", &BlockNeighbor::is_cube)
        .def("__repr__", [](const BlockNeighbor& n) {
            return "BlockNeighbor(block=" +
                   (n.block ? ("'" + n.block->name + "'") : std::string("None")) +
                   ", nbt=" + std::to_string(n.nbt) +
                   ", is_cube=" + (n.is_cube ? "True" : "False") + ")";
        });

    py::class_<BlockMeshData>(m, "BlockMeshData")
        .def(py::init<>())
        .def_readwrite("vertices", &BlockMeshData::vertices)
        .def_readwrite("faces", &BlockMeshData::faces)
        .def_readwrite("layers_are_texture_ids",
                       &BlockMeshData::layers_are_texture_ids)
        .def("add_vertex", &BlockMeshData::add_vertex, py::arg("vertex"),
             "Добавляет вершину в конец списка vertices")
        .def("add_face", &BlockMeshData::add_face, py::arg("face"),
             "Добавляет треугольник в конец списка faces")
        .def("clear", &BlockMeshData::clear,
             "Очищает vertices и faces")
        .def("__repr__", [](const BlockMeshData& d) {
            return "BlockMeshData(vertices=" +
                   std::to_string(d.vertices.size()) +
                   ", faces=" + std::to_string(d.faces.size()) +
                   ", layers_are_texture_ids=" +
                   (d.layers_are_texture_ids ? "True" : "False") + ")";
        });
    
    py::class_<BlockManager, std::shared_ptr<BlockManager>>(m, "BlockManager")
        .def(py::init<>())
        .def("register_block", &BlockManager::register_block, py::arg("block"))
        .def("get_block", &BlockManager::get_block, py::arg("id"))
        .def("has_block", &BlockManager::has_block, py::arg("id"))
        .def("size", &BlockManager::size)
        .def("get_air_id", &BlockManager::get_air_id)
        .def_readonly_static("MAX_BLOCKS", &BlockManager::MAX_BLOCKS)
        .def("__repr__", [](const BlockManager& bm) {
            return "BlockManager(size=" + std::to_string(bm.size()) + ")";
        });
}
