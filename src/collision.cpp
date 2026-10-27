#include "collision.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "block.h"
#include "chunk.h"
#include "world.h"


namespace {

constexpr float kContactEpsilon = 1e-4f;
constexpr float kMovementEpsilon = 1e-6f;
constexpr float kMaxDeltaTime = 0.1f;

inline float component(const Vec3f& value, int axis) {
    if (axis == 0) return value.x;
    if (axis == 1) return value.y;
    return value.z;
}

inline void set_component(Vec3f& value, int axis, float component_value) {
    if (axis == 0) value.x = component_value;
    else if (axis == 1) value.y = component_value;
    else value.z = component_value;
}

inline int floor_to_int(float value) {
    return static_cast<int>(std::floor(value));
}

// Последний индекс блока, который ещё пересекает значение value.
// 16.0 -> 15, 16.1 -> 16.
inline int ceil_to_block(float value) {
    return static_cast<int>(std::ceil(value)) - 1;
}

inline uint64_t pack_chunk_pos(const Vec2i& pos) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(pos.x)) << 32) |
           static_cast<uint32_t>(pos.y);
}

inline uint64_t pack_block_pos(const Vec3i& pos) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(pos.x)) << 42) ^
           (static_cast<uint64_t>(static_cast<uint32_t>(pos.y)) << 21) ^
           static_cast<uint64_t>(static_cast<uint32_t>(pos.z));
}

// Собирает статичные AABB из воксельного мира. Незагруженные чанки
// возвращают барьерный блок, поэтому они автоматически становятся стеной.
void collect_world_boxes(World* world,
                         const Aabb& query,
                         std::vector<Aabb>& out) {
    if (!world || !query.valid()) {
        return;
    }

    int x0 = floor_to_int(query.min.x);
    int x1 = ceil_to_block(query.max.x);
    int y0 = std::max(0, floor_to_int(query.min.y));
    int y1 = std::min(static_cast<int>(World::CHUNK_HEIGHT) - 1,
                      ceil_to_block(query.max.y));
    int z0 = floor_to_int(query.min.z);
    int z1 = ceil_to_block(query.max.z);

    if (x0 > x1 || y0 > y1 || z0 > z1) {
        return;
    }

    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                uint16_t nbt = 0;
                std::shared_ptr<Block> block =
                    world->get_block(Vec3i(x, y, z), nbt);
                if (!block) {
                    continue;
                }

                std::shared_ptr<CollisionMesh> mesh =
                    block->get_collision_mesh(nbt);
                if (!mesh || !mesh->physics_used() || mesh->empty()) {
                    continue;
                }

                const Vec3f origin(static_cast<float>(x),
                                   static_cast<float>(y),
                                   static_cast<float>(z));
                for (const Aabb& local_box : mesh->boxes) {
                    if (!local_box.valid()) {
                        continue;
                    }
                    Aabb world_box = local_box.translated(origin);
                    if (world_box.overlaps(query) ||
                        world_box.touches(query, nullptr, kContactEpsilon)) {
                        out.push_back(world_box);
                    }
                }
            }
        }
    }
}

void collect_static_boxes(World* world,
                          const std::vector<StaticCollision*>& statics,
                          const Aabb& query,
                          std::vector<Aabb>& out) {
    for (StaticCollision* object : statics) {
        if (object) {
            object->collect_boxes(query, out);
        }
    }
    collect_world_boxes(world, query, out);
}

// Двигает тело по одной оси и при необходимости останавливает его на грани
// статичного AABB. Возвращает true, если было столкновение.
bool move_axis(DynamicCollision& body,
               World* world,
               const std::vector<StaticCollision*>& statics,
               int axis,
               float delta) {
    if (std::abs(delta) <= kMovementEpsilon) {
        return false;
    }

    if (!body.enabled || !body.collisions_enabled) {
        set_component(body.position, axis,
                      component(body.position, axis) + delta);
        return false;
    }

    // Запоминаем положение до движения. Если тело уже перекрывало блок по
    // этой оси (например, из-за касания боком и погрешности float), то
    // движение по оси не должно «выталкивать» его на противоположную грань.
    // Иначе касание головой стены при движении вниз телепортировало тело
    // наверх стены.
    const float old_pos = component(body.position, axis);
    const float half = component(body.half_extents, axis);
    const float old_min = old_pos - half;
    const float old_max = old_pos + half;

    set_component(body.position, axis,
                  component(body.position, axis) + delta);

    bool hit = false;
    std::vector<Aabb> boxes;

    // 4 проходов достаточно даже для узких углов: после каждого уточнения
    // положение пересчитывается заново.
    for (int pass = 0; pass < 4; ++pass) {
        Aabb body_box = body.get_bounds().expanded(kContactEpsilon);
        boxes.clear();
        collect_static_boxes(world, statics, body_box, boxes);

        bool corrected = false;
        float target = component(body.position, axis);

        for (const Aabb& block : boxes) {
            Aabb current = body.get_bounds();
            if (!current.overlaps(block)) {
                continue;
            }

            const float block_min = component(block.min, axis);
            const float block_max = component(block.max, axis);

            if (delta > 0.0f) {
                // Блок должен находиться перед телом по направлению движения.
                // Небольшой допуск нужен для стабильного касания гранью.
                if (old_max > block_min + kContactEpsilon) {
                    continue;
                }
                target = std::min(target, block_min - half);
                hit = true;
                corrected = true;
            } else {
                if (old_min < block_max - kContactEpsilon) {
                    continue;
                }
                target = std::max(target, block_max + half);
                hit = true;
                corrected = true;
            }
        }

        if (!corrected) {
            break;
        }

        set_component(body.position, axis, target);
        set_component(body.velocity, axis, 0.0f);
    }

    return hit;
}

