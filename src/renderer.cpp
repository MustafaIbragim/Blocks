#include "renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>


Renderer::Renderer() 
    : window(nullptr)
    , width(0)
    , height(0)
    , block_shader_program(0)
    , entity_shader_program(0)
    , block_texture_array_id(0) 
{}

Renderer::~Renderer() {
    cleanup();
}

void Renderer::init(int width, int height, const std::string& title) {
    this->width = width;
    this->height = height;
    
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    
    glfwMakeContextCurrent(static_cast<GLFWwindow*>(window));
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Failed to initialize GLAD");
    }
    
    setup_opengl_state();
    compile_shaders();
}

void Renderer::cleanup() {
    if (window) {
        // Удаляем меши блоков
        for (auto& [id, mesh] : block_meshes) {
            glDeleteVertexArrays(1, &mesh.vao);
            glDeleteBuffers(1, &mesh.vbo);
            glDeleteBuffers(1, &mesh.fbo);
        }
        block_meshes.clear();
        
        // Удаляем текстуры сущностей
        for (auto& [id, tex_id] : entity_textures) {
            glDeleteTextures(1, &tex_id);
        }
        entity_textures.clear();
        
        // Удаляем array texture блоков
        if (block_texture_array_id) {
            glDeleteTextures(1, &block_texture_array_id);
            block_texture_array_id = 0;
        }
        
        // Удаляем шейдеры
        if (block_shader_program) {
            glDeleteProgram(block_shader_program);
            block_shader_program = 0;
        }
        if (entity_shader_program) {
            glDeleteProgram(entity_shader_program);
            entity_shader_program = 0;
        }
        
        glfwDestroyWindow(static_cast<GLFWwindow*>(window));
        glfwTerminate();
        window = nullptr;
    }
}

void Renderer::setup_opengl_state() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);  // Небесно-голубой фон
}

uint32_t Renderer::compile_shader(uint32_t type, const std::string& source) {
    uint32_t shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, nullptr, info_log);
        throw std::runtime_error(std::string("Shader compilation failed: ") + info_log);
    }
    
    return shader;
}

void Renderer::compile_shaders() {
    // ===== Шейдер для блоков (array texture + анимация) =====
    // Данные грани (uv/layer/anim) подаются отдельным атрибутом из буфера faces:
    // на каждый угол треугольника — своя запись с uv этого угла.
    std::string block_vertex_source = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in uint aBlockLight;
        layout(location = 2) in vec2 aFaceUV;
        layout(location = 3) in int aLayer;
        layout(location = 4) in uint aAnimData;

        out vec2 vUV;
        flat out int vLayer;
        flat out uint vAnimData;
        out float vBlockLight;

        uniform mat4 uView;
        uniform mat4 uProjection;

        void main() {
            gl_Position = uProjection * uView * vec4(aPos, 1.0);
            vUV = aFaceUV;
            vLayer = aLayer;
            vAnimData = aAnimData;
            vBlockLight = float(aBlockLight) / 255.0;
        }
    )";

    std::string block_fragment_source = R"(
        #version 330 core
        in vec2 vUV;
        flat in int vLayer;
        flat in uint vAnimData;
        in float vBlockLight;
        
        out vec4 FragColor;
        
        uniform sampler2DArray uTextureArray;
        uniform uint uCurrentTick;
        uniform float u_min_light;
        
        void main() {
            uint actualLayer = uint(vLayer);
            
            if (vAnimData != 0u) {
                uint frameCountBit = vAnimData & 0x0Fu;
                uint tickIntervalBit = (vAnimData >> 4u) & 0x0Fu;
                uint frameOffset = (uCurrentTick >> tickIntervalBit) & ((1u << frameCountBit) - 1u);
                actualLayer += frameOffset;
            }
            
            vec4 color = texture(uTextureArray, vec3(vUV, actualLayer));
            
            // Применяем минимальное освещение и свет блока
            float final_light = u_min_light + (1.0 - u_min_light) * vBlockLight;
            FragColor = color * vec4(final_light, final_light, final_light, 1.0);
        }
    )";
    
    // ===== Шейдер для сущностей (обычная 2D текстура) =====
    std::string entity_vertex_source = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aUV;
        
        out vec2 vUV;
        
        uniform mat4 uView;
        uniform mat4 uProjection;
        
        void main() {
            gl_Position = uProjection * uView * vec4(aPos, 1.0);
            vUV = aUV;
        }
    )";
    
    std::string entity_fragment_source = R"(
        #version 330 core
        in vec2 vUV;
        
        out vec4 FragColor;
        
        uniform sampler2D uTexture;
        
        void main() {
            FragColor = texture(uTexture, vUV);
        }
    )";
    
    // Компилируем шейдер блоков
    uint32_t bv = compile_shader(GL_VERTEX_SHADER, block_vertex_source);
    uint32_t bf = compile_shader(GL_FRAGMENT_SHADER, block_fragment_source);
    
    block_shader_program = glCreateProgram();
    glAttachShader(block_shader_program, bv);
    glAttachShader(block_shader_program, bf);
    glLinkProgram(block_shader_program);
    
    int success;
    glGetProgramiv(block_shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(block_shader_program, 512, nullptr, info_log);
        throw std::runtime_error(std::string("Block shader linking failed: ") + info_log);
    }
    
    glDeleteShader(bv);
    glDeleteShader(bf);
    
    // Компилируем шейдер сущностей
    uint32_t ev = compile_shader(GL_VERTEX_SHADER, entity_vertex_source);
    uint32_t ef = compile_shader(GL_FRAGMENT_SHADER, entity_fragment_source);
    
    entity_shader_program = glCreateProgram();
    glAttachShader(entity_shader_program, ev);
    glAttachShader(entity_shader_program, ef);
    glLinkProgram(entity_shader_program);
    
    glGetProgramiv(entity_shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(entity_shader_program, 512, nullptr, info_log);
        throw std::runtime_error(std::string("Entity shader linking failed: ") + info_log);
    }
    
    glDeleteShader(ev);
    glDeleteShader(ef);
}

