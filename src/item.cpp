#include "item.h"

#include <algorithm>
#include <stdexcept>

#include "block.h"
#include "world.h"


// ============================================================================
// Item
// ============================================================================

Item::Item()
    : name("item"), image(nullptr), block(nullptr), stack_limit_code(0) {}

Item::Item(const std::string& name,
           std::shared_ptr<Image> image,
           uint16_t max_stack)
    : name(name), image(std::move(image)), block(nullptr), stack_limit_code(0) {
    set_max_stack(max_stack);
}

bool Item::action_0(ItemActionContext& context) const {
    (void)context;
    return false;
}

bool Item::action_1(ItemActionContext& context) const {
    if (!context.world || context.stack.empty()) {
        return false;
    }
    if (!block || !context.has_block) {
        return false;
    }

    const Vec3i place = context.block_position + context.block_normal;
    if (place.y < 0 || place.y >= static_cast<int32_t>(World::CHUNK_HEIGHT)) {
        return false;
    }
    if (!context.world->has_chunk(World::world_to_chunk_coords(place))) {
        return false;
    }

    // Установка разрешена только в воздух. Проверка через World::get_block
    // учитывает все зарегистрированные типы блоков.
    std::shared_ptr<Block> current = context.world->get_block(place);
    if (current && current->id != 0) {
        return false;
    }

    context.world->set_block(place, block);
    context.stack.remove(1);
    return true;
}


// ============================================================================
// ItemStack
// ============================================================================

ItemStack::ItemStack(std::shared_ptr<Item> item, uint16_t count)
    : item(std::move(item)), count(count) {
    if (!this->item || count == 0) {
        this->item.reset();
        this->count = 0;
        return;
    }
    this->count = std::max<uint16_t>(1, std::min(count, this->item->max_stack()));
}

uint16_t ItemStack::max_stack() const {
    return item ? item->max_stack() : 0;
}

uint16_t ItemStack::space() const {
    const uint16_t limit = max_stack();
    return count < limit ? static_cast<uint16_t>(limit - count) : 0;
}

uint16_t ItemStack::add(uint16_t amount) {
    if (!item || amount == 0) {
        return 0;
    }
    uint16_t accepted = std::min(amount, space());
    count = static_cast<uint16_t>(count + accepted);
    return accepted;
}

uint16_t ItemStack::remove(uint16_t amount) {
    uint16_t removed = std::min(amount, count);
    count = static_cast<uint16_t>(count - removed);
    if (count == 0) {
        item.reset();
    }
    return removed;
}


// ============================================================================
// Inventory
// ============================================================================

Inventory::Inventory(uint32_t slot_count) {
    resize(slot_count);
}

void Inventory::resize(uint32_t slot_count) {
    slots.resize(slot_count);
}

const ItemStack& Inventory::get_slot(uint32_t index) const {
    return slots.at(index);
}

ItemStack& Inventory::get_slot(uint32_t index) {
    return slots.at(index);
}

void Inventory::set_slot(uint32_t index, const ItemStack& stack) {
    if (index >= slots.size()) {
        throw std::out_of_range("Inventory::set_slot: invalid slot");
    }
    slots[index] = stack;
}

uint16_t Inventory::add_to_slot(uint32_t index, ItemStack& stack) {
    if (stack.empty() || index >= slots.size()) {
        return stack.count;
    }

    ItemStack& dst = slots[index];
    if (dst.empty()) {
        const uint16_t moved = std::min(stack.count, stack.max_stack());
        dst.item = stack.item;
        dst.count = moved;
        stack.remove(moved);
        return stack.count;
    }

    // Ячейка занята: объединять можно только одинаковые предметы и только
    // до предела стака. Это и есть проверка «соответствия и количества».
    if (!dst.same_item(stack)) {
        return stack.count;
    }

    const uint16_t moved = std::min(stack.count, dst.space());
    dst.add(moved);
    stack.remove(moved);
    return stack.count;
}

uint16_t Inventory::add(ItemStack stack, uint32_t preferred_slot) {
    if (stack.empty()) {
        return 0;
    }

    if (preferred_slot < slots.size()) {
        add_to_slot(preferred_slot, stack);
        if (stack.empty()) {
            return 0;
        }
    }

    // Сначала заполняем уже существующие стаки того же предмета.
    for (uint32_t i = 0; i < slots.size() && !stack.empty(); ++i) {
        if (i == preferred_slot || slots[i].empty() ||
            !slots[i].same_item(stack)) {
            continue;
        }
        const uint16_t moved = std::min(stack.count, slots[i].space());
        slots[i].add(moved);
        stack.remove(moved);
    }

    // Остаток раскладываем в пустые ячейки (стак может не влезть целиком).
    for (uint32_t i = 0; i < slots.size() && !stack.empty(); ++i) {
        if (i == preferred_slot || !slots[i].empty()) {
            continue;
        }
        const uint16_t moved = std::min(stack.count, stack.max_stack());
        slots[i].item = stack.item;
        slots[i].count = moved;
        stack.remove(moved);
    }

    return stack.count;
}

ItemStack Inventory::take(uint32_t index, uint16_t count) {
    if (index >= slots.size() || slots[index].empty()) {
        return ItemStack();
    }

    ItemStack& slot = slots[index];
    const uint16_t amount = (count == 0)
        ? slot.count
        : std::min(count, slot.count);

    ItemStack result{slot.item, amount};
    slot.remove(amount);
    return result;
}

bool Inventory::can_accept(const ItemStack& stack) const {
    if (stack.empty()) {
        return true;
    }

    uint32_t free_space = 0;
    for (const ItemStack& slot : slots) {
        if (slot.empty()) {
            free_space += stack.max_stack();
        } else if (slot.same_item(stack)) {
            free_space += slot.space();
        }
        if (free_space >= stack.count) {
            return true;
        }
    }
    return false;
}

uint32_t Inventory::count_of(const std::shared_ptr<Item>& item) const {
    if (!item) {
        return 0;
    }

    uint32_t total = 0;
    for (const ItemStack& slot : slots) {
        if (slot.item.get() == item.get()) {
            total += slot.count;
        }
    }
    return total;
}

bool Inventory::empty() const {
    for (const ItemStack& slot : slots) {
        if (!slot.empty()) {
            return false;
        }
    }
    return true;
}

void Inventory::clear() {
    for (ItemStack& slot : slots) {
        slot.clear();
    }
}


// ============================================================================
// ItemEntity
// ============================================================================

ItemEntity::ItemEntity() {
    collision_body.set_half_extents(Vec3f(0.2f, 0.2f, 0.2f));
    collision_body.mass = 0.1f;
    collision_body.reset_history();
    render_position = transform.position;
}

ItemEntity::ItemEntity(const ItemStack& stack,
                       const Vec3f& position,
                       const Vec3f& velocity)
    : stack(stack), velocity(velocity) {
    transform.position = position;

    collision_body.set_half_extents(Vec3f(0.2f, 0.2f, 0.2f));
    collision_body.position = position;
    collision_body.velocity = velocity;
    collision_body.mass = 0.1f;
    collision_body.gravity_enabled = true;
    collision_body.collisions_enabled = true;
    collision_body.reset_history();

    render_position = position;
}