void refresh_contact_flags(DynamicCollision& body,
                           World* world,
                           const std::vector<StaticCollision*>& statics) {
    body.grounded = false;
    body.walled = false;
    if (!body.enabled || !body.collisions_enabled) {
        return;
    }

    Aabb body_box = body.get_bounds();
    Aabb query = body_box.expanded(kContactEpsilon * 2.0f);
    std::vector<Aabb> boxes;
    collect_static_boxes(world, statics, query, boxes);

    for (const Aabb& block : boxes) {
        Vec3f normal;
        if (!body_box.touches(block, &normal, kContactEpsilon)) {
            continue;
        }
        if (normal.y > 0.5f) {
            body.grounded = true;
        }
        if (std::abs(normal.x) > 0.5f || std::abs(normal.z) > 0.5f) {
            body.walled = true;
        }
    }
}

void resolve_static_penetration(DynamicCollision& body,
                                World* world,
                                const std::vector<StaticCollision*>& statics) {
    if (!body.enabled || !body.collisions_enabled) {
        return;
    }

    for (int pass = 0; pass < 4; ++pass) {
        Aabb body_box = body.get_bounds().expanded(kContactEpsilon);
        std::vector<Aabb> boxes;
        collect_static_boxes(world, statics, body_box, boxes);

        bool corrected = false;
        for (const Aabb& block : boxes) {
            Aabb current = body.get_bounds();
            if (!current.overlaps(block)) {
                continue;
            }
            Vec3f push = current.penetration(block);
            body.position = body.position + push;

            // Гасим скорость в направлении выталкивания.
            if (std::abs(push.x) >= std::abs(push.y) &&
                std::abs(push.x) >= std::abs(push.z)) {
                body.velocity.x = 0.0f;
            } else if (std::abs(push.y) >= std::abs(push.z)) {
                body.velocity.y = 0.0f;
            } else {
                body.velocity.z = 0.0f;
            }
            corrected = true;
        }
        if (!corrected) {
            break;
        }
    }
}

int choose_substeps(const std::vector<DynamicCollision*>& bodies,
                    float delta_time,
                    int max_substeps) {
    float max_travel = 0.0f;
    for (DynamicCollision* body : bodies) {
        if (!body || !body->enabled) {
            continue;
        }
        const Vec3f& v = body->velocity;
        float travel = (std::abs(v.x) + std::abs(v.y) + std::abs(v.z)) *
                       delta_time;
        max_travel = std::max(max_travel, travel);
    }

    int steps = static_cast<int>(std::ceil(max_travel / 0.2f));
    steps = std::max(1, steps);
    steps = std::min(steps, std::max(1, max_substeps));
    return steps;
}

} // namespace


// ============================================================================
// Aabb
// ============================================================================

Aabb::Aabb() : min(Vec3f::zero()), max(Vec3f::zero()) {}

Aabb::Aabb(const Vec3f& min, const Vec3f& max) : min(min), max(max) {
    if (this->min.x > this->max.x) std::swap(this->min.x, this->max.x);
    if (this->min.y > this->max.y) std::swap(this->min.y, this->max.y);
    if (this->min.z > this->max.z) std::swap(this->min.z, this->max.z);
}

Aabb Aabb::from_center_half(const Vec3f& center, const Vec3f& half_extents) {
    return Aabb(center - half_extents, center + half_extents);
}

Aabb Aabb::invalid() {
    constexpr float inf = std::numeric_limits<float>::infinity();
    Aabb result;
    // Не используем конструктор: он нормализует min/max и сделал бы
    // бесконечный AABB валидным.
    result.min = Vec3f(inf, inf, inf);
    result.max = Vec3f(-inf, -inf, -inf);
    return result;
}

bool Aabb::valid() const {
    return min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

Vec3f Aabb::center() const {
    return (min + max) * 0.5f;
}

Vec3f Aabb::half_extents() const {
    return (max - min) * 0.5f;
}

bool Aabb::overlaps(const Aabb& other) const {
    if (!valid() || !other.valid()) {
        return false;
    }
    return min.x < other.max.x && max.x > other.min.x &&
           min.y < other.max.y && max.y > other.min.y &&
           min.z < other.max.z && max.z > other.min.z;
}

bool Aabb::touches(const Aabb& other,
                   Vec3f* normal_out,
                   float epsilon) const {
    if (!valid() || !other.valid()) {
        return false;
    }

    // Строгое объёмное пересечение — это не касание.
    if (overlaps(other)) {
        return false;
    }

    int contact_axes[3] = {0, 0, 0};
    int contact_count = 0;

    for (int axis = 0; axis < 3; ++axis) {
        const float a_min = component(min, axis);
        const float a_max = component(max, axis);
        const float b_min = component(other.min, axis);
        const float b_max = component(other.max, axis);

        const float separation = std::max(b_min - a_max, a_min - b_max);
        if (separation > epsilon) {
            return false;
        }
        if (separation >= -epsilon) {
            contact_axes[axis] = 1;
            ++contact_count;
        }
    }

    if (contact_count == 0) {
        return false;
    }

    // Для грани контактная ось обычно одна. На ребре/углу выбираем ось с
    // наибольшим перекрытием по двум другим осям — это даёт стабильную
    // нормаль для isGrounded/isWalled.
    int best_axis = -1;
    float best_score = -std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        if (!contact_axes[axis]) {
            continue;
        }
        float score = 0.0f;
        for (int other_axis = 0; other_axis < 3; ++other_axis) {
            if (other_axis == axis) {
                continue;
            }
            float overlap = std::min(component(max, other_axis),
                                     component(other.max, other_axis)) -
                            std::max(component(min, other_axis),
                                     component(other.min, other_axis));
            score += overlap;
        }
        if (score > best_score) {
            best_score = score;
            best_axis = axis;
        }
    }

    if (best_axis < 0) {
        return false;
    }

    if (normal_out) {
        *normal_out = Vec3f::zero();
        const float a_center = component(center(), best_axis);
        const float b_center = component(other.center(), best_axis);
        set_component(*normal_out, best_axis,
                      a_center >= b_center ? 1.0f : -1.0f);
    }
    return true;
}

