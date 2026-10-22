#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "vec.h"
#include "mesh.h"


class Block {
public:
    uint16_t id;
    uint8_t opacity;
    uint8_t light;
    uint8_t status;
    std::string name;
    
    uint16_t face_textures[6]; 
    
    std::shared_ptr<Mesh> inner;
    
    Block();
    Block(uint16_t id, const std::string& name);
    
    virtual void on_place(const Vec3i& pos) const {}
    virtual void on_break(const Vec3i& pos) const {}
    virtual bool on_interact(const Vec3i& pos) const { return false; }
    virtual void on_replace(const Vec3i& pos) const {}
    virtual void on_changed(const Vec3i& pos) const {}
    
    virtual void on_tick(const Vec3i& pos, uint64_t tick) const {}
    virtual void on_slow_tick(const Vec3i& pos, uint64_t tick) const {}
    
    void set_face_texture(uint8_t face, uint16_t texture_id);
    uint16_t get_face_texture(uint8_t face) const;
    
    void set_inner_mesh(std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> get_inner_mesh() const;
    
    inline bool is_solid() const { return opacity == 255; }
    
    virtual ~Block() = default;
};

class BlockManager {
public:
    BlockManager();
    
    uint16_t register_block(std::shared_ptr<Block> block);
    
    std::shared_ptr<Block> get_block(uint16_t id) const;
    
    inline bool has_block(uint16_t id) const {
        return id < blocks.size() && blocks[id] != nullptr;
    }
    
    inline size_t size() const {
        return blocks.size();
    }
    
    inline uint16_t get_air_id() const { return 0; }

private:
    std::vector<std::shared_ptr<Block>> blocks;
};