void Renderer::upload_block_textures(const TextureBlocksManager& tex_blocks_mgr) {
    const auto& textures = tex_blocks_mgr.get_textures();
    if (textures.empty()) return;
    
    uint32_t total_layers = tex_blocks_mgr.get_total_layers();
    
    glGenTextures(1, &block_texture_array_id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, block_texture_array_id);
    
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 
                 TextureBlocksManager::STANDARD_SIZE, 
                 TextureBlocksManager::STANDARD_SIZE, 
                 total_layers,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    uint32_t layer_offset = 0;
    for (const auto& tex : textures) {
        for (size_t i = 0; i < tex->images.size(); ++i) {
            const Image& img = *tex->images[i];
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                           0, 0, layer_offset + static_cast<uint32_t>(i),
                           img.width(), img.height(), 1,
                           GL_RGBA, GL_UNSIGNED_BYTE, img.data());
        }
        layer_offset += static_cast<uint32_t>(tex->images.size());
    }
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void Renderer::upload_texture(uint32_t texture_id, const Image& img) {
    // Удаляем старую, если есть
    remove_texture(texture_id);
    
    uint32_t tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width(), img.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img.data());
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    entity_textures[texture_id] = tex_id;
}

void Renderer::remove_texture(uint32_t texture_id) {
    auto it = entity_textures.find(texture_id);
    if (it != entity_textures.end()) {
        glDeleteTextures(1, &it->second);
        entity_textures.erase(it);
    }
}

void Renderer::upload_mesh(uint32_t mesh_id, const Mesh& mesh) {
    remove_mesh(mesh_id);

    if (mesh.indices.size() != mesh.faces.size() * 3) {
        throw std::runtime_error("Renderer::upload_mesh: indices/faces size mismatch");
    }

    // Раскрываем индексированный меш в неиндексированный: на каждый
    // треугольник — свои 3 вершины и 3 записи атрибутов грани.
    //
    // Это принципиально: атрибуты 2..4 (uv/layer/anim) лежат в отдельном
    // буфере и при glDrawElements индексируются тем же индексом вершины,
    // что и позиция. Один общий индекс не может указывать одновременно на
    // разные uv/layer разных граней, поэтому индексированный меш с
    // переиспользованием вершин приводил к «съезжанию» uv и текстур.
    std::vector<Vertex> vertices;
    std::vector<GPUFaceAttr> face_attrs;
    vertices.reserve(mesh.indices.size());
    face_attrs.reserve(mesh.indices.size());

    for (size_t tri = 0; tri < mesh.faces.size(); ++tri) {
        const FaceData& fd = mesh.faces[tri];
        const float u[3] = {fd.u_0, fd.u_1, fd.u_2};
        const float v[3] = {fd.v_0, fd.v_1, fd.v_2};

        for (int corner = 0; corner < 3; ++corner) {
            const uint32_t vertex_index = mesh.indices[tri * 3 + corner];
            vertices.push_back(mesh.vertices[vertex_index]);
            face_attrs.push_back({u[corner], v[corner], fd.layer, fd.anim_data});
        }
    }

    GPUMesh gpu_mesh{};
    gpu_mesh.vertex_count = static_cast<uint32_t>(vertices.size());

    glGenVertexArrays(1, &gpu_mesh.vao);
    glGenBuffers(1, &gpu_mesh.vbo);
    glGenBuffers(1, &gpu_mesh.fbo);

    glBindVertexArray(gpu_mesh.vao);

    // Вербюфер: Vertex = 16 байт (vec3 pos + uint8 block_light + uint8 sky_light + uint16 padding)
    glBindBuffer(GL_ARRAY_BUFFER, gpu_mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribIPointer(1, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)12);
    glEnableVertexAttribArray(1);

    // Буфер граней: на каждый угол каждого треугольника — своя запись
    // (u, v, layer, anim_data). Порядок совпадает с порядком вершин в VBO.
    glBindBuffer(GL_ARRAY_BUFFER, gpu_mesh.fbo);
    glBufferData(GL_ARRAY_BUFFER, face_attrs.size() * sizeof(GPUFaceAttr),
                 face_attrs.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GPUFaceAttr), (void*)0);
    glEnableVertexAttribArray(2);

    glVertexAttribIPointer(3, 1, GL_INT, sizeof(GPUFaceAttr), (void*)8);
    glEnableVertexAttribArray(3);

    glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(GPUFaceAttr), (void*)12);
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);

    block_meshes[mesh_id] = gpu_mesh;
}