Vec3f Aabb::penetration(const Aabb& other) const {
    if (!overlaps(other)) {
        return Vec3f::zero();
    }

    const float overlap_x = std::min(max.x, other.max.x) -
                            std::max(min.x, other.min.x);
    const float overlap_y = std::min(max.y, other.max.y) -
                            std::max(min.y, other.min.y);
    const float overlap_z = std::min(max.z, other.max.z) -
                            std::max(min.z, other.min.z);

    int axis = 0;
    float smallest = overlap_x;
    if (overlap_y < smallest) {
        smallest = overlap_y;
        axis = 1;
    }
    if (overlap_z < smallest) {
        smallest = overlap_z;
        axis = 2;
    }

    Vec3f result = Vec3f::zero();
    const float self_center = component(center(), axis);
    const float other_center = component(other.center(), axis);
    set_component(result, axis,
                  self_center >= other_center ? smallest : -smallest);
    return result;
}

Aabb Aabb::translated(const Vec3f& delta) const {
    return Aabb(min + delta, max + delta);
}

Aabb Aabb::expanded(float amount) const {
    return Aabb(min - Vec3f(amount, amount, amount),
                max + Vec3f(amount, amount, amount));
}

Aabb Aabb::expanded(const Vec3f& amount) const {
    return Aabb(min - amount, max + amount);
}

bool aabb_overlap(const Aabb& a, const Aabb& b) {
    return a.overlaps(b);
}

bool aabb_touch(const Aabb& a, const Aabb& b, float epsilon) {
    return a.touches(b, nullptr, epsilon);
}

Vec3f aabb_contact_normal(const Aabb& a, const Aabb& b, float epsilon) {
    Vec3f normal = Vec3f::zero();
    a.touches(b, &normal, epsilon);
    return normal;
}

Vec3f aabb_penetration(const Aabb& moving, const Aabb& other) {
    return moving.penetration(other);
}


// ============================================================================
// CollisionMesh
// ============================================================================

void CollisionMesh::add_box(const Aabb& box) {
    if (box.valid()) {
        boxes.push_back(box);
    }
}

void CollisionMesh::add_box(const Vec3f& min, const Vec3f& max) {
    add_box(Aabb(min, max));
}

void CollisionMesh::add_box_center(const Vec3f& center,
                                   const Vec3f& half_extents) {
    add_box(Aabb::from_center_half(center, half_extents));
}

void CollisionMesh::add_unit_cube() {
    add_box(Aabb(Vec3f::zero(), Vec3f::one()));
}

void CollisionMesh::clear() {
    boxes.clear();
}

Aabb CollisionMesh::bounds() const {
    if (boxes.empty()) {
        return Aabb();
    }
    Aabb result = boxes.front();
    for (const Aabb& box : boxes) {
        result.min.x = std::min(result.min.x, box.min.x);
        result.min.y = std::min(result.min.y, box.min.y);
        result.min.z = std::min(result.min.z, box.min.z);
        result.max.x = std::max(result.max.x, box.max.x);
        result.max.y = std::max(result.max.y, box.max.y);
        result.max.z = std::max(result.max.z, box.max.z);
    }
    return result;
}

Aabb CollisionMesh::world_bounds(const Vec3f& origin) const {
    return bounds().translated(origin);
}

void CollisionMesh::collect_world_boxes(const Vec3f& origin,
                                        const Aabb& query,
                                        std::vector<Aabb>& out) const {
    if (!physics_used()) {
        return;
    }
    for (const Aabb& box : boxes) {
        if (!box.valid()) {
            continue;
        }
        Aabb world_box = box.translated(origin);
        if (world_box.overlaps(query) ||
            world_box.touches(query, nullptr, kContactEpsilon)) {
            out.push_back(world_box);
        }
    }
}

std::shared_ptr<CollisionMesh> CollisionMesh::unit_cube() {
    auto mesh = std::make_shared<CollisionMesh>();
    mesh->add_unit_cube();
    return mesh;
}

std::shared_ptr<CollisionMesh> CollisionMesh::box(const Aabb& local_box,
                                                  MeshPurpose purpose) {
    auto mesh = std::make_shared<CollisionMesh>(purpose);
    mesh->add_box(local_box);
    return mesh;
}

std::shared_ptr<CollisionMesh> CollisionMesh::box(const Vec3f& min,
                                                  const Vec3f& max,
                                                  MeshPurpose purpose) {
    return box(Aabb(min, max), purpose);
}

