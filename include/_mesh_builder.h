#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <array>
#include "vec.h"
#include "block.h"
#include "mesh.h"


class MeshBuilder {
public:
    static constexpr size_t BLOCKS_COUNT = 32 * 32 * 32;
    static constexpr size_t NEIGHBORS_COUNT = 26;
    
    static std::vector<std::shared_ptr<Mesh>> build_mesh(
        const std::array<uint16_t, BLOCKS_COUNT>& center,
        const std::array<const std::array<uint16_t, BLOCKS_COUNT>*, NEIGHBORS_COUNT>& neighbors,
        const BlockManager& block_manager,
        const Vec3i& world_pos
    );
    
    static void add_block_mesh(
        const Mesh& block_mesh,
        const Vec3f& offset,
        Mesh& final_mesh
    );
    
    static void add_block_mesh(
        const Mesh& block_mesh,
        float offset_x, float offset_y, float offset_z,
        Mesh& final_mesh
    );

private:
    static void greedy_mesh_axis(
        const std::array<uint16_t, BLOCKS_COUNT>& center,
        const std::array<const std::array<uint16_t, BLOCKS_COUNT>*, NEIGHBORS_COUNT>& neighbors,
        const BlockManager& block_manager,
        int axis,
        int world_x, int world_y, int world_z,
        std::unordered_map<int32_t, std::shared_ptr<Mesh>>& meshes_by_layer
    );
    
    static uint16_t get_neighbor_block(
        int local_x, int local_y, int local_z,
        const std::array<uint16_t, BLOCKS_COUNT>& center,
        const std::array<const std::array<uint16_t, BLOCKS_COUNT>*, NEIGHBORS_COUNT>& neighbors
    );
};


