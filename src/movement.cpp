#include "movement.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <queue>
#include <unordered_map>
#include <utility>

#include "block.h"
#include "collision.h"
#include "world.h"


namespace {

constexpr float kEpsilon = 1e-4f;
constexpr float kMaxDrop = 8.0f;
constexpr float kLiftIncrement = 0.05f;
constexpr std::size_t kMaxSearchNodes = 50000;

inline int floor_block(float value) {
    return static_cast<int>(std::floor(value));
}

// Последний индекс блока, который ещё пересекает значение value.
inline int ceil_to_block(float value) {
    return static_cast<int>(std::ceil(value)) - 1;
}

void collect_collision_boxes(World& world,
                             const Aabb& query,
                             std::vector<Aabb>& out) {
    if (!query.valid()) {
        return;
    }

    const int x0 = floor_block(query.min.x);
    const int x1 = ceil_to_block(query.max.x);
    const int y0 = std::max(0, floor_block(query.min.y));
    const int y1 = std::min(static_cast<int>(World::CHUNK_HEIGHT) - 1,
                            ceil_to_block(query.max.y));
    const int z0 = floor_block(query.min.z);
    const int z1 = ceil_to_block(query.max.z);

    if (x0 > x1 || y0 > y1 || z0 > z1) {
        return;
    }

    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                uint16_t nbt = 0;
                std::shared_ptr<Block> block =
                    world.get_block(Vec3i(x, y, z), nbt);
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
                        world_box.touches(query, nullptr, kEpsilon)) {
                        out.push_back(world_box);
                    }
                }
            }
        }
    }
}

PathNode make_node(int x, int z, float feet_y) {
    return PathNode(Vec3i(x, floor_block(feet_y + kEpsilon), z), feet_y);
}

// Определяет, где именно стоят ступни для заданной клетки. Если клетка
// свободна, ступни находятся на её нижней границе. Если клетка занята
// блоком, ищем верх опоры (например, для цели «встать на блок»).
bool resolve_feet_y(World& world,
                    const Vec3i& block,
                    const Vec3f& half_extents,
                    float& out_feet_y) {
    if (block.y < 0 || block.y >= static_cast<int>(World::CHUNK_HEIGHT)) {
        return false;
    }

    const Vec3f feet(static_cast<float>(block.x) + 0.5f,
                     static_cast<float>(block.y),
                     static_cast<float>(block.z) + 0.5f);
    if (GroundMover::body_free(world, feet, half_extents)) {
        out_feet_y = static_cast<float>(block.y);
        return true;
    }

    float support = 0.0f;
    if (!GroundMover::support_height(
            world, block.x, block.z, half_extents,
            static_cast<float>(block.y) + 1.0f + kEpsilon,
            4.0f, support)) {
        return false;
    }

    const Vec3f top(static_cast<float>(block.x) + 0.5f,
                    support,
                    static_cast<float>(block.z) + 0.5f);
    if (!GroundMover::body_free(world, top, half_extents)) {
        return false;
    }

    out_feet_y = support;
    return true;
}

struct NodeKey {
    int x = 0;
    int yq = 0;
    int z = 0;

    bool operator==(const NodeKey& other) const {
        return x == other.x && yq == other.yq && z == other.z;
    }
};