std::shared_ptr<CollisionMesh> CollisionMesh::from_mesh(const Mesh& mesh,
                                                        MeshPurpose purpose) {
    auto result = std::make_shared<CollisionMesh>(purpose);
    if (mesh.vertices.empty()) {
        return result;
    }

    Aabb bounds(Vec3f(mesh.vertices.front().x,
                      mesh.vertices.front().y,
                      mesh.vertices.front().z),
                Vec3f(mesh.vertices.front().x,
                      mesh.vertices.front().y,
                      mesh.vertices.front().z));
    for (const Vertex& vertex : mesh.vertices) {
        bounds.min.x = std::min(bounds.min.x, vertex.x);
        bounds.min.y = std::min(bounds.min.y, vertex.y);
        bounds.min.z = std::min(bounds.min.z, vertex.z);
        bounds.max.x = std::max(bounds.max.x, vertex.x);
        bounds.max.y = std::max(bounds.max.y, vertex.y);
        bounds.max.z = std::max(bounds.max.z, vertex.z);
    }
    result->add_box(bounds);
    return result;
}


// ============================================================================
// StaticCollision
// ============================================================================

StaticCollision::StaticCollision()
    : position(Vec3f::zero()), mesh(nullptr) {}

StaticCollision::StaticCollision(const Vec3f& position,
                                 std::shared_ptr<CollisionMesh> mesh)
    : position(position), mesh(std::move(mesh)) {}

Aabb StaticCollision::get_bounds() const {
    if (!mesh || mesh->empty()) {
        return Aabb();
    }
    return mesh->world_bounds(position);
}

bool StaticCollision::blocks_physics() const {
    return enabled && mesh && mesh->physics_used() && !mesh->empty();
}

void StaticCollision::collect_boxes(const Aabb& query,
                                    std::vector<Aabb>& out) const {
    if (!blocks_physics()) {
        return;
    }
    mesh->collect_world_boxes(position, query, out);
}


// ============================================================================
// DynamicCollision
// ============================================================================

DynamicCollision::DynamicCollision() {
    reset_history();
}

Aabb DynamicCollision::get_bounds() const {
    return Aabb::from_center_half(position, half_extents);
}

Aabb DynamicCollision::get_swept_bounds(const Vec3f& delta) const {
    Aabb result = get_bounds();
    Aabb moved = get_bounds().translated(delta);
    result.min.x = std::min(result.min.x, moved.min.x);
    result.min.y = std::min(result.min.y, moved.min.y);
    result.min.z = std::min(result.min.z, moved.min.z);
    result.max.x = std::max(result.max.x, moved.max.x);
    result.max.y = std::max(result.max.y, moved.max.y);
    result.max.z = std::max(result.max.z, moved.max.z);
    return result;
}

void DynamicCollision::set_position(const Vec3f& value,
                                    bool reset_history_after) {
    position = value;
    if (reset_history_after) {
        reset_history();
    }
}

void DynamicCollision::set_half_extents(const Vec3f& value) {
    half_extents = Vec3f(std::max(0.0f, value.x),
                         std::max(0.0f, value.y),
                         std::max(0.0f, value.z));
}

void DynamicCollision::record_history() {
    history_.push_back(position);
    const int limit = std::max(4, interpolation_ticks + 2);
    while (static_cast<int>(history_.size()) > limit) {
        history_.pop_front();
    }
}

void DynamicCollision::reset_history() {
    history_.clear();
    history_.push_back(position);
}

void DynamicCollision::set_interpolation_ticks(int value) {
    interpolation_ticks = std::max(0, value);
    while (static_cast<int>(history_.size()) >
           std::max(4, interpolation_ticks + 2)) {
        history_.pop_front();
    }
}

Vec3f DynamicCollision::get_render_position(float alpha) const {
    if (history_.empty()) {
        return position;
    }

    const int lag = std::max(0, interpolation_ticks);
    const int last = static_cast<int>(history_.size()) - 1;
    int base = std::max(0, last - lag);
    int next = std::min(last, base + 1);

    const float t = std::max(0.0f, std::min(1.0f, alpha));
    const Vec3f& a = history_[static_cast<size_t>(base)];
    const Vec3f& b = history_[static_cast<size_t>(next)];
    return a + (b - a) * t;
}


// ============================================================================
// EntityCollision / FragmentCollision
// ============================================================================

EntityCollision::EntityCollision() = default;

EntityCollision::EntityCollision(const Vec3f& half_extents,
                                 const Vec3f& position,
                                 float mass)
    : EntityCollision() {
    set_half_extents(half_extents);
    this->position = position;
    this->mass = mass;
    reset_history();
}

FragmentCollision::FragmentCollision() = default;

FragmentCollision::FragmentCollision(const Vec3f& half_extents,
                                     const Vec3f& position,
                                     float mass)
    : FragmentCollision() {
    set_half_extents(half_extents);
    this->position = position;
    this->mass = mass;
    reset_history();
}


// ============================================================================
// TriggerCollision
// ============================================================================

TriggerCollision::TriggerCollision() = default;

TriggerCollision::TriggerCollision(const Vec3f& position,
                                   const Vec3f& half_extents,
                                   const std::string& name)
    : position(position), half_extents(half_extents), name(name) {}

Aabb TriggerCollision::get_bounds() const {
    return Aabb::from_center_half(position, half_extents);
}

void TriggerCollision::on_enter(CollisionObject& other) {
    (void)other;
}

