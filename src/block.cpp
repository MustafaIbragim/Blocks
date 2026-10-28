#include "block.h"
#include <stdexcept>


Block::Block() 
    : id(0), opacity(0), light(0), status(0),
      name("air"), inner(nullptr), collision_mesh(nullptr) {
    for (int i = 0; i < 6; ++i) {
        face_textures[i] = 0;
    }
}

Block::Block(uint16_t id, const std::string& name)
    : id(id), opacity(255), light(0), status(0),
      name(name), inner(nullptr), collision_mesh(nullptr) {
    for (int i = 0; i < 6; ++i) {
        face_textures[i] = 0;
    }
}

void Block::set_face_texture(uint8_t face, uint16_t texture_id) {
    if (face < 6) {
        face_textures[face] = texture_id;
    }
}

uint16_t Block::get_face_texture(uint8_t face, uint16_t nbt) const {
    (void)nbt;
    return (face < 6) ? face_textures[face] : 0;
}

void Block::set_inner_mesh(std::shared_ptr<Mesh> mesh) {
    inner = std::move(mesh);
}

std::shared_ptr<Mesh> Block::get_inner_mesh(uint16_t nbt) const {
    (void)nbt;
    return inner;
}

namespace {

// Угол грани куба: мировая позиция, локальный угол (0/1) для света и UV.
struct CubeCorner {
    float px, py, pz;
    int lx, ly, lz;
    float u, v;
    uint8_t light = 0;
};

// Раскладывает грань куба в 4 угла. Порядок и UV совпадают с прежним
// ChunkMesher::add_face, поэтому геометрия не меняется.
bool get_cube_face_corners(uint8_t face, float fx, float fy, float fz,
                           CubeCorner (&corners)[4]) {
    switch (face) {
        case 0: // -X
            corners[0] = {fx,     fy,     fz + 1, 0, 0, 1, 0.0f, 0.0f};
            corners[1] = {fx,     fy + 1, fz + 1, 0, 1, 1, 0.0f, 1.0f};
            corners[2] = {fx,     fy + 1, fz,     0, 1, 0, 1.0f, 1.0f};
            corners[3] = {fx,     fy,     fz,     0, 0, 0, 1.0f, 0.0f};
            return true;
        case 1: // +X
            corners[0] = {fx + 1, fy,     fz,     1, 0, 0, 0.0f, 0.0f};
            corners[1] = {fx + 1, fy + 1, fz,     1, 1, 0, 0.0f, 1.0f};
            corners[2] = {fx + 1, fy + 1, fz + 1, 1, 1, 1, 1.0f, 1.0f};
            corners[3] = {fx + 1, fy,     fz + 1, 1, 0, 1, 1.0f, 0.0f};
            return true;
        case 2: // -Y
            corners[0] = {fx,     fy,     fz,     0, 0, 0, 0.0f, 0.0f};
            corners[1] = {fx + 1, fy,     fz,     1, 0, 0, 1.0f, 0.0f};
            corners[2] = {fx + 1, fy,     fz + 1, 1, 0, 1, 1.0f, 1.0f};
            corners[3] = {fx,     fy,     fz + 1, 0, 0, 1, 0.0f, 1.0f};
            return true;
        case 3: // +Y
            corners[0] = {fx,     fy + 1, fz,     0, 1, 0, 0.0f, 0.0f};
            corners[1] = {fx,     fy + 1, fz + 1, 0, 1, 1, 0.0f, 1.0f};
            corners[2] = {fx + 1, fy + 1, fz + 1, 1, 1, 1, 1.0f, 1.0f};
            corners[3] = {fx + 1, fy + 1, fz,     1, 1, 0, 1.0f, 0.0f};
            return true;
        case 4: // -Z
            corners[0] = {fx,     fy,     fz,     0, 0, 0, 0.0f, 0.0f};
            corners[1] = {fx,     fy + 1, fz,     0, 1, 0, 0.0f, 1.0f};
            corners[2] = {fx + 1, fy + 1, fz,     1, 1, 0, 1.0f, 1.0f};
            corners[3] = {fx + 1, fy,     fz,     1, 0, 0, 1.0f, 0.0f};
            return true;
        case 5: // +Z
            corners[0] = {fx + 1, fy,     fz + 1, 1, 0, 1, 0.0f, 0.0f};
            corners[1] = {fx + 1, fy + 1, fz + 1, 1, 1, 1, 0.0f, 1.0f};
            corners[2] = {fx,     fy + 1, fz + 1, 0, 1, 1, 1.0f, 1.0f};
            corners[3] = {fx,     fy,     fz + 1, 0, 0, 1, 1.0f, 0.0f};
            return true;
        default:
            return false;
    }
}

void append_triangle(BlockMeshData& out,
                     const CubeCorner& a, const CubeCorner& b,
                     const CubeCorner& c, uint16_t texture_id) {
    const CubeCorner* corners[3] = {&a, &b, &c};

    for (const CubeCorner* corner : corners) {
        Vertex vertex;
        vertex.x = corner->px;
        vertex.y = corner->py;
        vertex.z = corner->pz;
        vertex.block_light = corner->light;
        vertex.sky_light = 0;
        vertex.padding = 0;
        out.vertices.push_back(vertex);
    }

    FaceData face{};
    face.u_0 = a.u; face.v_0 = a.v;
    face.u_1 = b.u; face.v_1 = b.v;
    face.u_2 = c.u; face.v_2 = c.v;
    face.layer = static_cast<int32_t>(texture_id);
    face.anim_data = 0;
    out.faces.push_back(face);
}

} // namespace

