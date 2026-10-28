#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include "collision.h"
#include "vec.h"
#include "mesh.h"


class Block;

// Описание одного соседа, передаваемое блоку при построении геометрии.
// block — именно ссылка на блок (а не id), чтобы наследники могли
// анализировать его состояние/NBT. is_cube — занимает ли сосед полный
// куб (status & 0x01); такие соседи перекрывают грань.
struct BlockNeighbor {
    std::shared_ptr<Block> block;
    uint16_t nbt = 0;
    bool is_cube = false;

    BlockNeighbor() = default;
    BlockNeighbor(std::shared_ptr<Block> block, uint16_t nbt, bool is_cube)
        : block(std::move(block)), nbt(nbt), is_cube(is_cube) {}
};

// Результат построения геометрии блока. faces[i] — треугольник
// vertices[3*i .. 3*i+2]; ChunkMesher собирает из этого общий меш.
// layers_are_texture_ids = true означает, что FaceData::layer содержит
// id текстуры блока, а не готовый слой массива текстур; мешер сам
// переводит id в слой и подставляет anim_data.
struct BlockMeshData {
    std::vector<Vertex> vertices;
    std::vector<FaceData> faces;
    bool layers_are_texture_ids = true;

    void add_vertex(const Vertex& vertex) { vertices.push_back(vertex); }
    void add_face(const FaceData& face) { faces.push_back(face); }
    void clear() { vertices.clear(); faces.clear(); }
};


class Block {
public:
    // Порядок соседей/граней: -X, +X, -Y, +Y, -Z, +Z.
    static constexpr size_t NEIGHBOR_COUNT = 6;
    // Куб света 3x3x3 вокруг блока.
    static constexpr size_t LIGHT_CUBE_SIZE = 27;

    // Индекс клетки в кубе света по смещению от центра (-1..1).
    static constexpr size_t light_cube_index(int dx, int dy, int dz) {
        return static_cast<size_t>((dx + 1) * 9 + (dy + 1) * 3 + (dz + 1));
    }

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

    // Блок сам строит свою геометрию. На вход приходят:
    //  - глобальная позиция блока и его собственный NBT;
    //  - 6 описаний соседей (BlockNeighbor) в порядке -X,+X,-Y,+Y,-Z,+Z;
    //  - куб света 3x3x3 вокруг блока (27 значений, см. light_cube_index).
    // Возвращаются вершины и грани (по одному треугольнику на FaceData).
    virtual BlockMeshData build_geometry(
        const Vec3i& world_pos,
        uint16_t nbt,
        const std::array<BlockNeighbor, NEIGHBOR_COUNT>& neighbors,
        const std::array<uint8_t, LIGHT_CUBE_SIZE>& light_cube
    ) const;

    // Готовая реализация геометрии куба: отсекает грани по is_cube у
    // соседей, считает свет вершин по кубу 3x3x3 и возвращает UV/текстуру
    // для каждой грани. Наследники могут вызывать её из своего
    // build_geometry или полностью заменить поведение.
    virtual BlockMeshData build_cube_geometry(
        const Vec3i& world_pos,
        uint16_t nbt,
        const std::array<BlockNeighbor, NEIGHBOR_COUNT>& neighbors,
        const std::array<uint8_t, LIGHT_CUBE_SIZE>& light_cube
    ) const;

    // Среднее освещение угла куба (cx, cy, cz ∈ {0,1}) по 8 клеткам
    // куба света, примыкающим к этому углу.
    static uint8_t vertex_light_from_cube(
        const std::array<uint8_t, LIGHT_CUBE_SIZE>& light_cube,
        int cx, int cy, int cz
    );
    
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