void TriggerCollision::on_exit(CollisionObject& other) {
    (void)other;
}


// ============================================================================
// CollisionWorld
// ============================================================================

CollisionWorld::CollisionWorld() = default;

CollisionWorld::~CollisionWorld() = default;

uint32_t CollisionWorld::assign_id(CollisionObject& object) {
    if (object.id == 0) {
        object.id = next_id_++;
    } else {
        next_id_ = std::max(next_id_, object.id + 1);
    }
    return object.id;
}

void CollisionWorld::set_world(World& world) {
    world_ = &world;
}

void CollisionWorld::clear_world() {
    world_ = nullptr;
}

void CollisionWorld::add_chunk(World& world, const Vec2i& chunk_pos) {
    world_ = &world;
    chunks_.insert(pack_chunk_pos(chunk_pos));
}

void CollisionWorld::remove_chunk(const Vec2i& chunk_pos) {
    chunks_.erase(pack_chunk_pos(chunk_pos));
}

bool CollisionWorld::has_chunk(const Vec2i& chunk_pos) const {
    return chunks_.find(pack_chunk_pos(chunk_pos)) != chunks_.end();
}

void CollisionWorld::clear_chunks() {
    chunks_.clear();
}

uint32_t CollisionWorld::add_static(
    const std::shared_ptr<StaticCollision>& object) {
    if (!object) {
        throw std::invalid_argument("CollisionWorld: null static object");
    }
    for (const auto& existing : statics_) {
        if (existing.get() == object.get()) {
            return assign_id(*object);
        }
    }
    statics_.push_back(object);
    return assign_id(*object);
}

uint32_t CollisionWorld::add_static_box(const Aabb& box) {
    auto object = std::make_shared<StaticCollision>(
        Vec3f::zero(), CollisionMesh::box(box));
    return add_static(object);
}

uint32_t CollisionWorld::add_static_box(const Vec3f& min,
                                        const Vec3f& max) {
    return add_static_box(Aabb(min, max));
}

StaticCollision* CollisionWorld::get_static(uint32_t object_id) {
    for (const auto& object : statics_) {
        if (object && object->id == object_id) {
            return object.get();
        }
    }
    return nullptr;
}

bool CollisionWorld::remove_static(uint32_t object_id) {
    auto it = std::find_if(
        statics_.begin(), statics_.end(),
        [object_id](const std::shared_ptr<StaticCollision>& object) {
            return object && object->id == object_id;
        });
    if (it == statics_.end()) {
        return false;
    }
    (*it)->id = 0;
    statics_.erase(it);
    return true;
}

void CollisionWorld::clear_statics() {
    for (auto& object : statics_) {
        if (object) object->id = 0;
    }
    statics_.clear();
}

uint32_t CollisionWorld::add_entity(const Vec3f& half_extents,
                                    const Vec3f& position,
                                    float mass) {
    auto body = std::make_unique<EntityCollision>(half_extents, position, mass);
    uint32_t id = assign_id(*body);
    entities_.push_back(std::move(body));
    return id;
}

uint32_t CollisionWorld::add_entity(EntityCollision& body) {
    for (EntityCollision* existing : external_entities_) {
        if (existing == &body) {
            return assign_id(body);
        }
    }
    for (const auto& owned : entities_) {
        if (owned.get() == &body) {
            return assign_id(body);
        }
    }
    external_entities_.push_back(&body);
    body.reset_history();
    return assign_id(body);
}

EntityCollision* CollisionWorld::get_entity(uint32_t object_id) {
    for (const auto& body : entities_) {
        if (body && body->id == object_id) {
            return body.get();
        }
    }
    for (EntityCollision* body : external_entities_) {
        if (body && body->id == object_id) {
            return body;
        }
    }
    return nullptr;
}

bool CollisionWorld::remove_entity(uint32_t object_id) {
    auto owned_it = std::find_if(
        entities_.begin(), entities_.end(),
        [object_id](const std::unique_ptr<EntityCollision>& body) {
            return body && body->id == object_id;
        });
    if (owned_it != entities_.end()) {
        entities_.erase(owned_it);
        return true;
    }

    auto external_it = std::find_if(
        external_entities_.begin(), external_entities_.end(),
        [object_id](EntityCollision* body) {
            return body && body->id == object_id;
        });
    if (external_it != external_entities_.end()) {
        (*external_it)->id = 0;
        external_entities_.erase(external_it);
        return true;
    }
    return false;
}

void CollisionWorld::clear_entities() {
    for (EntityCollision* body : external_entities_) {
        if (body) body->id = 0;
    }
    external_entities_.clear();
    entities_.clear();
}

uint32_t CollisionWorld::add_fragment(const Vec3f& half_extents,
                                      const Vec3f& position,
                                      float mass) {
    auto body = std::make_unique<FragmentCollision>(half_extents, position, mass);
    uint32_t id = assign_id(*body);
    fragments_.push_back(std::move(body));
    return id;
}

uint32_t CollisionWorld::add_fragment(FragmentCollision& body) {
    for (FragmentCollision* existing : external_fragments_) {
        if (existing == &body) {
            return assign_id(body);
        }
    }
    for (const auto& owned : fragments_) {
        if (owned.get() == &body) {
            return assign_id(body);
        }
    }
    external_fragments_.push_back(&body);
    body.reset_history();
    return assign_id(body);
}

