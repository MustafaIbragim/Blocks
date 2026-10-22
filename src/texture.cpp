#include "texture.h"


Texture::Texture() : id(0), name(""), anim_data(0) {}

Texture::Texture(const std::string& name) : id(0), name(name), anim_data(0) {}

Texture::Texture(const std::vector<std::shared_ptr<Image>>& images, const std::string& name)
    : id(0), name(name), images(images), anim_data(0) {}

TextureManager::TextureManager() {
    auto default_image = std::make_shared<Image>(256, 256);
    default_image->fill(Vec4u8(127, 127, 127, 255));
    
    auto default_texture = std::make_shared<Texture>();
    default_texture->name = "default";
    default_texture->images.push_back(default_image);
    
    register_texture(default_texture);
}

int32_t TextureManager::register_texture(std::shared_ptr<Texture> texture) {
    if (!texture) {
        return -1;
    }
    
    int32_t layer_id = static_cast<int32_t>(textures.size());
    texture->id = static_cast<uint32_t>(layer_id);
    
    textures.push_back(texture);
    
    return layer_id;
}

int32_t TextureManager::get_layer_id(uint32_t texture_id) const {
    return texture_id < textures.size() ? static_cast<int32_t>(texture_id) : -1;
}

const Texture* TextureManager::get_texture(int32_t layer_id) const {
    if (layer_id >= 0 && layer_id < static_cast<int32_t>(textures.size())) {
        return textures[layer_id].get();
    }
    return nullptr;
}

const std::vector<std::shared_ptr<Texture>>& TextureManager::get_textures() const {
    return textures;
}

size_t TextureManager::size() const {
    return textures.size();
}

bool TextureManager::has_texture(uint32_t texture_id) const {
    return texture_id < textures.size();
}

std::pair<uint16_t, uint8_t> TextureManager::get_layer_and_anim(uint16_t texture_id) const {
    if (texture_id >= textures.size()) {
        return {0, 0};
    }
    const auto& tex = textures[texture_id];
    return {static_cast<uint16_t>(tex->id), tex->anim_data};
}

TextureBlocksManager::TextureBlocksManager() {}

uint16_t TextureBlocksManager::register_texture(std::shared_ptr<Texture> texture) {
    if (!texture) {
        throw std::invalid_argument("Cannot register null texture");
    }
    
    for (const auto& img : texture->images) {
        if (img->width() != STANDARD_SIZE || img->height() != STANDARD_SIZE) {
            throw std::invalid_argument(
                "Block texture must be " + std::to_string(STANDARD_SIZE) + "x" + 
                std::to_string(STANDARD_SIZE) + ", got " + 
                std::to_string(img->width()) + "x" + std::to_string(img->height())
            );
        }
    }
    
    uint16_t new_id = static_cast<uint16_t>(textures.size());
    texture->id = new_id;
    textures.push_back(std::move(texture));
    return new_id;
}

std::pair<uint16_t, uint8_t> TextureBlocksManager::get_layer_and_anim(uint16_t texture_id) const {
    if (texture_id >= textures.size()) {
        return {0, 0};
    }
    const auto& tex = textures[texture_id];
    return {static_cast<uint16_t>(tex->id), tex->anim_data};
}

uint32_t TextureBlocksManager::get_total_layers() const {
    uint32_t total = 0;
    for (const auto& tex : textures) {
        total += static_cast<uint32_t>(tex->images.size());
    }
    return total;
}


