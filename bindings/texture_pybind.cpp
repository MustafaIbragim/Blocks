#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "texture.h"


namespace py = pybind11;

PYBIND11_MODULE(texture_module, m) {
    py::module_::import("image_module");
    
    m.doc() = "Texture and TextureManager module";
    
    py::class_<Texture, std::shared_ptr<Texture>>(m, "Texture")
        .def(py::init<>())
        .def(py::init<const std::string&>(), py::arg("name"))
        .def(py::init<const std::vector<std::shared_ptr<Image>>&, const std::string&>(), 
             py::arg("images"), py::arg("name"))
        .def_readwrite("id", &Texture::id)
        .def_readwrite("name", &Texture::name)
        .def_readwrite("images", &Texture::images)
        .def_readwrite("anim_data", &Texture::anim_data)
        .def("get_frame_count_bit", &Texture::get_frame_count_bit)
        .def("get_tick_interval_bit", &Texture::get_tick_interval_bit)
        .def("set_animation", &Texture::set_animation, 
             py::arg("frame_count_bit"), py::arg("tick_interval_bit"))
        .def("get_current_frame", &Texture::get_current_frame, py::arg("current_tick"),
             py::return_value_policy::reference_internal)
        .def("__repr__", [](const Texture& t) {
            return "Texture(id=" + std::to_string(t.id) + 
                   ", name='" + t.name + 
                   "', images=" + std::to_string(t.images.size()) + ")";
        });
    
    py::class_<TextureManager>(m, "TextureManager")
        .def(py::init<>())
        .def("register_texture", &TextureManager::register_texture, py::arg("texture"))
        .def("get_layer_id", &TextureManager::get_layer_id, py::arg("texture_id"))
        .def("get_texture", &TextureManager::get_texture, py::arg("layer_id"),
             py::return_value_policy::reference_internal)
        .def("get_texture_by_name", &TextureManager::get_texture_by_name,
             py::arg("name"), "Возвращает Texture по имени или None")
        .def("get_layer_id_by_name", &TextureManager::get_layer_id_by_name,
             py::arg("name"), "Возвращает layer id по имени или -1")
        .def("get_textures", &TextureManager::get_textures, py::return_value_policy::reference)
        .def("size", &TextureManager::size)
        .def("has_texture", &TextureManager::has_texture, py::arg("texture_id"))
        .def("get_layer_and_anim", &TextureManager::get_layer_and_anim, py::arg("texture_id"),
             "Возвращает кортеж (base_layer, anim_data) для texture_id")
        .def("__repr__", [](const TextureManager& tm) {
            return "TextureManager(size=" + std::to_string(tm.size()) + ")";
        });
    
    py::class_<TextureBlocksManager>(m, "TextureBlocksManager")
        .def(py::init<>())
        .def("register_texture", &TextureBlocksManager::register_texture, py::arg("texture"))
        .def("get_texture_by_name", &TextureBlocksManager::get_texture_by_name,
             py::arg("name"), "Возвращает Texture по имени или None")
        .def("get_texture_id_by_name", &TextureBlocksManager::get_texture_id_by_name,
             py::arg("name"), "Возвращает texture id по имени или -1")
        .def("get_layer_and_anim", &TextureBlocksManager::get_layer_and_anim, py::arg("texture_id"),
             "Возвращает кортеж (base_layer, anim_data) для texture_id")
        .def("size", &TextureBlocksManager::size)
        .def("get_total_layers", &TextureBlocksManager::get_total_layers)
        .def("__repr__", [](const TextureBlocksManager& tbm) {
            return "TextureBlocksManager(size=" + std::to_string(tbm.size()) + ")";
        });
    
    m.attr("STANDARD_BLOCK_TEXTURE_SIZE") = TextureBlocksManager::STANDARD_SIZE;
}


