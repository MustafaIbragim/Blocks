#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "world.h"


namespace py = pybind11;

PYBIND11_MODULE(world_module, m) {
    py::module_::import("vec_module");
    py::module_::import("chunk_module");
    py::module_::import("block_module");
    py::module_::import("texture_module");
    py::module_::import("mesh_module");
    
    m.doc() = "World module";
    
    py::class_<World, std::shared_ptr<World>>(m, "World")
        .def(py::init<BlockManager&, TextureBlocksManager&, TextureManager&>(),
             py::arg("block_mgr"), py::arg("tex_blocks_mgr"), py::arg("tex_mgr"))
        
        // Управление чанками
        .def("get_chunk", static_cast<std::shared_ptr<Chunk> (World::*)(const Vec2i&)>(&World::get_chunk),
             py::arg("chunk_pos"))
        .def("set_chunk", &World::set_chunk, py::arg("chunk_pos"), py::arg("chunk"))
        .def("has_chunk", &World::has_chunk, py::arg("chunk_pos"))
        .def("remove_chunk", &World::remove_chunk, py::arg("chunk_pos"))
        
        // Преобразование координат
        .def_static("world_to_chunk_coords", &World::world_to_chunk_coords, py::arg("world_pos"))
        .def_static("world_to_local_coords", &World::world_to_local_coords, py::arg("world_pos"))
        
        // Доступ к блокам
        .def("get_block", &World::get_block, py::arg("world_pos"))
        .def("set_block", &World::set_block, py::arg("world_pos"), py::arg("block"))
        
        // Управление мешами
        .def("rebuild_chunk_mesh", &World::rebuild_chunk_mesh, py::arg("chunk_pos"))
        .def("get_chunk_mesh", &World::get_chunk_mesh, py::arg("chunk_pos"))
        .def("has_chunk_mesh", &World::has_chunk_mesh, py::arg("chunk_pos"))
        .def("mark_chunk_dirty", &World::mark_chunk_dirty, py::arg("chunk_pos"))
        .def("is_chunk_dirty", &World::is_chunk_dirty, py::arg("chunk_pos"))
        
        // Тики
        .def("tick_all", &World::tick_all, py::arg("tick"))
        .def("slow_tick", &World::slow_tick, py::arg("global_tick"), py::arg("tick_counter"))
        
        // Доступ к менеджерам
        .def("get_tex_blocks_mgr", &World::get_tex_blocks_mgr, py::return_value_policy::reference)
        .def("get_tex_mgr", &World::get_tex_mgr, py::return_value_policy::reference)
        
        .def("__repr__", [](const World& w) {
            return "World()";
        });
}


