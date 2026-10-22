#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "image.h"


class Texture {
public:
    uint32_t id;
    std::string name;
    std::vector<std::shared_ptr<Image>> images;
    uint8_t anim_data;
    
    Texture();
    Texture(const std::string& name);
    Texture(const std::vector<std::shared_ptr<Image>>& images, const std::string& name);
    
    uint8_t get_frame_count_bit() const { return anim_data & 0x0F; }
    uint8_t get_tick_interval_bit() const { return (anim_data >> 4) & 0x0F; }
    
    void set_animation(uint8_t frame_count_bit, uint8_t tick_interval_bit) {
        anim_data = (frame_count_bit & 0x0F) | ((tick_interval_bit & 0x0F) << 4);
    }
    
    const Image& get_current_frame(uint64_t current_tick) const {
        uint8_t tick_interval_bit = get_tick_interval_bit();
        uint8_t frame_count_bit = get_frame_count_bit();
        
        uint32_t frame_index = (current_tick >> tick_interval_bit) & ((1 << frame_count_bit) - 1);
        return *images[frame_index];
    }
};

class TextureManager {
public:
    TextureManager();
    
    int32_t register_texture(std::shared_ptr<Texture> texture);
    
    int32_t get_layer_id(uint32_t texture_id) const;
    
    const Texture* get_texture(int32_t layer_id) const;
    
    const std::vector<std::shared_ptr<Texture>>& get_textures() const;
    
    size_t size() const;
    
    bool has_texture(uint32_t texture_id) const;
    
    std::pair<uint16_t, uint8_t> get_layer_and_anim(uint16_t texture_id) const;

private:
    std::vector<std::shared_ptr<Texture>> textures;
};

class TextureBlocksManager {
public:
    static constexpr uint32_t STANDARD_SIZE = 16;
    
    TextureBlocksManager();
    
    uint16_t register_texture(std::shared_ptr<Texture> texture);
    
    std::pair<uint16_t, uint8_t> get_layer_and_anim(uint16_t texture_id) const;
    
    size_t size() const { return textures.size(); }
    
    uint32_t get_total_layers() const;
    
    const std::vector<std::shared_ptr<Texture>>& get_textures() const { return textures; }

private:
    std::vector<std::shared_ptr<Texture>> textures;
};


