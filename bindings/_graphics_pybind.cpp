#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "graphics.h"


namespace py = pybind11;

PYBIND11_MODULE(graphics_module, m) {
    py::module_::import("vec_module");
    py::module_::import("texture_module");
    
    m.doc() = "Graphics module with Mat4x4 and TextureAtlas";
    
    py::class_<Mat4x4>(m, "Mat4x4")
        .def(py::init<>())
        .def_static("identity", &Mat4x4::identity)
        .def_static("perspective", &Mat4x4::perspective,
            py::arg("fov"), py::arg("aspect"), py::arg("near"), py::arg("far"))
        .def_static("orthographic", &Mat4x4::orthographic,
            py::arg("left"), py::arg("right"), py::arg("bottom"), py::arg("top"),
            py::arg("near"), py::arg("far"))
        .def_static("look_at", &Mat4x4::look_at,
            py::arg("eye"), py::arg("target"), py::arg("up"))
        .def_static("translation", &Mat4x4::translation, py::arg("pos"))
        .def_static("rotation", &Mat4x4::rotation, py::arg("rot"))
        .def_static("scale", &Mat4x4::scale, py::arg("scl"))
        .def("__mul__", &Mat4x4::operator*)
        .def("get_data", [](const Mat4x4& m) {
            return py::array_t<float>(16, m.get_data());
        })
        .def("__repr__", [](const Mat4x4& m) {
            return "Mat4x4()";
        });
    
    py::class_<TextureAtlas::AtlasEntry>(m, "AtlasEntry")
        .def(py::init<>())
        .def_readwrite("texture_id", &TextureAtlas::AtlasEntry::texture_id)
        .def_readwrite("u0", &TextureAtlas::AtlasEntry::u0)
        .def_readwrite("v0", &TextureAtlas::AtlasEntry::v0)
        .def_readwrite("u1", &TextureAtlas::AtlasEntry::u1)
        .def_readwrite("v1", &TextureAtlas::AtlasEntry::v1)
        .def_readwrite("frame_count", &TextureAtlas::AtlasEntry::frame_count)
        .def_readwrite("frame_width", &TextureAtlas::AtlasEntry::frame_width)
        .def_readwrite("frame_height", &TextureAtlas::AtlasEntry::frame_height)
        .def_readwrite("anim_data", &TextureAtlas::AtlasEntry::anim_data);
    
    py::class_<TextureAtlas>(m, "TextureAtlas")
        .def(py::init<>())
        .def("add_texture", &TextureAtlas::add_texture, py::arg("texture"))
        .def("finalize", &TextureAtlas::finalize)
        .def("get_width", &TextureAtlas::get_width)
        .def("get_height", &TextureAtlas::get_height)
        .def("get_pixel_data", [](const TextureAtlas& atlas) {
            const auto& data = atlas.get_pixel_data();
            return py::array_t<uint8_t>(data.size(), data.data());
        })
        .def("get_entries", &TextureAtlas::get_entries, py::return_value_policy::reference)
        .def("get_entry", &TextureAtlas::get_entry, py::arg("texture_id"),
             py::return_value_policy::reference)
        .def("__repr__", [](const TextureAtlas& atlas) {
            return "TextureAtlas(width=" + std::to_string(atlas.get_width()) +
                   ", height=" + std::to_string(atlas.get_height()) + ")";
        });
}


