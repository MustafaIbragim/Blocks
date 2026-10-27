#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include "collision.h"
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

    // Данные для физики. Mesh выше — только графика; collision_mesh —
    // отдельный набор AABB, который читает физический движок. Геометрия
    // может быть построена из того же меша (CollisionMesh::from_mesh),
    // но назначение у этих данных разное.
    // Один shared_ptr переиспользуется всеми блоками типа (flyweight).
    // nullptr/пустой меш означает, что блок не участвует в коллизиях.
    std::shared_ptr<CollisionMesh> collision_mesh;
    
    Block();
    Block(uint16_t id, const std::string& name);

    void set_collision_mesh(std::shared_ptr<CollisionMesh> mesh) {
        collision_mesh = std::move(mesh);
    }

    void set_collision_box(const Vec3f& min, const Vec3f& max) {
        collision_mesh = CollisionMesh::box(min, max);
    }

    // NBT передаётся в аксессоры, чтобы наследники могли менять
    // коллизию в зависимости от состояния конкретной клетки.
    virtual std::shared_ptr<CollisionMesh> get_collision_mesh(
        uint16_t nbt = 0) const {
        (void)nbt;
        return collision_mesh;
    }

    void clear_collision_mesh() {
        collision_mesh.reset();
    }

    virtual bool has_collision(uint16_t nbt = 0) const {
        std::shared_ptr<CollisionMesh> mesh = get_collision_mesh(nbt);
        return mesh && mesh->physics_used() && !mesh->empty();
    }

    // Построить collision-меш из визуального меша блока. Это основной
    // сценарий задачи 6: геометрия остаётся той же, но физика использует
    // отдельный экземпляр CollisionMesh.
    void build_collision_mesh_from_inner() {
        if (inner) {
            collision_mesh = CollisionMesh::from_mesh(*inner);
        }
    }
    
    // Колбэки получают NBT и возвращают, возможно изменённое, значение.
    // Возврат по значению удобен и для C++, и для Python-переопределений.
    virtual uint16_t on_place(const Vec3i& pos, uint16_t nbt) const {
        (void)pos;
        return nbt;
    }
    virtual uint16_t on_break(const Vec3i& pos, uint16_t nbt) const {
        (void)pos;
        return nbt;
    }
    virtual bool on_interact(const Vec3i& pos, uint16_t nbt) const {
        (void)pos;
        (void)nbt;
        return false;
    }
    virtual uint16_t on_replace(const Vec3i& pos, uint16_t nbt) const {
        (void)pos;
        return nbt;
    }
    virtual uint16_t on_changed(const Vec3i& pos, uint16_t nbt) const {
        (void)pos;
        return nbt;
    }
    
    virtual uint16_t on_tick(const Vec3i& pos, uint64_t tick,
                             uint16_t nbt) const {
        (void)pos;
        (void)tick;
        return nbt;
    }
    virtual uint16_t on_slow_tick(const Vec3i& pos, uint64_t tick,
                                  uint16_t nbt) const {
        (void)pos;
        (void)tick;
        return nbt;
    }
    
    void set_face_texture(uint8_t face, uint16_t texture_id);
    virtual uint16_t get_face_texture(uint8_t face, uint16_t nbt = 0) const;
    
    void set_inner_mesh(std::shared_ptr<Mesh> mesh);
    virtual std::shared_ptr<Mesh> get_inner_mesh(uint16_t nbt = 0) const;
    
    virtual bool is_solid(uint16_t nbt = 0) const {
        (void)nbt;
        return opacity == 255;
    }
    
    virtual ~Block() = default;
};

class BlockManager {
public:
    // Полный 16-битный диапазон id доступен обычным блокам. Уникальные
    // блоки (когда появятся) — это наследники Block с обычным id, поэтому
    // старший бит больше не резервируется.
    static constexpr uint32_t MAX_BLOCKS = 1u << 16;

    BlockManager();
    
    uint16_t register_block(std::shared_ptr<Block> block);
    
    std::shared_ptr<Block> get_block(uint16_t id) const;

    // Не владеющий указатель без копирования shared_ptr — для горячих
    // путей (освещение, мешер, коллизии). Для воздуха и неизвестных id
    // возвращает указатель на воздух.
    const Block* get_block_raw(uint16_t id) const;

    // Барьерный блок: полностью прозрачный (не рендерится и не блокирует
    // свет) без меша, но с коллизией полноценного блока. Используется для
    // незагруженных чанков, чтобы рейкаст и физика не «вылетали» за пределы
    // загруженной части мира.
    std::shared_ptr<Block> get_barrier_block() const {
        return barrier_block;
    }

    // Общий collision-меш-куб для блоков, у которых не задана собственная
    // физическая геометрия.
    std::shared_ptr<CollisionMesh> get_default_collision_mesh() const {
        return default_collision_mesh;
    }

    // Эмиссия блока без создания shared_ptr — для горячих циклов освещения.
    inline uint8_t get_light(uint16_t id) const {
        return (id < blocks.size() && blocks[id]) ? blocks[id]->light : 0;
    }
    
    inline bool has_block(uint16_t id) const {
        return id < blocks.size() && blocks[id] != nullptr;
    }
    
    inline size_t size() const {
        return blocks.size();
    }
    
    inline uint16_t get_air_id() const { return 0; }

private:
    std::vector<std::shared_ptr<Block>> blocks;
    std::shared_ptr<CollisionMesh> default_collision_mesh;
    std::shared_ptr<Block> barrier_block;
};
