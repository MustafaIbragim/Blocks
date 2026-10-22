#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "renderer.h"


namespace py = pybind11;

PYBIND11_MODULE(renderer_module, m) {
    py::module_::import("vec_module");
    
    m.doc() = "OpenGL Renderer module";
    
    py::class_<ViewCam>(m, "ViewCam")
        .def(py::init<>())
        .def_readwrite("position", &ViewCam::position)
        .def_readwrite("rotation", &ViewCam::rotation)
        .def_readwrite("fov", &ViewCam::fov)
        .def_readwrite("near_plane", &ViewCam::near_plane)
        .def_readwrite("far_plane", &ViewCam::far_plane)
        .def("__repr__", [](const ViewCam& cam) {
            return "ViewCam(pos=(" + std::to_string(cam.position.x) + ", " + 
                   std::to_string(cam.position.y) + ", " + std::to_string(cam.position.z) + "))";
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