struct NodeKeyHash {
    std::size_t operator()(const NodeKey& key) const {
        std::size_t h = std::hash<int>()(key.x);
        h ^= std::hash<int>()(key.yq) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(key.z) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

struct SearchNode {
    NodeKey key;
    float feet_y = 0.0f;
    float g = 0.0f;
    float f = 0.0f;
    int parent = -1;
};

struct QueueItem {
    float f = 0.0f;
    int index = -1;

    bool operator>(const QueueItem& other) const {
        return f > other.f;
    }
};

NodeKey node_key(int x, int z, float feet_y) {
    NodeKey key;
    key.x = x;
    key.yq = static_cast<int>(std::lround(feet_y * 1000.0f));
    key.z = z;
    return key;
}

float path_node_distance(const PathNode& a, const PathNode& b) {
    return (b.feet_position() - a.feet_position()).length();
}

float compute_path_length(const std::vector<PathNode>& path) {
    float length = 0.0f;
    for (std::size_t i = 1; i < path.size(); ++i) {
        length += path_node_distance(path[i - 1], path[i]);
    }
    return length;
}

} // namespace


// ============================================================================
// PathMover
// ============================================================================

PathMover::~PathMover() {
    restore_gravity();
}

bool PathMover::find_path(World& world,
                          const Vec3i& start,
                          const Vec3i& target) {
    return find_path(world, start, target, half_extents_, step_height_);
}

bool PathMover::find_path(World& world,
                          const Vec3i& start,
                          const Vec3i& target,
                          float step_height) {
    step_height_ = std::max(0.0f, step_height);
    return find_path(world, start, target, half_extents_, step_height_);
}

bool PathMover::find_path(World& world,
                          const Vec3i& start,
                          const Vec3i& target,
                          const Vec3f& half_extents) {
    half_extents_ = Vec3f(std::max(0.001f, half_extents.x),
                          std::max(0.001f, half_extents.y),
                          std::max(0.001f, half_extents.z));
    return find_path(world, start, target, half_extents_, step_height_);
}

bool PathMover::move_to(World& world,
                        DynamicCollision& body,
                        const Vec3i& start,
                        const Vec3i& target,
                        float duration,
                        std::optional<float> step_height,
                        bool snap_to_start) {
    half_extents_ = Vec3f(std::max(0.001f, body.half_extents.x),
                          std::max(0.001f, body.half_extents.y),
                          std::max(0.001f, body.half_extents.z));
    if (step_height.has_value()) {
        step_height_ = std::max(0.0f, *step_height);
    }

    if (!find_path(world, start, target, half_extents_, step_height_)) {
        moving_ = false;
        body.velocity.x = 0.0f;
        body.velocity.z = 0.0f;
        if (flying_) {
            body.velocity.y = 0.0f;
        }
        return false;
    }

    start_path(body, duration, snap_to_start);
    return true;
}

bool PathMover::move_to(World& world,
                        DynamicCollision& body,
                        const Vec3i& target,
                        float duration,
                        std::optional<float> step_height,
                        bool snap_to_start) {
    const Vec3i start = current_block(body);
    return move_to(world, body, start, target, duration, step_height,
                   snap_to_start);
}

void PathMover::start_path(DynamicCollision& body,
                           float duration,
                           bool snap_to_start) {
    path_index_ = 0;
    path_length_ = compute_path_length(path_);
    moving_ = path_.size() > 1;

    if (duration > 0.0f) {
        speed_ = path_length_ / std::max(duration, 1e-3f);
    } else if (path_length_ > 0.0f) {
        speed_ = path_length_;
    } else {
        speed_ = 0.0f;
    }

    if (path_.empty()) {
        moving_ = false;
        restore_gravity();
        return;
    }

    if (snap_to_start) {
        const PathNode& first = path_.front();
        body.set_position(first.body_center(body.half_extents));
        body.velocity = Vec3f::zero();
        body.grounded = false;
    }

    if (!moving_) {
        restore_gravity();
        return;
    }

    path_index_ = 1;
    controlled_body_ = &body;

    if (flying_ && !gravity_saved_) {
        saved_gravity_enabled_ = body.gravity_enabled;
        body.gravity_enabled = false;
        gravity_saved_ = true;
    }
}

bool PathMover::finish_path(DynamicCollision& body) {
    if (flying_) {
        body.velocity = Vec3f::zero();
    } else {
        body.velocity.x = 0.0f;
        body.velocity.z = 0.0f;
    }
    stop();
    return false;
}

bool PathMover::update(World& world,
                       DynamicCollision& body,
                       float delta_time) {
    if (!moving_ || path_.empty() || delta_time <= 0.0f) {
        return false;
    }

    const float arrive_vertical = arrival_radius_ + 0.05f;

    // Проходим уже достигнутые узлы (например, если за один кадр тело
    // оказалось сразу в следующем узле).
    for (std::size_t guard = 0; guard <= path_.size(); ++guard) {
        if (path_index_ >= path_.size()) {
            return finish_path(body);
        }

        const PathNode& node = path_[path_index_];
        const Vec3f target = node.body_center(body.half_extents);
        const Vec3f delta = target - body.position;
        const float horizontal =
            std::sqrt(delta.x * delta.x + delta.z * delta.z);
        const float vertical = std::abs(delta.y);

        if (horizontal <= arrival_radius_ && vertical <= arrive_vertical) {
            ++path_index_;
            continue;
        }
        break;
    }

    if (path_index_ >= path_.size()) {
        return finish_path(body);
    }

    const PathNode& node = path_[path_index_];
    const Vec3f target = node.body_center(body.half_extents);

    if (flying_) {
        Vec3f delta = target - body.position;
        const float length = delta.length();
        if (length <= arrival_radius_) {
            ++path_index_;
            return update(world, body, delta_time);
        }

        // Не зависаем в миллиметрах под узлом: если по высоте осталось
        // совсем немного, аккуратно доводим Y до центра узла. Иначе
        // горизонтальное движение может упереться в верх препятствия,
        // хотя путь уже проходит над ним.
        if (std::abs(delta.y) <= 0.12f) {
            const Vec3f snap_feet(body.position.x,
                                  target.y - body.half_extents.y,
                                  body.position.z);
            if (GroundMover::body_free(world, snap_feet,
                                       body.half_extents)) {
                body.position.y = target.y;
                delta.y = 0.0f;
                body.velocity.y = 0.0f;
            }
        }

        const float remaining = delta.length();
        body.velocity = remaining > 1e-5f
                            ? (delta / remaining) * speed_
                            : Vec3f::zero();
        return true;
    }

    const Vec3f horizontal_delta(target.x - body.position.x,
                                 0.0f,
                                 target.z - body.position.z);
    const float horizontal_length = horizontal_delta.length();
    if (horizontal_length > 1e-5f) {
        const Vec3f direction = horizontal_delta / horizontal_length;
        body.velocity.x = direction.x * speed_;
        body.velocity.z = direction.z * speed_;
    } else {
        body.velocity.x = 0.0f;
        body.velocity.z = 0.0f;
    }

    // Шаг вверх выполняется до физического шага: тело приподнимается, а
    // CollisionWorld затем двигает его горизонтально и опускает на опору.
    const Vec3f desired(static_cast<float>(body.velocity.x * delta_time),
                        0.0f,
                        static_cast<float>(body.velocity.z * delta_time));
    GroundMover::try_step_up(world, body, desired, step_height_);

    return true;
}

void PathMover::stop() {
    moving_ = false;
    path_index_ = path_.size();
    restore_gravity();
}

void PathMover::clear_path() {
    stop();
    path_.clear();
    path_index_ = 0;
    path_length_ = 0.0f;
}

void PathMover::restore_gravity() {
    if (gravity_saved_ && controlled_body_) {
        controlled_body_->gravity_enabled = saved_gravity_enabled_;
    }
    gravity_saved_ = false;
    controlled_body_ = nullptr;
}

std::vector<Vec3i> PathMover::get_path_blocks() const {
    std::vector<Vec3i> result;
    result.reserve(path_.size());
    for (const PathNode& node : path_) {
        result.push_back(node.block);
    }
    return result;
}

PathNode PathMover::current_node() const {
    if (path_.empty() || path_index_ >= path_.size()) {
        return PathNode();
    }
    return path_[path_index_];
}

PathNode PathMover::target_node() const {
    if (path_.empty()) {
        return PathNode();
    }
    return path_.back();
}

void PathMover::set_speed(float value) {
    speed_ = std::max(0.0f, value);
}

void PathMover::set_arrival_radius(float value) {
    arrival_radius_ = std::max(0.001f, value);
}

void PathMover::set_step_height(float value) {
    step_height_ = std::max(0.0f, value);
}

void PathMover::set_half_extents(const Vec3f& value) {
    half_extents_ = Vec3f(std::max(0.001f, value.x),
                          std::max(0.001f, value.y),
                          std::max(0.001f, value.z));
}

Vec3i PathMover::current_block(const DynamicCollision& body) {
    const Vec3f feet = body.position - Vec3f(0.0f, body.half_extents.y, 0.0f);
    return Vec3i(floor_block(feet.x + kEpsilon),
                 floor_block(feet.y + kEpsilon),
                 floor_block(feet.z + kEpsilon));
}


// ============================================================================
// GroundMover
// ============================================================================

GroundMover::GroundMover() {
    flying_ = false;
    set_half_extents(Vec3f(0.3f, 0.9f, 0.3f));
    set_step_height(0.6f);
}

GroundMover::GroundMover(float step_height)
    : GroundMover() {
    set_step_height(step_height);
}

GroundMover::GroundMover(float body_radius,
                         float body_height,
                         float step_height)
    : GroundMover(step_height) {
    set_half_extents(Vec3f(std::max(0.001f, body_radius),
                           std::max(0.001f, body_height * 0.5f),
                           std::max(0.001f, body_radius)));
}

bool GroundMover::body_free(World& world,
                            const Vec3f& feet_position,
                            const Vec3f& half_extents) {
    if (!std::isfinite(feet_position.x) ||
        !std::isfinite(feet_position.y) ||
        !std::isfinite(feet_position.z)) {
        return false;
    }

    const float height = half_extents.y * 2.0f;
    if (feet_position.y < -kEpsilon ||
        feet_position.y + height >
            static_cast<float>(World::CHUNK_HEIGHT) + kEpsilon) {
        return false;
    }

    const Vec3f center = feet_position + Vec3f(0.0f, half_extents.y, 0.0f);
    Aabb body_box = Aabb::from_center_half(center, half_extents)
                        .expanded(-kEpsilon);
    if (!body_box.valid()) {
        return false;
    }

    std::vector<Aabb> boxes;
    collect_collision_boxes(world, body_box.expanded(kEpsilon * 2.0f), boxes);
    for (const Aabb& box : boxes) {
        if (body_box.overlaps(box)) {
            return false;
        }
    }
    return true;
}

bool GroundMover::support_height(World& world,
                                 int x,
                                 int z,
                                 const Vec3f& half_extents,
                                 float max_y,
                                 float max_drop,
                                 float& out_height) {
    const float center_x = static_cast<float>(x) + 0.5f;
    const float center_z = static_cast<float>(z) + 0.5f;
    const float min_x = center_x - half_extents.x;
    const float max_x = center_x + half_extents.x;
    const float min_z = center_z - half_extents.z;
    const float max_z = center_z + half_extents.z;

    const int y_top = std::min(static_cast<int>(World::CHUNK_HEIGHT) - 1,
                               floor_block(max_y + kEpsilon));
    // Опора с верхом ровно на max_y лежит в блоке ниже, поэтому спускаемся
    // ещё на один блок (с запасом на погрешность float).
    const int y_bottom = std::max(
        0, floor_block(max_y - std::max(0.0f, max_drop) - 2.0f * kEpsilon));
    if (y_top < y_bottom) {
        return false;
    }

    const int x0 = floor_block(min_x);
    const int x1 = ceil_to_block(max_x);
    const int z0 = floor_block(min_z);
    const int z1 = ceil_to_block(max_z);

    bool found = false;
    float best = -std::numeric_limits<float>::infinity();

    for (int by = y_top; by >= y_bottom; --by) {
        for (int bz = z0; bz <= z1; ++bz) {
            for (int bx = x0; bx <= x1; ++bx) {
                uint16_t nbt = 0;
                std::shared_ptr<Block> block =
                    world.get_block(Vec3i(bx, by, bz), nbt);
                if (!block) {
                    continue;
                }

                std::shared_ptr<CollisionMesh> mesh =
                    block->get_collision_mesh(nbt);
                if (!mesh || !mesh->physics_used() || mesh->empty()) {
                    continue;
                }

                const Vec3f origin(static_cast<float>(bx),
                                   static_cast<float>(by),
                                   static_cast<float>(bz));
                for (const Aabb& local_box : mesh->boxes) {
                    if (!local_box.valid()) {
                        continue;
                    }
                    const Aabb world_box = local_box.translated(origin);
                    if (world_box.max.y > max_y + kEpsilon) {
                        continue;
                    }

                    const float overlap_x =
                        std::min(max_x, world_box.max.x) -
                        std::max(min_x, world_box.min.x);
                    const float overlap_z =
                        std::min(max_z, world_box.max.z) -
                        std::max(min_z, world_box.min.z);
                    if (overlap_x <= kEpsilon || overlap_z <= kEpsilon) {
                        continue;
                    }

                    if (world_box.max.y > best) {
                        best = world_box.max.y;
                        found = true;
                    }
                }
            }
        }
    }

    if (!found) {
        return false;
    }
    out_height = best;
    return true;
}

bool GroundMover::try_step_up(World& world,
                              DynamicCollision& body,
                              const Vec3f& desired_delta,
                              float step_height,
                              float* lifted) {
    if (!body.enabled || !body.collisions_enabled || !body.grounded) {
        return false;
    }

    const float max_lift = std::max(0.0f, step_height);
    if (max_lift < kLiftIncrement) {
        return false;
    }

    const float horizontal =
        std::abs(desired_delta.x) + std::abs(desired_delta.z);
    if (horizontal <= 1e-6f) {
        return false;
    }

    const Vec3f half = body.half_extents;
    const Vec3f feet = body.position - Vec3f(0.0f, half.y, 0.0f);
    const Vec3f desired_feet =
        feet + Vec3f(desired_delta.x, 0.0f, desired_delta.z);

    // Если горизонтальный шаг и так свободен, ступенька не нужна.
    if (body_free(world, desired_feet, half)) {
        return false;
    }

    // Ищем минимальный подъём, который убирает препятствие. Это важно,
    // если step_height больше фактической ступеньки: не поднимаем тело
    // выше, чем нужно, и не упираемся в потолок.
    for (float lift = kLiftIncrement;
         lift <= max_lift + kEpsilon;
         lift += kLiftIncrement) {
        const Vec3f raised_feet = feet + Vec3f(0.0f, lift, 0.0f);
        const Vec3f raised_desired =
            desired_feet + Vec3f(0.0f, lift, 0.0f);

        if (!body_free(world, raised_feet, half) ||
            !body_free(world, raised_desired, half)) {
            continue;
        }

        body.position.y += lift;
        if (std::abs(desired_delta.x) >= std::abs(desired_delta.z)) {
            body.position.x += desired_delta.x;
        } else {
            body.position.z += desired_delta.z;
        }
        body.grounded = false;
        if (body.velocity.y < 0.0f) {
            body.velocity.y = 0.0f;
        }
        if (lifted) {
            *lifted = lift;
        }
        return true;
    }

    return false;
}

bool GroundMover::find_path(World& world,
                            const Vec3i& start,
                            const Vec3i& target,
                            const Vec3f& half_extents,
                            float step_height) {
    clear_path();

    half_extents_ = Vec3f(std::max(0.001f, half_extents.x),
                          std::max(0.001f, half_extents.y),
                          std::max(0.001f, half_extents.z));
    step_height_ = std::max(0.0f, step_height);

    float start_feet = 0.0f;
    float target_feet = 0.0f;
    if (!resolve_feet_y(world, start, half_extents_, start_feet) ||
        !resolve_feet_y(world, target, half_extents_, target_feet)) {
        return false;
    }

    const NodeKey start_key = node_key(start.x, start.z, start_feet);
    const NodeKey goal_key = node_key(target.x, target.z, target_feet);

    if (start_key == goal_key) {
        path_.push_back(make_node(start.x, start.z, start_feet));
        path_length_ = 0.0f;
        return true;
    }

    std::vector<SearchNode> nodes;
    nodes.reserve(1024);
    std::unordered_map<NodeKey, float, NodeKeyHash> best;
    std::priority_queue<QueueItem, std::vector<QueueItem>,
                        std::greater<QueueItem>> open;

    SearchNode first;
    first.key = start_key;
    first.feet_y = start_feet;
    first.g = 0.0f;
    first.f = static_cast<float>(
        std::abs(start.x - target.x) +
        std::abs(start.z - target.z)) +
        std::abs(start_feet - target_feet) * 0.5f;
    first.parent = -1;
    nodes.push_back(first);
    best[start_key] = 0.0f;
    open.push({first.f, 0});

    const int directions[4][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };

    while (!open.empty()) {
        const QueueItem item = open.top();
        open.pop();

        if (item.index < 0 ||
            item.index >= static_cast<int>(nodes.size())) {
            continue;
        }

        const SearchNode current = nodes[static_cast<std::size_t>(item.index)];
        auto best_it = best.find(current.key);
        if (best_it == best.end() ||
            current.g > best_it->second + kEpsilon) {
            continue;
        }

        if (current.key == goal_key) {
            std::vector<PathNode> result;
            int index = item.index;
            while (index >= 0) {
                const SearchNode& node =
                    nodes[static_cast<std::size_t>(index)];
                result.push_back(
                    make_node(node.key.x, node.key.z, node.feet_y));
                index = node.parent;
            }
            std::reverse(result.begin(), result.end());
            path_ = std::move(result);
            path_length_ = compute_path_length(path_);
            return true;
        }

        for (const auto& direction : directions) {
            const int nx = current.key.x + direction[0];
            const int nz = current.key.z + direction[1];
            const float max_reach =
                current.feet_y + step_height_ + kEpsilon;

            float support = 0.0f;
            if (!support_height(world, nx, nz, half_extents_,
                                max_reach, kMaxDrop, support)) {
                continue;
            }
            if (support > max_reach + kEpsilon) {
                continue;
            }

            const Vec3f feet(static_cast<float>(nx) + 0.5f,
                             support,
                             static_cast<float>(nz) + 0.5f);
            if (!body_free(world, feet, half_extents_)) {
                continue;
            }

            // При шаге вверх тело сначала приподнимается в текущей клетке.
            // Проверяем, что над ней есть место, иначе команда была бы
            // заведомо невыполнимой.
            if (support > current.feet_y + kEpsilon) {
                const Vec3f raised_current(
                    static_cast<float>(current.key.x) + 0.5f,
                    support,
                    static_cast<float>(current.key.z) + 0.5f);
                if (!body_free(world, raised_current, half_extents_)) {
                    continue;
                }
            }

            const NodeKey next_key = node_key(nx, nz, support);
            const float dy = support - current.feet_y;
            const float step_cost =
                1.0f + std::abs(dy) * 0.75f +
                (dy > 0.0f ? 0.25f : 0.0f);
            const float next_g = current.g + step_cost;

            auto next_best = best.find(next_key);
            if (next_best != best.end() &&
                next_g >= next_best->second - kEpsilon) {
                continue;
            }
            best[next_key] = next_g;

            const float heuristic =
                static_cast<float>(
                    std::abs(nx - target.x) +
                    std::abs(nz - target.z)) +
                std::abs(support - target_feet) * 0.5f;

            SearchNode next;
            next.key = next_key;
            next.feet_y = support;
            next.g = next_g;
            next.f = next_g + heuristic;
            next.parent = item.index;
            nodes.push_back(next);
            open.push({next.f, static_cast<int>(nodes.size()) - 1});

            if (nodes.size() > kMaxSearchNodes) {
                return false;
            }
        }
    }

    return false;
}


// ============================================================================
// FlyMover
// ============================================================================

FlyMover::FlyMover() {
    flying_ = true;
    set_half_extents(Vec3f(0.3f, 0.9f, 0.3f));
    set_step_height(0.0f);
}

FlyMover::FlyMover(float body_radius, float body_height)
    : FlyMover() {
    set_half_extents(Vec3f(std::max(0.001f, body_radius),
                           std::max(0.001f, body_height * 0.5f),
                           std::max(0.001f, body_radius)));
}

bool FlyMover::find_path(World& world,
                         const Vec3i& start,
                         const Vec3i& target,
                         const Vec3f& half_extents,
                         float step_height) {
    (void)step_height;
    clear_path();

    half_extents_ = Vec3f(std::max(0.001f, half_extents.x),
                          std::max(0.001f, half_extents.y),
                          std::max(0.001f, half_extents.z));

    float start_feet = 0.0f;
    float target_feet = 0.0f;
    if (!resolve_feet_y(world, start, half_extents_, start_feet) ||
        !resolve_feet_y(world, target, half_extents_, target_feet)) {
        return false;
    }

    const NodeKey start_key = node_key(start.x, start.z, start_feet);
    const NodeKey goal_key = node_key(target.x, target.z, target_feet);

    if (start_key == goal_key) {
        path_.push_back(make_node(start.x, start.z, start_feet));
        path_length_ = 0.0f;
        return true;
    }

    std::vector<SearchNode> nodes;
    nodes.reserve(1024);
    std::unordered_map<NodeKey, float, NodeKeyHash> best;
    std::priority_queue<QueueItem, std::vector<QueueItem>,
                        std::greater<QueueItem>> open;

    SearchNode first;
    first.key = start_key;
    first.feet_y = start_feet;
    first.g = 0.0f;
    first.f = static_cast<float>(
        std::abs(start.x - target.x) +
        std::abs(start.z - target.z)) +
        std::abs(start_feet - target_feet);
    first.parent = -1;
    nodes.push_back(first);
    best[start_key] = 0.0f;
    open.push({first.f, 0});

    const int directions[6][3] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}
    };

    while (!open.empty()) {
        const QueueItem item = open.top();
        open.pop();

        if (item.index < 0 ||
            item.index >= static_cast<int>(nodes.size())) {
            continue;
        }

        const SearchNode current = nodes[static_cast<std::size_t>(item.index)];
        auto best_it = best.find(current.key);
        if (best_it == best.end() ||
            current.g > best_it->second + kEpsilon) {
            continue;
        }

        if (current.key == goal_key) {
            std::vector<PathNode> result;
            int index = item.index;
            while (index >= 0) {
                const SearchNode& node =
                    nodes[static_cast<std::size_t>(index)];
                result.push_back(
                    make_node(node.key.x, node.key.z, node.feet_y));
                index = node.parent;
            }
            std::reverse(result.begin(), result.end());
            path_ = std::move(result);
            path_length_ = compute_path_length(path_);
            return true;
        }

        for (const auto& direction : directions) {
            const int nx = current.key.x + direction[0];
            const int nz = current.key.z + direction[2];
            const float next_feet =
                current.feet_y + static_cast<float>(direction[1]);

            const Vec3f feet(static_cast<float>(nx) + 0.5f,
                             next_feet,
                             static_cast<float>(nz) + 0.5f);
            if (!GroundMover::body_free(world, feet, half_extents_)) {
                continue;
            }

            const NodeKey next_key = node_key(nx, nz, next_feet);
            const float next_g = current.g + 1.0f;

            auto next_best = best.find(next_key);
            if (next_best != best.end() &&
                next_g >= next_best->second - kEpsilon) {
                continue;
            }
            best[next_key] = next_g;

            const float heuristic =
                static_cast<float>(
                    std::abs(nx - target.x) +
                    std::abs(nz - target.z)) +
                std::abs(next_feet - target_feet);

            SearchNode next;
            next.key = next_key;
            next.feet_y = next_feet;
            next.g = next_g;
            next.f = next_g + heuristic;
            next.parent = item.index;
            nodes.push_back(next);
            open.push({next.f, static_cast<int>(nodes.size()) - 1});

            if (nodes.size() > kMaxSearchNodes) {
                return false;
            }
        }
    }

    return false;
}
