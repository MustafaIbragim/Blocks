#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "renderer.h"


namespace py = pybind11;

PYBIND11_MODULE(renderer_module, m) {
    py::module_::import("vec_module");
    py::module_::import("mesh_module");
    py::module_ view_cam = py::module_::import("view_cam_module");
    
    m.doc() = "OpenGL Renderer module";

    // ViewCam живёт в отдельном модуле; оставляем алиас для совместимости.
    m.attr("ViewCam") = view_cam.attr("ViewCam");
    
    py::class_<Sprite>(m, "Sprite")
        .def(py::init<>())
        .def(py::init<const Vec3f&, float, uint32_t>(),
             py::arg("position"), py::arg("size") = 0.5f,
             py::arg("texture_id") = 0)
        .def_readwrite("position", &Sprite::position)
        .def_readwrite("size", &Sprite::size)
        .def_readwrite("texture_id", &Sprite::texture_id)
        .def("__repr__", [](const Sprite& sprite) {
            return "Sprite(texture_id=" + std::to_string(sprite.texture_id) +
                   ", size=" + std::to_string(sprite.size) + ")";
        });

    py::class_<MeshInstance>(m, "MeshInstance")
        .def(py::init<>())
        .def(py::init<std::shared_ptr<Mesh>, const Vec3f&, const Vec3f&,
                      uint32_t>(),
             py::arg("mesh"), py::arg("position"),
             py::arg("scale") = Vec3f::one(),
             py::arg("texture_id") = 0)
        .def_readwrite("mesh", &MeshInstance::mesh)
        .def_readwrite("position", &MeshInstance::position)
        .def_readwrite("scale", &MeshInstance::scale)
        .def_readwrite("texture_id", &MeshInstance::texture_id)
        .def("__repr__", [](const MeshInstance& instance) {
            return "MeshInstance(texture_id=" +
                   std::to_string(instance.texture_id) + ")";
        });

    py::class_<Renderer>(m, "Renderer")
        .def(py::init<>())
        .def("init", &Renderer::init, py::arg("width"), py::arg("height"), py::arg("title"))
        .def("cleanup", &Renderer::cleanup)
        .def("upload_block_textures", &Renderer::upload_block_textures, py::arg("tex_blocks_mgr"))
        .def("upload_texture", &Renderer::upload_texture, py::arg("texture_id"), py::arg("img"))
        .def("remove_texture", &Renderer::remove_texture, py::arg("texture_id"))
        .def("upload_mesh", &Renderer::upload_mesh, py::arg("mesh_id"), py::arg("mesh"))
        .def("remove_mesh", &Renderer::remove_mesh, py::arg("mesh_id"))
        .def("render", &Renderer::render, py::arg("current_tick"), py::arg("camera"), py::arg("min_light"))
        .def("render_meshes", &Renderer::render_meshes,
             py::arg("instances"), py::arg("camera"),
             "Рисует 3D-меши с 2D-текстурой (например, выброшенные предметы)")
        .def("render_sprites", &Renderer::render_sprites,
             py::arg("sprites"), py::arg("camera"))
        .def("render_screen_texture", &Renderer::render_screen_texture,
             py::arg("texture_id"), py::arg("x"), py::arg("y"),
             py::arg("w"), py::arg("h"),
             "Рисует 2D-текстуру в пикселях от левого нижнего угла")
        .def("should_close", &Renderer::should_close)
        .def("poll_events", &Renderer::poll_events)
        .def("swap_buffers", &Renderer::swap_buffers)
        .def("get_width", &Renderer::get_width)
        .def("get_height", &Renderer::get_height)
        .def("get_window_handle", [](Renderer& self) -> uintptr_t {
            return reinterpret_cast<uintptr_t>(self.get_window_handle());
        }, "Возвращает дескриптор окна для Input")
        .def("__repr__", [](const Renderer&) {
            return "Renderer()";
        });
}