FragmentCollision* CollisionWorld::get_fragment(uint32_t object_id) {
    for (const auto& body : fragments_) {
        if (body && body->id == object_id) {
            return body.get();
        }
    }
    for (FragmentCollision* body : external_fragments_) {
        if (body && body->id == object_id) {
            return body;
        }
    }
    return nullptr;
}

bool CollisionWorld::remove_fragment(uint32_t object_id) {
    auto owned_it = std::find_if(
        fragments_.begin(), fragments_.end(),
        [object_id](const std::unique_ptr<FragmentCollision>& body) {
            return body && body->id == object_id;
        });
    if (owned_it != fragments_.end()) {
        fragments_.erase(owned_it);
        return true;
    }

    auto external_it = std::find_if(
        external_fragments_.begin(), external_fragments_.end(),
        [object_id](FragmentCollision* body) {
            return body && body->id == object_id;
        });
    if (external_it != external_fragments_.end()) {
        (*external_it)->id = 0;
        external_fragments_.erase(external_it);
        return true;
    }
    return false;
}

void CollisionWorld::clear_fragments() {
    for (FragmentCollision* body : external_fragments_) {
        if (body) body->id = 0;
    }
    external_fragments_.clear();
    fragments_.clear();
}

uint32_t CollisionWorld::add_trigger(
    const std::shared_ptr<TriggerCollision>& object) {
    if (!object) {
        throw std::invalid_argument("CollisionWorld: null trigger object");
    }
    for (const auto& existing : triggers_) {
        if (existing.get() == object.get()) {
            return assign_id(*object);
        }
    }
    triggers_.push_back(object);
    return assign_id(*object);
}

uint32_t CollisionWorld::add_trigger_box(const Vec3f& position,
                                         const Vec3f& half_extents,
                                         const std::string& name) {
    auto object = std::make_shared<TriggerCollision>(
        position, half_extents, name);
    return add_trigger(object);
}

TriggerCollision* CollisionWorld::get_trigger(uint32_t object_id) {
    for (const auto& object : triggers_) {
        if (object && object->id == object_id) {
            return object.get();
        }
    }
    return nullptr;
}

bool CollisionWorld::remove_trigger(uint32_t object_id) {
    auto it = std::find_if(
        triggers_.begin(), triggers_.end(),
        [object_id](const std::shared_ptr<TriggerCollision>& object) {
            return object && object->id == object_id;
        });
    if (it == triggers_.end()) {
        return false;
    }
    (*it)->id = 0;
    triggers_.erase(it);
    return true;
}

void CollisionWorld::clear_triggers() {
    for (auto& object : triggers_) {
        if (object) object->id = 0;
    }
    triggers_.clear();
}

void CollisionWorld::clear_all() {
    clear_triggers();
    clear_fragments();
    clear_entities();
    clear_statics();
    clear_chunks();
    world_ = nullptr;
    next_id_ = 1;
    num_contacts_ = 0;
}

void CollisionWorld::resolve_entity_pairs() {
    std::vector<EntityCollision*> bodies;
    bodies.reserve(entities_.size() + external_entities_.size());
    for (const auto& body : entities_) {
        if (body) bodies.push_back(body.get());
    }
    for (EntityCollision* body : external_entities_) {
        if (body) bodies.push_back(body);
    }

    for (size_t i = 0; i < bodies.size(); ++i) {
        EntityCollision* a = bodies[i];
        if (!a || !a->enabled || !a->collisions_enabled) {
            continue;
        }
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            EntityCollision* b = bodies[j];
            if (!b || !b->enabled || !b->collisions_enabled) {
                continue;
            }

            Aabb box_a = a->get_bounds();
            Aabb box_b = b->get_bounds();
            if (!box_a.overlaps(box_b)) {
                continue;
            }

            Vec3f push = box_a.penetration(box_b);
            if (push.length() <= kMovementEpsilon) {
                continue;
            }

            ++num_contacts_;
            const float inv_a = a->inverse_mass();
            const float inv_b = b->inverse_mass();
            const float total_inv = inv_a + inv_b;
            if (total_inv <= kMovementEpsilon) {
                continue;
            }

            a->position = a->position + push * (inv_a / total_inv);
            b->position = b->position - push * (inv_b / total_inv);

            Vec3f normal = push.normalized();
            Vec3f relative = a->velocity - b->velocity;
            const float along_normal = relative.dot(normal);
            if (along_normal < 0.0f) {
                const float impulse = -along_normal / total_inv;
                a->velocity = a->velocity + normal * (impulse * inv_a);
                b->velocity = b->velocity - normal * (impulse * inv_b);
            }
        }
    }
}

void CollisionWorld::resolve_static_overlap(DynamicCollision& body) {
    std::vector<StaticCollision*> statics;
    statics.reserve(statics_.size());
    for (const auto& object : statics_) {
        statics.push_back(object.get());
    }
    resolve_static_penetration(body, world_, statics);
}

void CollisionWorld::refresh_contacts(DynamicCollision& body) {
    std::vector<StaticCollision*> statics;
    statics.reserve(statics_.size());
    for (const auto& object : statics_) {
        statics.push_back(object.get());
    }
    refresh_contact_flags(body, world_, statics);
}

void CollisionWorld::reset_trigger_events() {
    for (auto& trigger : triggers_) {
        if (!trigger) continue;
        trigger->entered.clear();
        trigger->exited.clear();
    }
}

