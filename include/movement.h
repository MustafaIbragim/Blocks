#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "vec.h"


class World;
class DynamicCollision;


// Узел пути. block — клетка, в которой лежат ступни сущности
// (floor(feet_y)); feet_y — точная мировая высота верхней опоры. Для
// полного блока-пола сама опора находится в block.y - 1, а для плиты или
// ступеньки — в той же клетке block.y.
struct PathNode {
    Vec3i block;
    float feet_y = 0.0f;

    PathNode() = default;
    PathNode(const Vec3i& block, float feet_y)
        : block(block), feet_y(feet_y) {}

    Vec3f feet_position() const {
        return Vec3f(static_cast<float>(block.x) + 0.5f,
                     feet_y,
                     static_cast<float>(block.z) + 0.5f);
    }

    Vec3f body_center(const Vec3f& half_extents) const {
        return feet_position() + Vec3f(0.0f, half_extents.y, 0.0f);
    }

    bool operator==(const PathNode& other) const {
        return block == other.block && feet_y == other.feet_y;
    }
    bool operator!=(const PathNode& other) const { return !(*this == other); }
};


// Общая часть наземного и воздушного перемещения.
//
// Сначала find_path() проверяет проходимость и строит путь. Только если
// путь существует, move_to() выдаёт команду движения. update() каждый кадр
// задаёт скорость тела вдоль пути; фактическое перемещение и разрешение
// коллизий остаются за CollisionWorld / step_fragment_against_world.
class PathMover {
public:
    virtual ~PathMover();

    // Ядро поиска пути. start/target — клетки, в которых находятся ступни.
    // half_extents — размеры AABB сущности. step_height — максимальная
    // высота, на которую разрешено подняться за один шаг (для полёта
    // игнорируется).
    virtual bool find_path(World& world,
                           const Vec3i& start,
                           const Vec3i& target,
                           const Vec3f& half_extents,
                           float step_height) = 0;

    // Удобные перегрузки, использующие сохранённые размеры/высоту шага.
    bool find_path(World& world, const Vec3i& start, const Vec3i& target);
    bool find_path(World& world, const Vec3i& start, const Vec3i& target,
                   float step_height);
    bool find_path(World& world, const Vec3i& start, const Vec3i& target,
                   const Vec3f& half_extents);

    // Найти путь и, если он есть, начать движение из start в target за
    // duration секунд. snap_to_start=true телепортирует тело в центр
    // стартового узла (это гарантирует, что путь начинается ровно там, где
    // рассчитывался). step_height=nullopt — использовать сохранённое
    // значение.
    bool move_to(World& world,
                 DynamicCollision& body,
                 const Vec3i& start,
                 const Vec3i& target,
                 float duration,
                 std::optional<float> step_height = std::nullopt,
                 bool snap_to_start = true);

    // То же, но старт берётся из текущей позиции тела.
    bool move_to(World& world,
                 DynamicCollision& body,
                 const Vec3i& target,
                 float duration,
                 std::optional<float> step_height = std::nullopt,
                 bool snap_to_start = true);

    // Продвинуть движение на delta_time. Возвращает true, пока путь не
    // закончен. Тело нужно затем передать в физический шаг.
    bool update(World& world, DynamicCollision& body, float delta_time);

    void stop();
    void clear_path();

    bool has_path() const { return !path_.empty(); }
    bool is_moving() const { return moving_; }
    std::size_t current_index() const { return path_index_; }

    std::vector<PathNode> get_path() const { return path_; }
    std::vector<Vec3i> get_path_blocks() const;
    PathNode current_node() const;
    PathNode target_node() const;

    float path_length() const { return path_length_; }
    float speed() const { return speed_; }
    void set_speed(float value);
    float arrival_radius() const { return arrival_radius_; }
    void set_arrival_radius(float value);

    float step_height() const { return step_height_; }
    void set_step_height(float value);

    Vec3f half_extents() const { return half_extents_; }
    void set_half_extents(const Vec3f& value);

    // Клетка ступней по текущей позиции физического тела.
    static Vec3i current_block(const DynamicCollision& body);

protected:
    std::vector<PathNode> path_;
    std::size_t path_index_ = 0;
    bool moving_ = false;
    bool flying_ = false;
    float speed_ = 4.0f;
    float arrival_radius_ = 0.08f;
    float path_length_ = 0.0f;
    float step_height_ = 0.6f;
    Vec3f half_extents_ = Vec3f(0.3f, 0.9f, 0.3f);

    // Для полёта гравитация на время движения отключается, а при остановке
    // восстанавливается. Указатель нужен только на время движения.
    DynamicCollision* controlled_body_ = nullptr;
    bool gravity_saved_ = false;
    bool saved_gravity_enabled_ = true;

    void start_path(DynamicCollision& body, float duration, bool snap_to_start);
    bool finish_path(DynamicCollision& body);
    void restore_gravity();
};


// Наземное перемещение: 4-направленный A* по клеткам, шаг вверх не больше
// step_height, шаг вниз — до 8 блоков (падение). Проходимость проверяется
// по реальным CollisionMesh блоков, а не только по opacity/status.
class GroundMover : public PathMover {
public:
    GroundMover();
    explicit GroundMover(float step_height);
    GroundMover(float body_radius, float body_height,
                float step_height = 0.6f);

    using PathMover::find_path;

    bool find_path(World& world,
                   const Vec3i& start,
                   const Vec3i& target,
                   const Vec3f& half_extents,
                   float step_height) override;

    // Проверяет, что AABB сущности с заданными ступнями не пересекает
    // статичные коллизии мира.
    static bool body_free(World& world,
                          const Vec3f& feet_position,
                          const Vec3f& half_extents);

    // Ищет максимальную высоту опоры под ступнями в клетке (x, z), не выше
    // max_y и не ниже max_y - max_drop. Возвращает false, если опоры нет.
    static bool support_height(World& world,
                               int x,
                               int z,
                               const Vec3f& half_extents,
                               float max_y,
                               float max_drop,
                               float& out_height);

    // Стандартное «забирание» на ступеньку: если горизонтальный шаг
    // desired_delta упирается в препятствие, пробует поднять тело на
    // минимальную высоту до step_height, при которой и текущее, и желаемое
    // положение свободны. Меняет body.position.y при успехе.
    static bool try_step_up(World& world,
                            DynamicCollision& body,
                            const Vec3f& desired_delta,
                            float step_height,
                            float* lifted = nullptr);
};


// Полёт: 6-направленный A* в объёме, без гравитации и ступенек.
class FlyMover : public PathMover {
public:
    FlyMover();
    FlyMover(float body_radius, float body_height);

    using PathMover::find_path;

    bool find_path(World& world,
                   const Vec3i& start,
                   const Vec3i& target,
                   const Vec3f& half_extents,
                   float step_height) override;
};