void Renderer::remove_mesh(uint32_t mesh_id) {
    auto it = block_meshes.find(mesh_id);
    if (it != block_meshes.end()) {
        glDeleteVertexArrays(1, &it->second.vao);
        glDeleteBuffers(1, &it->second.vbo);
        glDeleteBuffers(1, &it->second.fbo);
        block_meshes.erase(it);
    }
}

void Renderer::render(uint64_t current_tick, const ViewCam& camera, float min_light) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (block_shader_program == 0 || block_meshes.empty()) return;
    
    glUseProgram(block_shader_program);
    
    float yaw = camera.rotation.x;
    float pitch = camera.rotation.y;
    
    Vec3f front;
    front.x = std::cos(yaw) * std::cos(pitch);
    front.y = std::sin(pitch);
    front.z = std::sin(yaw) * std::cos(pitch);
    front = front.normalized();
    
    Vec3f up(0.0f, 1.0f, 0.0f);
    Vec3f target = camera.position + front;
    
    Vec3f z_axis = (camera.position - target).normalized();
    Vec3f x_axis = up.cross(z_axis).normalized();
    Vec3f y_axis = z_axis.cross(x_axis);
    
    float view[16] = {
        x_axis.x, y_axis.x, z_axis.x, 0.0f,
        x_axis.y, y_axis.y, z_axis.y, 0.0f,
        x_axis.z, y_axis.z, z_axis.z, 0.0f,
        -x_axis.dot(camera.position), -y_axis.dot(camera.position), -z_axis.dot(camera.position), 1.0f
    };
    
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float tan_half_fov = std::tan(camera.fov * 0.5f * 3.14159265f / 180.0f);
    
    float proj[16] = {
        1.0f / (aspect * tan_half_fov), 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f / tan_half_fov, 0.0f, 0.0f,
        0.0f, 0.0f, -(camera.far_plane + camera.near_plane) / (camera.far_plane - camera.near_plane), -1.0f,
        0.0f, 0.0f, -(2.0f * camera.far_plane * camera.near_plane) / (camera.far_plane - camera.near_plane), 0.0f
    };
    
    glUniformMatrix4fv(glGetUniformLocation(block_shader_program, "uView"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(block_shader_program, "uProjection"), 1, GL_FALSE, proj);
    glUniform1ui(glGetUniformLocation(block_shader_program, "uCurrentTick"), static_cast<uint32_t>(current_tick));
    glUniform1f(glGetUniformLocation(block_shader_program, "u_min_light"), min_light);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, block_texture_array_id);
    glUniform1i(glGetUniformLocation(block_shader_program, "uTextureArray"), 0);
    
    for (auto& [mesh_id, gpu_mesh] : block_meshes) {
        glBindVertexArray(gpu_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, gpu_mesh.vertex_count);
    }
    
    glBindVertexArray(0);
}

bool Renderer::should_close() const {
    return window ? glfwWindowShouldClose(static_cast<GLFWwindow*>(window)) : true;
}

void Renderer::poll_events() {
    glfwPollEvents();
}

void Renderer::swap_buffers() {
    if (window) {
        glfwSwapBuffers(static_cast<GLFWwindow*>(window));
    }
}


