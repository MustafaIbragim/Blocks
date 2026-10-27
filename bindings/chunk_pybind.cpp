#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "chunk.h"

namespace py = pybind11;

PYBIND11_MODULE(chunk_module, m) {
    py::module_::import("vec_module");
    py::module_::import("block_module");
    
    m.doc() = "Chunk module";
    
    py::enum_<ChunkNeighborDir>(m, "ChunkNeighborDir")
        .value("NegX", ChunkNeighborDir::NegX)
        .value("PosX", ChunkNeighborDir::PosX)
        .value("NegZ", ChunkNeighborDir::NegZ)
        .value("PosZ", ChunkNeighborDir::PosZ)
        .export_values();
    
    py::class_<ChunkNugget>(m, "ChunkNugget")
        .def(py::init<Chunk*, uint8_t>(), py::arg("parent") = nullptr, py::arg("y_idx") = 0)
        
        .def("get_block_id", &ChunkNugget::get_block_id, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("set_block_id", &ChunkNugget::set_block_id, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("id"))

        // --- NBT: обычное 16-битное число на клетку ---
        .def("get_nbt", static_cast<uint16_t (ChunkNugget::*)(uint8_t, uint8_t, uint8_t) const>(&ChunkNugget::get_nbt),
             py::arg("x"), py::arg("y"), py::arg("z"))
        .def("set_nbt", static_cast<void (ChunkNugget::*)(uint8_t, uint8_t, uint8_t, uint16_t)>(&ChunkNugget::set_nbt),
             py::arg("x"), py::arg("y"), py::arg("z"), py::arg("nbt"))
        .def("get_nbt_by_index", &ChunkNugget::get_nbt_by_index, py::arg("index"))
        .def("set_nbt_by_index", &ChunkNugget::set_nbt_by_index, py::arg("index"), py::arg("nbt"))
        
        .def("get_block", static_cast<std::shared_ptr<Block> (ChunkNugget::*)(uint8_t, uint8_t, uint8_t, const BlockManager&) const>(&ChunkNugget::get_block),
             py::arg("x"), py::arg("y"), py::arg("z"), py::arg("block_mgr"))
        .def("set_block", static_cast<void (ChunkNugget::*)(uint8_t, uint8_t, uint8_t, std::shared_ptr<Block>)>(&ChunkNugget::set_block),
             py::arg("x"), py::arg("y"), py::arg("z"), py::arg("block"))
        
        .def("get_block", static_cast<std::shared_ptr<Block> (ChunkNugget::*)(const Vec3i&, const BlockManager&) const>(&ChunkNugget::get_block),
             py::arg("pos"), py::arg("block_mgr"))
        .def("set_block", static_cast<void (ChunkNugget::*)(const Vec3i&, std::shared_ptr<Block>)>(&ChunkNugget::set_block),
             py::arg("pos"), py::arg("block"))
        
        .def_static("coords_to_index", &ChunkNugget::coords_to_index, py::arg("x"), py::arg("y"), py::arg("z"))
        .def_static("index_to_coords", &ChunkNugget::index_to_coords, py::arg("index"))
        
        .def("add_tick_block", &ChunkNugget::add_tick_block, py::arg("local_index"))
        .def("remove_tick_block", &ChunkNugget::remove_tick_block, py::arg("local_index"))
        .def("add_slow_tick_block", &ChunkNugget::add_slow_tick_block, py::arg("local_index"))
        .def("remove_slow_tick_block", &ChunkNugget::remove_slow_tick_block, py::arg("local_index"))
        
        .def_property_readonly("block_ids_np", [](ChunkNugget& self) {
            return py::array_t<uint16_t>(
                {ChunkNugget::BLOCKS_COUNT},
                {sizeof(uint16_t)},
                self.block_ids.data(),
                py::capsule(&self)
            );
        })
        .def_property_readonly("nbt_data_np", [](ChunkNugget& self) {
            return py::array_t<uint16_t>(
                {ChunkNugget::BLOCKS_COUNT},
                {sizeof(uint16_t)},
                self.nbt_data.data(),
                py::capsule(&self)
            );
        })
        
        .def_readonly_static("SIZE", &ChunkNugget::SIZE)
        .def_readonly_static("BLOCKS_COUNT", &ChunkNugget::BLOCKS_COUNT)
        
        .def("__repr__", [](const ChunkNugget& cn) {
            return "ChunkNugget(tick_blocks=" + std::to_string(cn.tick_indices.size()) + 
                   ", slow_tick_blocks=" + std::to_string(cn.slow_tick_indices.size()) + ")";
        });
    
    py::class_<Chunk, std::shared_ptr<Chunk>>(m, "Chunk")
        .def(py::init<>())
        .def(py::init<const Vec2i&>(), py::arg("xz") = Vec2i(0, 0))
        .def_readwrite("chunk_xz", &Chunk::chunk_xz)
        
        .def("set_neighbor", &Chunk::set_neighbor, py::arg("dir"), py::arg("neighbor"))
        .def("get_neighbor", &Chunk::get_neighbor, py::arg("dir"), py::return_value_policy::reference)
        .def("clear_neighbors", &Chunk::clear_neighbors)
        
        .def("get_block_id", &Chunk::get_block_id, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("set_block_id", &Chunk::set_block_id, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("id"))
        
        .def("get_block", static_cast<std::shared_ptr<Block> (Chunk::*)(int32_t, int32_t, int32_t, const BlockManager&) const>(&Chunk::get_block),
             py::arg("x"), py::arg("y"), py::arg("z"), py::arg("block_mgr"))
        .def("get_block", static_cast<std::shared_ptr<Block> (Chunk::*)(const Vec3i&, const BlockManager&) const>(&Chunk::get_block),
             py::arg("pos"), py::arg("block_mgr"))
        .def("get_block_fast", &Chunk::get_block_fast, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("block_mgr"))
        
        .def("set_block", static_cast<void (Chunk::*)(uint8_t, uint16_t, uint8_t, std::shared_ptr<Block>)>(&Chunk::set_block),
             py::arg("x"), py::arg("y"), py::arg("z"), py::arg("block"))
        .def("set_block", static_cast<void (Chunk::*)(const Vec3i&, std::shared_ptr<Block>)>(&Chunk::set_block),
             py::arg("pos"), py::arg("block"))

        // --- NBT ---
        .def("get_nbt", static_cast<uint16_t (Chunk::*)(uint8_t, uint16_t, uint8_t) const>(&Chunk::get_nbt),
             py::arg("x"), py::arg("y"), py::arg("z"))
        .def("set_nbt", static_cast<void (Chunk::*)(uint8_t, uint16_t, uint8_t, uint16_t)>(&Chunk::set_nbt),
             py::arg("x"), py::arg("y"), py::arg("z"), py::arg("nbt"))
        .def("get_nbt", static_cast<uint16_t (Chunk::*)(const Vec3i&) const>(&Chunk::get_nbt),
             py::arg("pos"))
        .def("set_nbt", static_cast<void (Chunk::*)(const Vec3i&, uint16_t)>(&Chunk::set_nbt),
             py::arg("pos"), py::arg("nbt"))
        
        .def("get_opacity", &Chunk::get_opacity, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("set_opacity", &Chunk::set_opacity, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("value"))
        
        .def("get_light", &Chunk::get_light, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("set_light", &Chunk::set_light, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("value"))
        
        .def("is_light_dirty", &Chunk::is_light_dirty, py::arg("global_index"))
        .def("mark_light_dirty", &Chunk::mark_light_dirty, py::arg("global_index"), py::arg("state"))
        .def("clear_all_light_dirty", &Chunk::clear_all_light_dirty)
        
        .def("is_standard_cube", &Chunk::is_standard_cube, py::arg("global_index"))
        .def("set_standard_cube", &Chunk::set_standard_cube, py::arg("global_index"), py::arg("state"))
        
        .def_static("coords_to_global_index", &Chunk::coords_to_global_index, py::arg("x"), py::arg("y"), py::arg("z"))
        .def("local_to_world", &Chunk::local_to_world, py::arg("x"), py::arg("y"), py::arg("z"))
        
        .def("tick_all", &Chunk::tick_all, py::arg("tick"), py::arg("block_mgr"))
        .def("slow_tick", &Chunk::slow_tick, py::arg("global_tick"), py::arg("tick_counter"), py::arg("block_mgr"))
        
        .def_property_readonly("nuggets", [](Chunk& c) {
            py::list l;
            for (auto& n : c.nuggets) {
                l.append(py::cast(&n, py::return_value_policy::reference));
            }
            return l;
        })
        
        .def_property_readonly("opacity_np", [](Chunk& self) {
            return py::array_t<uint8_t>({Chunk::VOLUME}, {sizeof(uint8_t)}, self.opacity_data.data(), py::capsule(&self));
        })
        .def_property_readonly("light_np", [](Chunk& self) {
            return py::array_t<uint8_t>({Chunk::VOLUME}, {sizeof(uint8_t)}, self.light_data.data(), py::capsule(&self));
        })
        .def_property_readonly("light_queue_level_np", [](Chunk& self) {
            return py::array_t<uint8_t>({Chunk::VOLUME}, {sizeof(uint8_t)}, self.light_queue_level.data(), py::capsule(&self));
        })
        .def_property_readonly("light_dirty_np", [](Chunk& self) {
            return py::array_t<uint64_t>({Chunk::FLAG_WORDS}, {sizeof(uint64_t)}, self.light_dirty.data(), py::capsule(&self));
        })
        .def_property_readonly("cube_flags_np", [](Chunk& self) {
            return py::array_t<uint64_t>({Chunk::FLAG_WORDS}, {sizeof(uint64_t)}, self.cube_flags.data(), py::capsule(&self));
        })
        
        .def_readonly_static("NUGGETS_COUNT", &Chunk::NUGGETS_COUNT)
        .def_readonly_static("SIZE_XZ", &Chunk::SIZE_XZ)
        .def_readonly_static("HEIGHT", &Chunk::HEIGHT)
        .def_readonly_static("VOLUME", &Chunk::VOLUME)
        .def_readonly_static("FLAG_WORDS", &Chunk::FLAG_WORDS)
        
        .def("__repr__", [](const Chunk& c) {
            return "Chunk(xz=(" + std::to_string(c.chunk_xz.x) + ", " + 
                   std::to_string(c.chunk_xz.y) + "))";
        });
}
