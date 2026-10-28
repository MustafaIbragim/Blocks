#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "chunk_mesher.h"
#include "chunk.h"
#include "block.h"
#include "texture.h"
#include "mesh.h"


namespace py = pybind11;

PYBIND11_MODULE(chunk_mesher_module, m) {
    py::module_::import("chunk_module");
    py::module_::import("block_module");
    py::module_::import("texture_module");
    py::module_::import("mesh_module");
    
    m.doc() = "ChunkMesher module - builds mesh from chunk data";
    
    py::class_<ChunkMesher>(m, "ChunkMesher")
        .def_static("build_mesh", &ChunkMesher::build_mesh,
                    py::arg("chunk"), py::arg("block_mgr"), py::arg("tex_mgr"),
                    "Строит меш чанка: блоки сами возвращают свою геометрию "
                    "(Block::build_geometry)")
        .def("__repr__", [](const ChunkMesher&) {
            return "ChunkMesher(static class)";
        });
}


