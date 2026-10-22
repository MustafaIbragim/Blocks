#include "graphics.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>


Mat4x4::Mat4x4() {
    std::memset(data, 0, sizeof(data));
    data[0] = data[5] = data[10] = data[15] = 1.0f;
}

Mat4x4 Mat4x4::identity() {
    return Mat4x4();
}

Mat4x4 Mat4x4::perspective(float fov, float aspect, float near, float far) {
    Mat4x4 result;
    std::memset(result.data, 0, sizeof(result.data));
    
    float f = 1.0f / std::tan(fov / 2.0f);
    result.data[0] = f / aspect;
    result.data[5] = f;
    result.data[10] = (far + near) / (near - far);
    result.data[11] = -1.0f;
    result.data[14] = (2.0f * far * near) / (near - far);
    
    return result;
}

Mat4x4 Mat4x4::orthographic(float left, float right, float bottom, float top, float near, float far) {
    Mat4x4 result;
    std::memset(result.data, 0, sizeof(result.data));
    
    result.data[0] = 2.0f / (right - left);
    result.data[5] = 2.0f / (top - bottom);
    result.data[10] = -2.0f / (far - near);
    result.data[12] = -(right + left) / (right - left);
    result.data[13] = -(top + bottom) / (top - bottom);
    result.data[14] = -(far + near) / (far - near);
    result.data[15] = 1.0f;
    
    return result;
}

Mat4x4 Mat4x4::look_at(const Vec3f& eye, const Vec3f& target, const Vec3f& up) {
    Vec3f f = (target - eye).normalized();
    Vec3f s = f.cross(up).normalized();
    Vec3f u = s.cross(f);
    
    Mat4x4 result;
    result.data[0] = s.x;
    result.data[1] = u.x;
    result.data[2] = -f.x;
    result.data[4] = s.y;
    result.data[5] = u.y;
    result.data[6] = -f.y;
    result.data[8] = s.z;
    result.data[9] = u.z;
    result.data[10] = -f.z;
    result.data[12] = -s.dot(eye);
    result.data[13] = -u.dot(eye);
    result.data[14] = f.dot(eye);
    result.data[15] = 1.0f;
    
    return result;
}

Mat4x4 Mat4x4::translation(const Vec3f& pos) {
    Mat4x4 result;
    result.data[12] = pos.x;
    result.data[13] = pos.y;
    result.data[14] = pos.z;
    return result;
}

Mat4x4 Mat4x4::rotation(const Vec3r& rot) {
    Mat4x4 result;
    
    float cx = std::cos(rot.x), sx = std::sin(rot.x);
    float cy = std::cos(rot.y), sy = std::sin(rot.y);
    float cz = std::cos(rot.z), sz = std::sin(rot.z);
    
    result.data[0] = cz * cy;
    result.data[1] = cz * sy * sx - sz * cx;
    result.data[2] = cz * sy * cx + sz * sx;
    
    result.data[4] = sz * cy;
    result.data[5] = sz * sy * sx + cz * cx;
    result.data[6] = sz * sy * cx - cz * sx;
    
    result.data[8] = -sy;
    result.data[9] = cy * sx;
    result.data[10] = cy * cx;
    
    return result;
}

Mat4x4 Mat4x4::scale(const Vec3f& scl) {
    Mat4x4 result;
    result.data[0] = scl.x;
    result.data[5] = scl.y;
    result.data[10] = scl.z;
    return result;
}

Mat4x4 Mat4x4::operator*(const Mat4x4& other) const {
    Mat4x4 result;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result.data[col * 4 + row] = 
                data[0 * 4 + row] * other.data[col * 4 + 0] +
                data[1 * 4 + row] * other.data[col * 4 + 1] +
                data[2 * 4 + row] * other.data[col * 4 + 2] +
                data[3 * 4 + row] * other.data[col * 4 + 3];
        }
    }
    return result;
}

TextureAtlas::TextureAtlas() = default;

void TextureAtlas::add_texture(std::shared_ptr<Texture> texture) {
    textures.push_back(std::move(texture));
}

void TextureAtlas::finalize() {
    pack_textures();
}

const TextureAtlas::AtlasEntry* TextureAtlas::get_entry(uint32_t texture_id) const {
    for (const auto& entry : entries) {
        if (entry.texture_id == texture_id) {
            return &entry;
        }
    }
    return nullptr;
}

void TextureAtlas::pack_textures() {
    if (textures.empty()) {
        width = height = 0;
        return;
    }
    
    std::unordered_map<uint32_t, std::shared_ptr<Texture>> unique_textures;
    for (auto& tex : textures) {
        unique_textures[tex->id] = tex;
    }
    
    uint32_t max_width = 0;
    uint32_t total_height = 0;
    
    for (auto& pair : unique_textures) {
        auto& tex = pair.second;
        if (tex->images.empty()) continue;
        
        uint32_t frame_width = tex->images[0]->width();
        uint32_t frame_height = tex->images[0]->height();
        int frame_count = tex->images.size();
        
        max_width = std::max(max_width, frame_width * frame_count);
        total_height += frame_height;
    }
    
    width = 1;
    height = 1;
    while (width < max_width) width <<= 1;
    while (height < total_height) height <<= 1;
    
    if (width > 4096) width = 4096;
    if (height > 4096) height = 4096;
    
    pixel_data.resize(width * height * 3, 0);
    entries.clear();
    
    uint32_t current_y = 0;
    
    for (auto& pair : unique_textures) {
        auto& tex = pair.second;
        if (tex->images.empty()) continue;
        
        uint32_t frame_width = tex->images[0]->width();
        uint32_t frame_height = tex->images[0]->height();
        int frame_count = tex->images.size();
        
        float u0 = 0.0f;
        float v0 = static_cast<float>(current_y) / height;
        float u1 = static_cast<float>(frame_width * frame_count) / width;
        float v1 = static_cast<float>(current_y + frame_height) / height;
        
        AtlasEntry entry;
        entry.texture_id = tex->id;
        entry.u0 = u0;
        entry.v0 = v0;
        entry.u1 = u1;
        entry.v1 = v1;
        entry.frame_count = frame_count;
        entry.frame_width = static_cast<float>(frame_width) / width;
        entry.frame_height = static_cast<float>(frame_height) / height;
        entry.anim_data = tex->anim_data;
        
        entries.push_back(entry);
        
        for (int frame = 0; frame < frame_count; ++frame) {
            auto& image = tex->images[frame];
            if (!image) continue;
            
            uint32_t img_width = image->width();
            uint32_t img_height = image->height();
            const uint8_t* img_data = image->data();
            
            for (uint32_t y = 0; y < img_height; ++y) {
                for (uint32_t x = 0; x < img_width; ++x) {
                    uint32_t atlas_x = frame * img_width + x;
                    uint32_t atlas_y = current_y + y;
                    
                    if (atlas_x < width && atlas_y < height) {
                        uint32_t atlas_idx = (atlas_y * width + atlas_x) * 3;
                        uint32_t img_idx = (y * img_width + x) * 3;
                        
                        pixel_data[atlas_idx] = img_data[img_idx];
                        pixel_data[atlas_idx + 1] = img_data[img_idx + 1];
                        pixel_data[atlas_idx + 2] = img_data[img_idx + 2];
                    }
                }
            }
        }
        
        current_y += frame_height;
    }
}


