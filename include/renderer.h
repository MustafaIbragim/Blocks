#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
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
    
    struct GPUMesh {
        uint32_t vao, vbo, fbo; // fbo — буфер данных граней (uv/layer/anim)
        uint32_t vertex_count;
    };
    std::unordered_map<uint32_t, GPUMesh> block_meshes;
    std::unordered_map<uint32_t, uint32_t> entity_textures;
    
    void compile_shaders();
    void setup_opengl_state();
    uint32_t compile_shader(uint32_t type, const std::string& source);
};


