#include "block.h"
#include <stdexcept>
#include <BulletCollision/CollisionShapes/btBoxShape.h>


Block::Block() 
    : id(0), opacity(0), light(0), status(0), name("air"), inner(nullptr), collision(nullptr) {
    for (int i = 0; i < 6; ++i) {
        face_textures[i] = 0;
    }
}

Block::Block(uint16_t id, const std::string& name)
    : id(id), opacity(255), light(0), status(0), name(name), inner(nullptr), collision(nullptr) {
    for (int i = 0; i < 6; ++i) {
        face_textures[i] = 0;
    }
}

void Block::set_face_texture(uint8_t face, uint16_t texture_id) {
    if (face < 6) {
        face_textures[face] = texture_id;
    }
}

uint16_t Block::get_face_texture(uint8_t face) const {
    return (face < 6) ? face_textures[face] : 0;
}

void Block::set_inner_mesh(std::shared_ptr<Mesh> mesh) {
    inner = std::move(mesh);
}

std::shared_ptr<Mesh> Block::get_inner_mesh() const {
    return inner;
}

BlockManager::BlockManager() {
    default_collision_shape = std::make_shared<btBoxShape>(
        btVector3(btScalar(0.5), btScalar(0.5), btScalar(0.5)));
    // Кубы воксельного мира должны совпадать с сеткой без "зазоров" от margin.
    default_collision_shape->setMargin(btScalar(0.0));

    auto air = std::make_shared<Block>(0, "air");
    air->opacity = 0;
    air->clear_collision_shape();
    blocks.push_back(air);
}

uint16_t BlockManager::register_block(std::shared_ptr<Block> block) {
    if (!block) {
        throw std::invalid_argument("Cannot register null block");
    }
    
    uint16_t new_id = static_cast<uint16_t>(blocks.size());
    block->id = new_id;
    // Flyweight: по умолчанию все зарегистрированные блоки используют одну
    // общую форму-куб. Собственная форма задаётся через Block::set_collision_shape.
    if (!block->collision) {
        block->collision = default_collision_shape;
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


