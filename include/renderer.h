#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <utility>
#include <vector>
#include "vec.h"
#include "mesh.h"
#include "world.h"
#include "texture.h"
#include "view_cam.h"


// GPU-формат грани: одна запись на каждый угол треугольника
// (uv этого угла + layer/anim всей грани). Stride = 16 байт.
struct GPUFaceAttr {
    float u, v;
    int32_t layer;
    uint32_t anim_data;
};


// Camera-facing quad с обычной 2D-текстурой. Используется для иконок
// выброшенных предметов, счётчиков количества и будущих простых сущностей.
struct Sprite {
    Vec3f position;
    float size = 0.5f;
    uint32_t texture_id = 0;
};


// Экземпляр 3D-меша с обычной 2D-текстурой. Используется для выброшенных
// предметов: Item::mesh задаёт геометрию, texture_id — иконку предмета.
// Меш должен быть центрирован в начале координат; position — его центр в мире.
struct MeshInstance {
    std::shared_ptr<Mesh> mesh;
    Vec3f position;
    Vec3f scale = Vec3f::one();
    uint32_t texture_id = 0;

    MeshInstance() = default;
    MeshInstance(std::shared_ptr<Mesh> mesh,
                 const Vec3f& position,
                 const Vec3f& scale = Vec3f::one(),
                 uint32_t texture_id = 0)
        : mesh(std::move(mesh)), position(position), scale(scale),
          texture_id(texture_id) {}
};


class Renderer {
public:
    Renderer();
    ~Renderer();
    
    void init(int width, int height, const std::string& title);
    void cleanup();
    
    void upload_block_textures(const TextureBlocksManager& tex_blocks_mgr);
    void upload_texture(uint32_t texture_id, const Image& img);
    void remove_texture(uint32_t texture_id);
    
    void upload_mesh(uint32_t mesh_id, const Mesh& mesh);
    void remove_mesh(uint32_t mesh_id);
    
    void render(uint64_t current_tick, const ViewCam& camera, float min_light);

    // Рисует набор 3D-мешей после основного прохода. Текстуры должны быть
    // загружены заранее через upload_texture(texture_id, image). Меши
    // кэшируются по адресу и могут переиспользоваться между кадрами.
    void render_meshes(const std::vector<MeshInstance>& instances,
                       const ViewCam& camera);

    // Рисует набор спрайтов после основного прохода. Текстуры должны быть
    // загружены заранее через upload_texture(texture_id, image).
    void render_sprites(const std::vector<Sprite>& sprites, const ViewCam& camera);

    // Рисует 2D-текстуру в экранных координатах (пиксели, начало — левый
    // нижний угол). Используется для HUD: иконка предмета и количество.
    void render_screen_texture(uint32_t texture_id, float x, float y,
                               float w, float h);
    
    bool should_close() const;
    void poll_events();
    void swap_buffers();
    
    int get_width() const { return width; }
    int get_height() const { return height; }
    
    void* get_window_handle() const { return window; }

private:
    void* window; // ???
    int width;
    int height;
    
    uint32_t block_shader_program;
    uint32_t entity_shader_program;
    uint32_t block_texture_array_id;
    uint32_t sprite_vao;
    uint32_t sprite_vbo;
    uint32_t hud_vao;
    uint32_t hud_vbo;
    
    struct GPUMesh {
        uint32_t vao, vbo, fbo; // fbo — буфер данных граней (uv/layer/anim)
        uint32_t vertex_count;
    };

    struct GPUItemMesh {
        uint32_t vao = 0;
        uint32_t vbo = 0;
        uint32_t vertex_count = 0;
    };

    std::unordered_map<uint32_t, GPUMesh> block_meshes;
    std::unordered_map<uint32_t, uint32_t> entity_textures;
    std::unordered_map<const Mesh*, GPUItemMesh> item_meshes;
    std::unordered_map<const Mesh*, std::shared_ptr<Mesh>> item_mesh_sources;

    const GPUItemMesh& get_item_mesh(const Mesh& mesh);
    void compile_shaders();
    void setup_opengl_state();
    uint32_t compile_shader(uint32_t type, const std::string& source);
    void compute_camera_matrices(const ViewCam& camera, float view[16],
                                 float proj[16]) const;
};


