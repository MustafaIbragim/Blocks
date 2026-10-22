#include <pybind11/pybind11.h>
#include "input.h"


namespace py = pybind11;

PYBIND11_MODULE(input_module, m) {
    m.doc() = "Input handling module";

    py::class_<Input>(m, "Input")
        .def(py::init<uintptr_t>(), py::arg("window_handle"),
             "Инициализирует ввод, принимая дескриптор окна от Renderer")
        .def("is_key_pressed", &Input::is_key_pressed, py::arg("key"))
        .def("get_mouse_delta", [](Input& self) {
            float dx, dy;
            self.get_mouse_delta(dx, dy);
            return py::make_tuple(dx, dy);
        })
        .def("set_cursor_visible", &Input::set_cursor_visible, py::arg("visible"))
        .def("poll_events", &Input::poll_events)
        .def("__repr__", [](const Input&) {
            return "Input()";
        });
}


