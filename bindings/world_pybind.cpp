#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <limits>
#include "world.h"


namespace py = pybind11;

PYBIND11_MODULE(world_module, m) {
    py::module_::import("vec_module");
    py::module_::import("chunk_module");
    py::module_::import("block_module");
    py::module_::import("texture_module");
    py::module_::import("mesh_module");
    py::module_::import("item_module");
    
    m.doc() = "World module";
    
    py::class_<LightEngine>(m, "LightEngine")
        .def("update", &LightEngine::update,
             "Полный цикл: затемнение, затем освещение")
        .def("remove_light", &LightEngine::remove_light, "Только затемнение")
        .def("propagate_light", &LightEngine::propagate_light, "Только освещение")
        .def("remove_sky_light", &LightEngine::remove_sky_light,
             "Только затемнение небесного света")
        .def("propagate_sky_light", &LightEngine::propagate_sky_light,
             "Только распространение небесного света")
        .def("mark_neighborhood", &LightEngine::mark_neighborhood,
             py::arg("chunk"), py::arg("local_pos"))
        .def("mark_chunk_sources", &LightEngine::mark_chunk_sources,
             py::arg("chunk"))
        .def("has_pending", &LightEngine::has_pending)
        .def("has_dark_pending", &LightEngine::has_dark_pending)
        .def("has_light_pending", &LightEngine::has_light_pending)
        .def("has_sky_dark_pending", &LightEngine::has_sky_dark_pending)
        .def("has_sky_light_pending", &LightEngine::has_sky_light_pending)
        .def("clear_queues", &LightEngine::clear_queues);
    
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
        .def("get_block", static_cast<std::shared_ptr<Block> (World::*)(const Vec3i&) const>(&World::get_block),
             py::arg("world_pos"))
        .def("get_block_with_nbt",
             [](const World& self, const Vec3i& pos) {
                 uint16_t nbt = 0;
                 std::shared_ptr<Block> block = self.get_block(pos, nbt);
                 return py::make_tuple(block, nbt);
             },
             py::arg("world_pos"),
             "Возвращает (block, nbt) для указанной клетки")
        .def("set_block", &World::set_block, py::arg("world_pos"), py::arg("block"))
        .def("remove_block", &World::remove_block, py::arg("world_pos"))
        .def("get_block_id", &World::get_block_id, py::arg("world_pos"),
             "16-битный id обычного блока из BlockManager")

        // NBT: 16-битное число, интерпретируемое типом блока
        .def("get_nbt", &World::get_nbt, py::arg("world_pos"))
        .def("set_nbt", &World::set_nbt, py::arg("world_pos"), py::arg("nbt"))

        // Items в мире (временно на уровне World)
        .def("spawn_item_entity", &World::spawn_item_entity,
             py::arg("stack"), py::arg("position"),
             py::arg("velocity") = Vec3f::zero())
        .def("remove_item_entity", &World::remove_item_entity, py::arg("entity_id"))
        .def("clear_item_entities", &World::clear_item_entities)
        .def("get_item_entity", &World::get_item_entity, py::arg("entity_id"))
        .def("get_item_entities", &World::get_item_entities,
             py::return_value_policy::reference_internal)
        .def("item_entity_count", &World::item_entity_count)
        .def("pickup_item_entity", &World::pickup_item_entity,
             py::arg("entity_id"), py::arg("inventory"),
             py::arg("preferred_slot") = std::numeric_limits<uint32_t>::max())
        .def("pickup_nearest_item", &World::pickup_nearest_item,
             py::arg("position"), py::arg("radius"), py::arg("inventory"),
             py::arg("preferred_slot") = std::numeric_limits<uint32_t>::max())
        .def("update_items", &World::update_items, py::arg("delta_time"))
        
        // Освещение и тактовое обновление мира
        .def("get_light_engine", &World::get_light_engine,
             py::return_value_policy::reference_internal)
        .def("update_lighting", &World::update_lighting)
        .def("update", &World::update, py::arg("delta_time"),
             "Обновляет мир не чаще 20 Гц: свет, затем dirty-меши")
        
        // Управление мешами
        .def("rebuild_chunk_mesh", &World::rebuild_chunk_mesh, py::arg("chunk_pos"))
        .def("rebuild_dirty_meshes", &World::rebuild_dirty_meshes)
        .def("set_mesh_update_callback", &World::set_mesh_update_callback,
             py::arg("callback"),
             "Callback(chunk_pos, mesh), вызывается после пересборки меша")
        .def("get_chunk_mesh", &World::get_chunk_mesh, py::arg("chunk_pos"))
        .def("has_chunk_mesh", &World::has_chunk_mesh, py::arg("chunk_pos"))
        .def("mark_chunk_dirty", &World::mark_chunk_dirty, py::arg("chunk_pos"))
        .def("is_chunk_dirty", &World::is_chunk_dirty, py::arg("chunk_pos"))
        
        // Тики
        .def("tick_all", &World::tick_all, py::arg("tick"))
        .def("slow_tick", &World::slow_tick, py::arg("global_tick"),
             py::arg("tick_counter") = 0,
             "Обновляет следующие 32 блока во всех загруженных чанках")
        .def("reshuffle_slow_tick_sequence",
             &World::reshuffle_slow_tick_sequence,
             "Пересобирает общую перестановку индексов и начинает цикл заново")
        .def_property("slow_tick_cursor", &World::get_slow_tick_cursor,
                      &World::set_slow_tick_cursor,
                      "Текущая позиция в slow_tick_sequence_np")
        .def_property_readonly("slow_tick_sequence_np", [](World& self) {
            return py::array_t<uint32_t>(
                {World::SLOW_TICK_BLOCK_COUNT},
                {sizeof(uint32_t)},
                self.get_slow_tick_sequence().data(),
                py::capsule(&self));
        }, "Перестановка всех локальных индексов блоков чанка")
        .def_readonly_static("SLOW_TICK_BLOCK_COUNT",
                             &World::SLOW_TICK_BLOCK_COUNT)
        .def_readonly_static("SLOW_TICK_BATCH_SIZE",
                             &World::SLOW_TICK_BATCH_SIZE)
        
        // Доступ к менеджерам
        .def("get_tex_blocks_mgr", &World::get_tex_blocks_mgr, py::return_value_policy::reference)
        .def("get_tex_mgr", &World::get_tex_mgr, py::return_value_policy::reference)
        
        .def("__repr__", [](const World& w) {
            return "World()";
        });
}