void CollisionWorld::update_triggers() {
    std::vector<CollisionObject*> dynamics;
    std::unordered_map<uint32_t, CollisionObject*> by_id;
    auto add_dynamic = [&](CollisionObject* object) {
        if (!object || !object->enabled) return;
        dynamics.push_back(object);
        if (object->id != 0) by_id[object->id] = object;
    };

    for (const auto& body : entities_) add_dynamic(body.get());
    for (EntityCollision* body : external_entities_) add_dynamic(body);
    for (const auto& body : fragments_) add_dynamic(body.get());
    for (FragmentCollision* body : external_fragments_) add_dynamic(body);

    for (auto& trigger : triggers_) {
        if (!trigger) continue;

        std::unordered_set<uint32_t> current;
        if (trigger->enabled) {
            Aabb trigger_box = trigger->get_bounds();
            for (CollisionObject* object : dynamics) {
                if (!object || object->id == 0) continue;
                if (trigger_box.overlaps(object->get_bounds())) {
                    current.insert(object->id);
                    if (trigger->inside.find(object->id) ==
                        trigger->inside.end()) {
                        trigger->inside.insert(object->id);
                        trigger->entered.push_back(object->id);
                        trigger->on_enter(*object);
                    }
                }
            }
        }

        for (auto it = trigger->inside.begin();
             it != trigger->inside.end();) {
            if (current.find(*it) == current.end()) {
                const uint32_t id = *it;
                trigger->exited.push_back(id);
                auto found = by_id.find(id);
                if (found != by_id.end()) {
                    trigger->on_exit(*found->second);
                }
                it = trigger->inside.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void CollisionWorld::step(float delta_time,
                          int max_substeps,
                          float fixed_time_step) {
    (void)fixed_time_step;

    if (delta_time <= 0.0f) {
        return;
    }

    const float dt = std::min(delta_time, kMaxDeltaTime);
    num_contacts_ = 0;
    reset_trigger_events();

    std::vector<DynamicCollision*> all_bodies;
    all_bodies.reserve(entities_.size() + external_entities_.size() +
                       fragments_.size() + external_fragments_.size());

    for (auto& body : entities_) {
        if (body) all_bodies.push_back(body.get());
    }
    for (EntityCollision* body : external_entities_) {
        if (body) all_bodies.push_back(body);
    }
    for (auto& body : fragments_) {
        if (body) all_bodies.push_back(body.get());
    }
    for (FragmentCollision* body : external_fragments_) {
        if (body) all_bodies.push_back(body);
    }

    // Гравитация применяется один раз на весь шаг, а движение дробится
    // на подшаги для защиты от туннелирования.
    for (DynamicCollision* body : all_bodies) {
        if (body && body->enabled && body->gravity_enabled) {
            body->velocity = body->velocity + gravity_ * dt;
        }
    }

    const int substeps = choose_substeps(all_bodies, dt, max_substeps);
    const float h = dt / static_cast<float>(substeps);

    std::vector<StaticCollision*> statics;
    statics.reserve(statics_.size());
    for (const auto& object : statics_) {
        statics.push_back(object.get());
    }

    for (int step_index = 0; step_index < substeps; ++step_index) {
        for (auto& body : entities_) {
            if (body && body->enabled) {
                if (move_axis(*body, world_, statics, 0, body->velocity.x * h)) {
                    ++num_contacts_;
                }
                if (move_axis(*body, world_, statics, 1, body->velocity.y * h)) {
                    ++num_contacts_;
                }
                if (move_axis(*body, world_, statics, 2, body->velocity.z * h)) {
                    ++num_contacts_;
                }
            }
        }
        for (EntityCollision* body : external_entities_) {
            if (body && body->enabled) {
                if (move_axis(*body, world_, statics, 0, body->velocity.x * h)) {
                    ++num_contacts_;
                }
                if (move_axis(*body, world_, statics, 1, body->velocity.y * h)) {
                    ++num_contacts_;
                }
                if (move_axis(*body, world_, statics, 2, body->velocity.z * h)) {
                    ++num_contacts_;
                }
            }
        }

        resolve_entity_pairs();

        for (auto& body : entities_) {
            if (body) resolve_static_overlap(*body);
        }
        for (EntityCollision* body : external_entities_) {
            if (body) resolve_static_overlap(*body);
        }

        for (auto& body : fragments_) {
            if (body && body->enabled) {
                if (move_axis(*body, world_, statics, 0, body->velocity.x * h)) {
                    ++num_contacts_;
                }
                if (move_axis(*body, world_, statics, 1, body->velocity.y * h)) {
                    ++num_contacts_;
                }
                if (move_axis(*body, world_, statics, 2, body->velocity.z * h)) {
                    ++num_contacts_;
                }
                resolve_static_overlap(*body);
            }
        }
        for (FragmentCollision* body : external_fragments_) {
            if (body && body->enabled) {
                if (move_axis(*body, world_, statics, 0, body->velocity.x * h)) {
                    ++num_contacts_;
                }
                if (move_axis(*body, world_, statics, 1, body->velocity.y * h)) {
                    ++num_contacts_;
                }
                if (move_axis(*body, world_, statics, 2, body->velocity.z * h)) {
                    ++num_contacts_;
                }
                resolve_static_overlap(*body);
            }
        }
    }

    for (DynamicCollision* body : all_bodies) {
        if (!body) continue;
        refresh_contacts(*body);
        body->record_history();
    }

    update_triggers();
}

void CollisionWorld::step_fragments(float delta_time) {
    if (delta_time <= 0.0f) {
        return;
    }

    const float dt = std::min(delta_time, kMaxDeltaTime);
    reset_trigger_events();
    std::vector<FragmentCollision*> fragments;
    fragments.reserve(fragments_.size() + external_fragments_.size());
    for (auto& body : fragments_) {
        if (body) fragments.push_back(body.get());
    }
    for (FragmentCollision* body : external_fragments_) {
        if (body) fragments.push_back(body);
    }

    for (FragmentCollision* body : fragments) {
        if (body && body->enabled && body->gravity_enabled) {
            body->velocity = body->velocity + gravity_ * dt;
        }
    }

    std::vector<StaticCollision*> statics;
    statics.reserve(statics_.size());
    for (const auto& object : statics_) {
        statics.push_back(object.get());
    }

    std::vector<DynamicCollision*> dynamic_fragments;
    dynamic_fragments.reserve(fragments.size());
    for (FragmentCollision* body : fragments) {
        if (body && body->enabled) dynamic_fragments.push_back(body);
    }
    const int substeps = choose_substeps(dynamic_fragments, dt, 16);
    const float h = dt / static_cast<float>(substeps);

    for (FragmentCollision* body : fragments) {
        if (!body || !body->enabled) continue;
        for (int i = 0; i < substeps; ++i) {
            move_axis(*body, world_, statics, 0, body->velocity.x * h);
            move_axis(*body, world_, statics, 1, body->velocity.y * h);
            move_axis(*body, world_, statics, 2, body->velocity.z * h);
            resolve_static_penetration(*body, world_, statics);
        }
        refresh_contact_flags(*body, world_, statics);
        body->record_history();
    }
    update_triggers();
}

int CollisionWorld::get_num_collision_objects() const {
    return static_cast<int>(statics_.size() + entities_.size() +
                            external_entities_.size() + fragments_.size() +
                            external_fragments_.size() + triggers_.size());
}

Vec3f CollisionWorld::get_entity_position(uint32_t object_id) const {
    for (const auto& body : entities_) {
        if (body && body->id == object_id) return body->position;
    }
    for (EntityCollision* body : external_entities_) {
        if (body && body->id == object_id) return body->position;
    }
    throw std::out_of_range("CollisionWorld: invalid entity id");
}

Vec3f CollisionWorld::get_entity_render_position(uint32_t object_id,
                                                 float alpha) const {
    for (const auto& body : entities_) {
        if (body && body->id == object_id) {
            return body->get_render_position(alpha);
        }
    }
    for (EntityCollision* body : external_entities_) {
        if (body && body->id == object_id) {
            return body->get_render_position(alpha);
        }
    }
    throw std::out_of_range("CollisionWorld: invalid entity id");
}

Vec3f CollisionWorld::get_entity_velocity(uint32_t object_id) const {
    for (const auto& body : entities_) {
        if (body && body->id == object_id) return body->velocity;
    }
    for (EntityCollision* body : external_entities_) {
        if (body && body->id == object_id) return body->velocity;
    }
    throw std::out_of_range("CollisionWorld: invalid entity id");
}

bool CollisionWorld::is_entity_grounded(uint32_t object_id) const {
    for (const auto& body : entities_) {
        if (body && body->id == object_id) return body->grounded;
    }
    for (EntityCollision* body : external_entities_) {
        if (body && body->id == object_id) return body->grounded;
    }
    throw std::out_of_range("CollisionWorld: invalid entity id");
}

bool CollisionWorld::is_entity_walled(uint32_t object_id) const {
    for (const auto& body : entities_) {
        if (body && body->id == object_id) return body->walled;
    }
    for (EntityCollision* body : external_entities_) {
        if (body && body->id == object_id) return body->walled;
    }
    throw std::out_of_range("CollisionWorld: invalid entity id");
}

void CollisionWorld::set_entity_position(uint32_t object_id,
                                         const Vec3f& position) {
    for (auto& body : entities_) {
        if (body && body->id == object_id) {
            body->set_position(position);
            return;
        }
    }
    for (EntityCollision* body : external_entities_) {
        if (body && body->id == object_id) {
            body->set_position(position);
            return;
        }
    }
    throw std::out_of_range("CollisionWorld: invalid entity id");
}


// ============================================================================
// Автономный шаг фрагмента в World
// ============================================================================

void step_fragment_against_world(World& world,
                                 FragmentCollision& body,
                                 float delta_time,
                                 const Vec3f& gravity) {
    if (!body.enabled || delta_time <= 0.0f) {
        return;
    }

    const float dt = std::min(delta_time, kMaxDeltaTime);

    if (body.gravity_enabled) {
        body.velocity = body.velocity + gravity * dt;
    }

    std::vector<DynamicCollision*> single{&body};
    const int substeps = choose_substeps(single, dt, 16);
    const float h = dt / static_cast<float>(substeps);

    const std::vector<StaticCollision*> no_statics;
    for (int i = 0; i < substeps; ++i) {
        move_axis(body, &world, no_statics, 0, body.velocity.x * h);
        move_axis(body, &world, no_statics, 1, body.velocity.y * h);
        move_axis(body, &world, no_statics, 2, body.velocity.z * h);
    }

    refresh_contact_flags(body, &world, no_statics);

    // Лёгкое горизонтальное трение при контакте с полом — поведение
    // выпавших предметов из старого World::update_items.
    if (body.grounded) {
        body.velocity.x *= 0.75f;
        body.velocity.z *= 0.75f;
    }

    body.record_history();
}
