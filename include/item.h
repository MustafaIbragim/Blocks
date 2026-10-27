#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "collision.h"
#include "entity.h"
#include "image.h"
#include "mesh.h"
#include "vec.h"


class Block;
class Item;
class World;


// Стак предметов. Определён до ItemActionContext, потому что контекст
// хранит стак по значению (действие может его изменить).
struct ItemStack {
    std::shared_ptr<Item> item;
    uint16_t count = 0;

    ItemStack() = default;
    ItemStack(std::shared_ptr<Item> item, uint16_t count = 1);

    bool empty() const { return !item || count == 0; }
    uint16_t max_stack() const;
    bool same_item(const ItemStack& other) const {
        return item && other.item && item.get() == other.item.get();
    }
    uint16_t space() const;

    uint16_t add(uint16_t amount);
    uint16_t remove(uint16_t amount);

    void clear() {
        item.reset();
        count = 0;
    }
};


// Контекст вызова действия предмета.
//
// action_0 — «взаимодействие» (обычно ПКМ);
// action_1 — «установка/использование» (обычно ЛКМ).
//
// Имена намеренно не привязаны к кнопкам: в будущем те же действия могут
// вызываться из UI, модов или автоматики.
struct ItemActionContext {
    World* world = nullptr;
    ItemStack stack;

    // Точка, из которой выполняется действие (обычно глаза игрока).
    Vec3f position;

    // Блок, на который направлено действие, нормаль его грани и NBT
    // конкретной клетки (может влиять на поведение предмета/блока).
    Vec3i block_position;
    Vec3i block_normal;
    uint16_t block_nbt = 0;
    bool has_block = false;
};


class Item {
public:
    std::string name;

    // Изображение предмета: иконка в инвентаре и запасной спрайт
    // выброшенного item-entity. Может быть nullptr, если изображение ещё не
    // задано.
    std::shared_ptr<Image> image;

    // Необязательная 3D-геометрия для выброшенного предмета. Если задана,
    // рендерер рисует её вместо плоского спрайта. На физику, подбор и
    // инвентарь это поле не влияет. Меш должен быть центрирован в начале
    // координат; масштаб задаётся при отрисовке экземпляра.
    std::shared_ptr<Mesh> mesh;

    // Готовый куб с центром в (0, 0, 0). Удобен как простая 3D-модель
    // выброшенного предмета. size — длина ребра в мировых единицах.
    static std::shared_ptr<Mesh> create_cube_mesh(float size = 0.8f);

    // Блок, который предмет устанавливает действием action_1.
    // Может быть nullptr для «неблочных» предметов.
    std::shared_ptr<Block> block;

    // Количество копий в одном стаке хранится как code + 1:
    // 0 -> 1 копия, 255 -> 256 копий. Это ровно uint8-поле.
    uint8_t stack_limit_code;

    Item();
    Item(const std::string& name,
         std::shared_ptr<Image> image = nullptr,
         uint16_t max_stack = 1);
    virtual ~Item() = default;

    uint16_t max_stack() const {
        return static_cast<uint16_t>(stack_limit_code) + 1;
    }

    void set_max_stack(uint16_t value) {
        if (value < 1) value = 1;
        if (value > 256) value = 256;
        stack_limit_code = static_cast<uint8_t>(value - 1);
    }

    void set_stack_limit_code(uint8_t code) { stack_limit_code = code; }

    // Действия по умолчанию. action_0 ничего не делает; action_1 пытается
    // поставить block в соседнюю с целью клетку и уменьшить стак.
    virtual bool action_0(ItemActionContext& context) const;
    virtual bool action_1(ItemActionContext& context) const;
};


// Простой контейнер слотов. Используется и как инвентарь игрока (в тесте),
// и как инвентарь блоков с состоянием (сундуков/печей).
class Inventory {
public:
    explicit Inventory(uint32_t slots = 0);

    uint32_t size() const { return static_cast<uint32_t>(slots.size()); }
    void resize(uint32_t slots);

    bool valid_slot(uint32_t index) const { return index < slots.size(); }

    const ItemStack& get_slot(uint32_t index) const;
    ItemStack& get_slot(uint32_t index);
    void set_slot(uint32_t index, const ItemStack& stack);

    // Кладёт максимум возможного в конкретную ячейку. Ячейка должна быть
    // пустой или содержать такой же предмет. Возвращает остаток.
    uint16_t add_to_slot(uint32_t index, ItemStack& stack);

    // Кладёт стак: сначала в preferred_slot (если задан), затем в
    // существующие стаки того же предмета, затем в пустые ячейки.
    // Возвращает остаток, который не поместился.
    uint16_t add(ItemStack stack,
                 uint32_t preferred_slot = std::numeric_limits<uint32_t>::max());

    // Забирает до count копий из ячейки. Возвращает забранный стак.
    ItemStack take(uint32_t index, uint16_t count = 0);

    bool can_accept(const ItemStack& stack) const;
    uint32_t count_of(const std::shared_ptr<Item>& item) const;
    bool empty() const;
    void clear();

private:
    std::vector<ItemStack> slots;
};


// Выброшенный в мир предмет. Пока коллизии/хранение сущностей в чанках не
// переработаны, эти объекты временно живут на уровне World (см. roadmap).
class ItemEntity : public Entity {
public:
    ItemStack stack;
    Vec3f velocity;
    float pickup_delay = 0.35f;
    bool alive = true;
    uint32_t id = 0;

    // Физика выпавшего предмета: лёгкий фрагмент, который сталкивается
    // только со статичными блоками и игнорирует сущности.
    FragmentCollision collision_body;

    // Позиция для графики. Отстаёт от физической на 2 такта и
    // интерполируется, чтобы предмет не телепортировался между кадрами.
    Vec3f render_position;

    // Копия Item::mesh на один выброшенный предмет: освещение зависит от
    // позиции в мире, поэтому общий меш предмета использовать нельзя.
    // Геометрия копируется из stack.item->mesh, когда предмет появляется.
    std::shared_ptr<Mesh> render_mesh;
    const Mesh* render_mesh_source = nullptr;

    // Смещение центра меша относительно центра физического тела. Куб 0.8
    // стоит на полу, а тело предмета имеет половину высоты 0.2, поэтому
    // визуальный центр выше на 0.2.
    Vec3f mesh_offset = Vec3f(0.0f, 0.2f, 0.0f);

    // Размер запасного спрайта в мировых единицах. Используется только
    // для предметов без Item::mesh; рендерер понимает его как половину
    // стороны camera-facing квада.
    float sprite_size = 0.35f;

    ItemEntity();
    ItemEntity(const ItemStack& stack,
               const Vec3f& position = Vec3f::zero(),
               const Vec3f& velocity = Vec3f::zero());

    // Синхронизирует render_mesh с текущим stack.item->mesh.
    void sync_render_mesh();

    // Пересчитывает свет вершин render_mesh по текущей позиции в мире.
    void refresh_mesh_lighting(const World& world);

    bool can_pickup() const {
        return alive && pickup_delay <= 0.0f && !stack.empty();
    }

    void update(uint32_t tick) override { (void)tick; }
};
