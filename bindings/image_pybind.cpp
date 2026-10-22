#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "image.h"
#include "vec.h"


namespace py = pybind11;

PYBIND11_MODULE(image_module, m) {
    py::module_::import("vec_module");
    
    m.doc() = "Image processing module with NumPy synchronization";
    
    py::class_<Image, std::shared_ptr<Image>>(m, "Image", py::buffer_protocol())
        .def(py::init<uint32_t, uint32_t>(), py::arg("width"), py::arg("height"))
        .def_property_readonly("width", &Image::width)
        .def_property_readonly("height", &Image::height)
        .def_property_readonly("channels", &Image::channels)
        
        .def("get_pixel", 
             static_cast<Vec4u8 (Image::*)(uint32_t, uint32_t) const>(&Image::get_pixel),
             py::arg("x"), py::arg("y"))
        .def("set_pixel",
             static_cast<void (Image::*)(uint32_t, uint32_t, const Vec4u8&)>(&Image::set_pixel),
             py::arg("x"), py::arg("y"), py::arg("color"))
        
        .def("get_pixel",
             static_cast<Vec4u8 (Image::*)(const Vec2i&) const>(&Image::get_pixel),
             py::arg("pos"))
        .def("set_pixel",
             static_cast<void (Image::*)(const Vec2i&, const Vec4u8&)>(&Image::set_pixel),
             py::arg("pos"), py::arg("color"))
        
        .def("get_pixel",
             static_cast<Vec4u8 (Image::*)(const Vec2f&) const>(&Image::get_pixel),
             py::arg("pos"))
        .def("set_pixel",
             static_cast<void (Image::*)(const Vec2f&, const Vec4u8&)>(&Image::set_pixel),
             py::arg("pos"), py::arg("color"))
        
        .def("fill", &Image::fill, py::arg("color"))
        
        .def_buffer([](Image &img) -> py::buffer_info {
            return py::buffer_info(
                img.data(),
                sizeof(uint8_t),
                py::format_descriptor<uint8_t>::format(),
                3,
                { static_cast<py::ssize_t>(img.height()),
                  static_cast<py::ssize_t>(img.width()),
                  static_cast<py::ssize_t>(img.channels()) },
                { static_cast<py::ssize_t>(sizeof(uint8_t) * img.width() * img.channels()),
                  static_cast<py::ssize_t>(sizeof(uint8_t) * img.channels()),
                  static_cast<py::ssize_t>(sizeof(uint8_t)) }
            );
        })
        
        .def("__repr__", [](const Image& img) {
            return "Image(width=" + std::to_string(img.width()) + 
                   ", height=" + std::to_string(img.height()) + 
                   ", channels=" + std::to_string(img.channels()) + ")";
        });
}


