#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "mesh_builder.h"
#include "chunk.h"
#include "block.h"
#include <cstring>


namespace py = pybind11;

PYBIND11_MODULE(mesh_builder_module, m) {
    py::module_::import("vec_module");
    py::module_::import("block_module");
    py::module_::import("mesh_module");
    
    m.doc() = "MeshBuilder module with greedy meshing";
    
    py::class_<MeshBuilder>(m, "MeshBuilder")
        .def_static("build_mesh", [](
            py::array_t<uint16_t> center_np,
            py::list neighbors_list,
            const BlockManager& block_manager,
            const Vec3i& world_pos
        ) {
            auto center_info = center_np.request();
            if (center_info.size != MeshBuilder::BLOCKS_COUNT) {
                throw std::runtime_error("center array must have size " + 
                    std::to_string(MeshBuilder::BLOCKS_COUNT));
            }
            
            std::array<uint16_t, MeshBuilder::BLOCKS_COUNT> center;
            std::memcpy(center.data(), center_info.ptr, 
                       center_info.size * sizeof(uint16_t));
            
            std::array<const std::array<uint16_t, MeshBuilder::BLOCKS_COUNT>*, 
                       MeshBuilder::NEIGHBORS_COUNT> neighbors;
            
            for (size_t i = 0; i < MeshBuilder::NEIGHBORS_COUNT; ++i) {
                if (i < neighbors_list.size() && !neighbors_list[i].is_none()) {
                    py::array_t<uint16_t> neighbor_np = neighbors_list[i].cast<py::array_t<uint16_t>>();
                    auto info = neighbor_np.request();
                    if (info.size != MeshBuilder::BLOCKS_COUNT) {
                        throw std::runtime_error("neighbor array must have size " + 
                            std::to_string(MeshBuilder::BLOCKS_COUNT));
                    }
                    neighbors[i] = reinterpret_cast<const std::array<uint16_t, MeshBuilder::BLOCKS_COUNT>*>(info.ptr);
                } else {
                    neighbors[i] = nullptr;
                }
            }
            
            auto meshes = MeshBuilder::build_mesh(center, neighbors, block_manager, world_pos);
            
            py::list result;
            for (auto& mesh : meshes) {
                py::dict mesh_dict;
                
                auto vertices_np = py::array_t<float>(mesh->vertices.size());
                auto vertices_buf = vertices_np.mutable_unchecked<1>();
                for (size_t i = 0; i < mesh->vertices.size(); ++i) {
                    vertices_buf(i) = mesh->vertices[i];
                }
                
                auto indices_np = py::array_t<uint32_t>(mesh->indices.size());
                auto indices_buf = indices_np.mutable_unchecked<1>();
                for (size_t i = 0; i < mesh->indices.size(); ++i) {
                    indices_buf(i) = mesh->indices[i];
                }
                
                mesh_dict["vertices"] = vertices_np;
                mesh_dict["indices"] = indices_np;
                mesh_dict["layer_id"] = mesh->layer_id;
                
                result.append(mesh_dict);
            }
            
            return result;
        }, py::arg("center"), py::arg("neighbors"), 
           py::arg("block_manager"), py::arg("world_pos"));
}


