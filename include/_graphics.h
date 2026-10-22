#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <cmath>
#include "vec.h"
#include "texture.h"


class Mat4x4 {
public:
    float data[16];
    
    Mat4x4();
    
    static Mat4x4 identity();
    static Mat4x4 perspective(float fov, float aspect, float near, float far);
    static Mat4x4 orthographic(float left, float right, float bottom, float top, float near, float far);
    static Mat4x4 look_at(const Vec3f& eye, const Vec3f& target, const Vec3f& up);
    static Mat4x4 translation(const Vec3f& pos);
    static Mat4x4 rotation(const Vec3r& rot);
    static Mat4x4 scale(const Vec3f& scl);
    
    Mat4x4 operator*(const Mat4x4& other) const;
    
    const float* get_data() const { return data; }
    float* get_data() { return data; }
};


class TextureAtlas {
public:
    struct AtlasEntry {
        uint32_t texture_id;
        float u0, v0, u1, v1;
        int frame_count;
        float frame_width, frame_height;
        uint8_t anim_data;
    };
    
    TextureAtlas();
    
    void add_texture(std::shared_ptr<Texture> texture);
    void finalize();
    
    const std::vector<uint8_t>& get_pixel_data() const { return pixel_data; }
    const std::vector<AtlasEntry>& get_entries() const { return entries; }
    
    uint32_t get_width() const { return width; }
    uint32_t get_height() const { return height; }
    
    const AtlasEntry* get_entry(uint32_t texture_id) const;

private:
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixel_data;
    std::vector<AtlasEntry> entries;
    std::vector<std::shared_ptr<Texture>> textures;
    
    void pack_textures();
};