uint8_t Block::vertex_light_from_cube(
    const std::array<uint8_t, LIGHT_CUBE_SIZE>& light_cube,
    int cx, int cy, int cz) {
    uint32_t total = 0;
    for (int dx = -1; dx <= 0; ++dx) {
        for (int dy = -1; dy <= 0; ++dy) {
            for (int dz = -1; dz <= 0; ++dz) {
                total += light_cube[light_cube_index(cx + dx, cy + dy, cz + dz)];
            }
        }
    }
    return static_cast<uint8_t>(total / 8);
}

BlockMeshData Block::build_cube_geometry(
    const Vec3i& world_pos,
    uint16_t nbt,
    const std::array<BlockNeighbor, NEIGHBOR_COUNT>& neighbors,
    const std::array<uint8_t, LIGHT_CUBE_SIZE>& light_cube) const {
    BlockMeshData out;
    out.layers_are_texture_ids = true;

    const float fx = static_cast<float>(world_pos.x);
    const float fy = static_cast<float>(world_pos.y);
    const float fz = static_cast<float>(world_pos.z);

    for (uint8_t face = 0; face < NEIGHBOR_COUNT; ++face) {
        if (neighbors[face].is_cube) {
            continue;
        }

        CubeCorner corners[4];
        if (!get_cube_face_corners(face, fx, fy, fz, corners)) {
            continue;
        }

        for (CubeCorner& corner : corners) {
            corner.light = vertex_light_from_cube(
                light_cube, corner.lx, corner.ly, corner.lz);
        }

        const uint16_t texture_id = get_face_texture(face, nbt);
        append_triangle(out, corners[0], corners[1], corners[2], texture_id);
        append_triangle(out, corners[0], corners[2], corners[3], texture_id);
    }

    return out;
}

BlockMeshData Block::build_geometry(
    const Vec3i& world_pos,
    uint16_t nbt,
    const std::array<BlockNeighbor, NEIGHBOR_COUNT>& neighbors,
    const std::array<uint8_t, LIGHT_CUBE_SIZE>& light_cube) const {
    if (status & 0x01) {
        return build_cube_geometry(world_pos, nbt, neighbors, light_cube);
    }

    BlockMeshData out;
    std::shared_ptr<Mesh> inner_mesh = get_inner_mesh(nbt);
    if (!inner_mesh || inner_mesh->faces.empty()) {
        return out;
    }

    const size_t triangle_count = inner_mesh->faces.size();
    const bool indexed = inner_mesh->indices.size() == triangle_count * 3;

    // Проверяем индексы заранее, чтобы не получить частичный треугольник.
    for (size_t tri = 0; tri < triangle_count; ++tri) {
        for (size_t corner = 0; corner < 3; ++corner) {
            const uint32_t index = indexed
                ? inner_mesh->indices[tri * 3 + corner]
                : static_cast<uint32_t>(tri * 3 + corner);
            if (index >= inner_mesh->vertices.size()) {
                return out;
            }
        }
    }

    out.layers_are_texture_ids = false;
    out.vertices.reserve(triangle_count * 3);
    out.faces.reserve(triangle_count);

    for (size_t tri = 0; tri < triangle_count; ++tri) {
        for (size_t corner = 0; corner < 3; ++corner) {
            const uint32_t index = indexed
                ? inner_mesh->indices[tri * 3 + corner]
                : static_cast<uint32_t>(tri * 3 + corner);

            Vertex vertex = inner_mesh->vertices[index];
            vertex.x += static_cast<float>(world_pos.x);
            vertex.y += static_cast<float>(world_pos.y);
            vertex.z += static_cast<float>(world_pos.z);
            out.vertices.push_back(vertex);
        }
        out.faces.push_back(inner_mesh->faces[tri]);
    }

    return out;
}

BlockManager::BlockManager() {
    // Общий куб [0..1]^3. AABB не имеет margin, поэтому соседние блоки
    // стыкуются без зазоров и без двойного проникновения.
    default_collision_mesh = CollisionMesh::unit_cube();

    auto air = std::make_shared<Block>(0, "air");
    air->opacity = 0;
    air->clear_collision_mesh();
    blocks.push_back(air);

    // Барьер: полностью прозрачный блок без меша, но с коллизией куба.
    // Не регистрируется в общем списке (id остаётся 0) — используется
    // только как замена воздуха для незагруженных чанков.
    barrier_block = std::make_shared<Block>(0, "barrier");
    barrier_block->opacity = 0;   // прозрачность: не рендерится и не глушит свет
    barrier_block->light = 0;
    for (int i = 0; i < 6; ++i) {
        barrier_block->face_textures[i] = 0;
    }
    barrier_block->inner = nullptr;             // никакого меша
    barrier_block->collision_mesh = default_collision_mesh;  // полноценный куб
}

uint16_t BlockManager::register_block(std::shared_ptr<Block> block) {
    if (!block) {
        throw std::invalid_argument("Cannot register null block");
    }
    if (blocks.size() >= MAX_BLOCKS) {
        throw std::overflow_error(
            "BlockManager: block limit (65536) exceeded");
    }
    
    uint16_t new_id = static_cast<uint16_t>(blocks.size());
    block->id = new_id;
    // Flyweight: по умолчанию все зарегистрированные блоки используют один
    // общий collision-меш-куб. Собственная физическая геометрия задаётся
    // через Block::set_collision_mesh.
    if (!block->collision_mesh) {
        block->collision_mesh = default_collision_mesh;
    }
    blocks.push_back(std::move(block));
    return new_id;
}

std::shared_ptr<Block> BlockManager::get_block(uint16_t id) const {
    if (id < blocks.size() && blocks[id]) {
        return blocks[id];
    }
    return blocks[0]; 
}

const Block* BlockManager::get_block_raw(uint16_t id) const {
    if (id < blocks.size() && blocks[id]) {
        return blocks[id].get();
    }
    return blocks.empty() ? nullptr : blocks[0].get();
}


