#include <pybind11/pybind11.h>
#include "light_engine.h"
#include "world.h"
#include "chunk.h"
#include "vec.h"


namespace py = pybind11;

PYBIND11_MODULE(light_engine_module, m) {
    py::module_::import("world_module");
    py::module_::import("chunk_module");
    py::module_::import("vec_module");
    
    m.doc() = "Light engine module for voxel world lighting calculations";
    
    py::class_<LightEngine> light_engine_class(m, "LightEngine");
    
    light_engine_class
        .def(py::init<World&>(), py::arg("world"), 
             "Инициализирует движок освещения для указанного мира")
        .def("update", &LightEngine::update, 
             "Полный цикл обновления: сначала затемнение, затем распространение света")
        .def("remove_light", &LightEngine::remove_light, 
             "Только затемнение (снятие старого света)")
        .def("propagate_light", &LightEngine::propagate_light, 
             "Только распространение света от источников")
        .def("remove_sky_light", &LightEngine::remove_sky_light,
             "Только затемнение небесного света")
        .def("propagate_sky_light", &LightEngine::propagate_sky_light,
             "Только распространение небесного света")
        .def("mark_neighborhood", &LightEngine::mark_neighborhood, 
             py::arg("chunk"), py::arg("local_pos"),
             "Помечает блок и его 26 соседей на обновление (консервативный режим)")
        .def("mark_block_changed", &LightEngine::mark_block_changed,
             py::arg("chunk"), py::arg("local_pos"), 
             py::arg("old_opacity"), py::arg("new_opacity"),
             py::arg("old_light"), py::arg("new_light"),
             "Обновляет только те клетки, где свет действительно мог измениться")
        .def("mark_chunk_sources", &LightEngine::mark_chunk_sources,
             py::arg("chunk"),
             "Помечает источники света в чанке на обновление (при загрузке чанка)")
        .def("mark_chunk_border", &LightEngine::mark_chunk_border,
             py::arg("chunk"), py::arg("allow_decrease") = false,
             "Помечает освещённые блоки на границе чанка на пересчёт")
        
        .def("forget_chunk", [](LightEngine& self, Chunk& chunk) {
            self.forget_chunk(&chunk);
        }, py::arg("chunk"),
             "Убирает из очередей ссылки на удалённый чанк")
        .def("has_pending", &LightEngine::has_pending, 
             "Возвращает true, если есть отложенные задачи освещения или затемнения")
        .def("has_dark_pending", &LightEngine::has_dark_pending,
             "Возвращает true, если есть отложенные задачи затемнения")
        .def("has_light_pending", &LightEngine::has_light_pending,
             "Возвращает true, если есть отложенные задачи распространения света")
        .def("has_sky_dark_pending", &LightEngine::has_sky_dark_pending,
             "Возвращает true, если есть отложенные задачи затемнения неба")
        .def("has_sky_light_pending", &LightEngine::has_sky_light_pending,
             "Возвращает true, если есть отложенные задачи распространения неба")
        .def("changed_nugget_count", &LightEngine::changed_nugget_count,
             "Количество ChunkNugget с реально изменившимся светом")
        .def("clear_changed_nuggets", &LightEngine::clear_changed_nuggets,
             "Сбрасывает список затронутых ChunkNugget")
        .def("clear_queues", &LightEngine::clear_queues,
             "Очищает все очереди и сбрасывает флаги dirty");
    
    light_engine_class.attr("LEVEL_COUNT") = LightEngine::LEVEL_COUNT;
    light_engine_class.attr("CENTER") = LightEngine::CENTER;
}


