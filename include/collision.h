#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "mesh.h"
#include "vec.h"


class World;
class Entity;
class CollisionObject;
class StaticCollision;
class EntityCollision;
class FragmentCollision;
class TriggerCollision;


// ============================================================================
// AABB: единственная геометрическая форма физического движка.
// ============================================================================
//
// Все тела в движке выровнены по осям и не вращаются. Это позволяет
// обрабатывать столкновения очень простыми и предсказуемыми функциями:
// пересечение, касание и выталкивание.
struct Aabb {
    Vec3f min;
    Vec3f max;

    Aabb();
    Aabb(const Vec3f& min, const Vec3f& max);

    static Aabb from_center_half(const Vec3f& center, const Vec3f& half_extents);
    static Aabb invalid();

    bool valid() const;
    Vec3f center() const;
    Vec3f half_extents() const;

    // Строгое пересечение объёмов. Касание гранями пересечением не считается.
    bool overlaps(const Aabb& other) const;

    // Касание гранями/рёбрами без проникновения. При касании возвращает
    // нормаль, направленную от other к this (например, (0, 1, 0), когда this
    // стоит на other).
    bool touches(const Aabb& other,
                 Vec3f* normal_out = nullptr,
                 float epsilon = 1e-4f) const;

    // Минимальный вектор, который нужно прибавить к this, чтобы убрать
    // строгое пересечение с other. Для непересекающихся тел — (0, 0, 0).
    Vec3f penetration(const Aabb& other) const;

    // Вспомогательные операции для broadphase и построения коллизий.
    Aabb translated(const Vec3f& delta) const;
    Aabb expanded(float amount) const;
    Aabb expanded(const Vec3f& amount) const;
};

bool aabb_overlap(const Aabb& a, const Aabb& b);
bool aabb_touch(const Aabb& a, const Aabb& b, float epsilon = 1e-4f);
Vec3f aabb_contact_normal(const Aabb& a, const Aabb& b,
                          float epsilon = 1e-4f);
Vec3f aabb_penetration(const Aabb& moving, const Aabb& other);


// ============================================================================
// Данные мешей для физики
// ============================================================================
//
// Меши разделены на два назначения:
//   * Mesh (mesh.h) — геометрия для графики; физика её не читает;
//   * CollisionMesh — набор AABB, который читает физика.
// Геометрия может быть одной и той же (CollisionMesh::from_mesh), меняется
// только назначение. MeshPurpose::RenderOnly позволяет хранить такие данные,
// но явно исключить их из физики.
enum class MeshPurpose : uint8_t {
    Physics = 0,
    RenderOnly = 1,
};

class CollisionMesh {
public:
    // AABB в локальных координатах блока/объекта.
    std::vector<Aabb> boxes;
    MeshPurpose purpose = MeshPurpose::Physics;

    CollisionMesh() = default;
    explicit CollisionMesh(MeshPurpose purpose) : purpose(purpose) {}

    bool physics_used() const { return purpose == MeshPurpose::Physics; }
    bool empty() const { return boxes.empty(); }

    void add_box(const Aabb& box);
    void add_box(const Vec3f& min, const Vec3f& max);
    void add_box_center(const Vec3f& center, const Vec3f& half_extents);
    void add_unit_cube();
    void clear();

    Aabb bounds() const;
    Aabb world_bounds(const Vec3f& origin) const;

    // Пересекающиеся с query мировые AABB. origin — позиция локальной
    // системы координат меша в мире.
    void collect_world_boxes(const Vec3f& origin,
                             const Aabb& query,
                             std::vector<Aabb>& out) const;

    static std::shared_ptr<CollisionMesh> unit_cube();
    static std::shared_ptr<CollisionMesh> box(const Aabb& local_box,
                                              MeshPurpose purpose = MeshPurpose::Physics);
    static std::shared_ptr<CollisionMesh> box(const Vec3f& min,
                                              const Vec3f& max,
                                              MeshPurpose purpose = MeshPurpose::Physics);

    // Строит collision-меш из тех же вершин, что используются графикой.
    // Для стандартного куба результат совпадает с unit_cube().
    static std::shared_ptr<CollisionMesh> from_mesh(
        const Mesh& mesh,
        MeshPurpose purpose = MeshPurpose::Physics);
};


// ============================================================================
// Иерархия физических объектов
// ============================================================================

enum class CollisionType : uint8_t {
    Static = 0,
    Entity = 1,
    Fragment = 2,
    Trigger = 3,
};

class CollisionObject {
public:
    uint32_t id = 0;
    bool enabled = true;

    virtual ~CollisionObject() = default;

    virtual CollisionType type() const = 0;
    virtual Aabb get_bounds() const = 0;

    bool is_static() const { return type() == CollisionType::Static; }
    bool is_entity() const { return type() == CollisionType::Entity; }
    bool is_fragment() const { return type() == CollisionType::Fragment; }
    bool is_trigger() const { return type() == CollisionType::Trigger; }
};


// Статичное препятствие: никогда не двигается, не имеет скорости и массы.
class StaticCollision : public CollisionObject {
public:
    Vec3f position;
    std::shared_ptr<CollisionMesh> mesh;

    StaticCollision();
    StaticCollision(const Vec3f& position,
                    std::shared_ptr<CollisionMesh> mesh);

    CollisionType type() const override { return CollisionType::Static; }
    Aabb get_bounds() const override;

    bool blocks_physics() const;
    void collect_boxes(const Aabb& query, std::vector<Aabb>& out) const;
};


// Общая часть EntityCollision и FragmentCollision. Не является отдельным
// видом поведения: type() задаётся наследниками.
class DynamicCollision : public CollisionObject {
public:
    // position — центр AABB (как у капсулы Dummy в старом API).
    Vec3f position;
    Vec3f half_extents = Vec3f(0.5f, 0.5f, 0.5f);
    Vec3f velocity;
    float mass = 1.0f;

    // Коэффициент квадратичного сопротивления (drag * v * |v|). Если задан
    // owner, используется Entity::drag, иначе это локальное значение.
    float drag = 0.0f;

    // Необязательная game-сущность, которой принадлежит тело (Dummy,
    // ItemEntity). Позволяет физике читать актуальный Entity::drag без
    // ручной синхронизации полей на каждом кадре.
    Entity* owner = nullptr;

    // Действующий коэффициент сопротивления с учётом owner.
    float get_drag() const;

    bool collisions_enabled = true;
    bool gravity_enabled = true;

    // Результаты последнего шага, используемые игровой логикой.
    bool grounded = false;
    bool walled = false;

    // Сколько физических тактов отстаёт графика. 0 — без отставания.
    int interpolation_ticks = 2;

    DynamicCollision();
    virtual ~DynamicCollision() = default;

    Aabb get_bounds() const override;
    Aabb get_swept_bounds(const Vec3f& delta) const;

    // Меняет позицию и сбрасывает историю интерполяции. Это телепорт,
    // а не обычное движение.
    void set_position(const Vec3f& value, bool reset_history = true);
    void set_half_extents(const Vec3f& value);

    // История хранит позиции на физических тактах. Графика читает
    // get_render_position(), отставая на interpolation_ticks тактов.
    void record_history();
    void reset_history();
    void set_interpolation_ticks(int value);
    int history_size() const {
        return static_cast<int>(history_.size());
    }

    // alpha = 0 — ровно задержанное состояние; alpha = 1 — следующее за ним.
    // Так можно интерполировать между двумя физическими тактами.
    Vec3f get_render_position(float alpha = 0.0f) const;

    float inverse_mass() const {
        return mass > 1e-8f ? 1.0f / mass : 0.0f;
    }

private:
    std::deque<Vec3f> history_;
};


// Полноценная динамическая сущность (игрок, моб). Сталкивается со статикой
// и с другими EntityCollision, но не с FragmentCollision.
class EntityCollision : public DynamicCollision {
public:
    EntityCollision();
    EntityCollision(const Vec3f& half_extents,
                    const Vec3f& position,
                    float mass = 1.0f);

    CollisionType type() const override { return CollisionType::Entity; }
};


// Лёгкая динамика (частицы, выпавшие предметы). Сталкивается только со
// статичным миром; сущности и другие фрагменты для неё не существуют.
class FragmentCollision : public DynamicCollision {
public:
    // Пользовательское значение (например, id item-entity). Физика его
    // не интерпретирует, но триггеры могут использовать для связи.
    uint32_t source_id = 0;

    FragmentCollision();
    FragmentCollision(const Vec3f& half_extents,
                      const Vec3f& position,
                      float mass = 0.1f);

    CollisionType type() const override { return CollisionType::Fragment; }
};


// Триггер имеет границы, но не создаёт физического препятствия. Он лишь
// сообщает о вхождении и выходе динамических объектов.
class TriggerCollision : public CollisionObject {
public:
    Vec3f position;
    Vec3f half_extents = Vec3f(0.5f, 0.5f, 0.5f);
    std::string name;

    // Объекты, находящиеся внутри на текущем такте.
    std::unordered_set<uint32_t> inside;
    // События последнего шага.
    std::vector<uint32_t> entered;
    std::vector<uint32_t> exited;

    TriggerCollision();
    TriggerCollision(const Vec3f& position, const Vec3f& half_extents,
                     const std::string& name = "");

    CollisionType type() const override { return CollisionType::Trigger; }
    Aabb get_bounds() const override;

    bool contains_id(uint32_t object_id) const {
        return inside.find(object_id) != inside.end();
    }

    // Наследники (в том числе из Python) могут реагировать на события.
    virtual void on_enter(CollisionObject& other);
    virtual void on_exit(CollisionObject& other);
};


// ============================================================================
// Мир коллизий
// ============================================================================

class CollisionWorld {
public:
    CollisionWorld();
    ~CollisionWorld();

    CollisionWorld(const CollisionWorld&) = delete;
    CollisionWorld& operator=(const CollisionWorld&) = delete;

    // Воксельный мир, из которого читается статика. add_chunk оставлен как
    // явная регистрация загруженного чанка (удобно для тестов и внешнего
    // кода); сама геометрия всегда читается из актуального World.
    void set_world(World& world);
    void clear_world();
    void add_chunk(World& world, const Vec2i& chunk_pos);
    void remove_chunk(const Vec2i& chunk_pos);
    bool has_chunk(const Vec2i& chunk_pos) const;
    void clear_chunks();

    // Статичные объекты.
    uint32_t add_static(const std::shared_ptr<StaticCollision>& object);
    uint32_t add_static_box(const Aabb& box);
    uint32_t add_static_box(const Vec3f& min, const Vec3f& max);
    StaticCollision* get_static(uint32_t object_id);
    bool remove_static(uint32_t object_id);
    void clear_statics();

    // Сущности. add_entity(EntityCollision&) позволяет подключить уже
    // существующее тело (например, Dummy::body) без копирования.
    uint32_t add_entity(const Vec3f& half_extents,
                        const Vec3f& position,
                        float mass = 1.0f);
    uint32_t add_entity(EntityCollision& body);
    EntityCollision* get_entity(uint32_t object_id);
    bool remove_entity(uint32_t object_id);
    void clear_entities();

    // Фрагменты. Игнорируют сущности и друг друга, сталкиваются только
    // со статикой.
    uint32_t add_fragment(const Vec3f& half_extents,
                          const Vec3f& position,
                          float mass = 0.1f);
    uint32_t add_fragment(FragmentCollision& body);
    FragmentCollision* get_fragment(uint32_t object_id);
    bool remove_fragment(uint32_t object_id);
    void clear_fragments();

    // Триггеры.
    uint32_t add_trigger(const std::shared_ptr<TriggerCollision>& object);
    uint32_t add_trigger_box(const Vec3f& position,
                             const Vec3f& half_extents,
                             const std::string& name = "");
    TriggerCollision* get_trigger(uint32_t object_id);
    bool remove_trigger(uint32_t object_id);
    void clear_triggers();

    void clear_all();

    void set_gravity(const Vec3f& gravity) { gravity_ = gravity; }
    Vec3f get_gravity() const { return gravity_; }

    // Основной шаг физики. delta_time ограничивается 0.1 с, внутри движение
    // дробится на подшаги, чтобы быстрые объекты не проходили сквозь блоки.
    // max_substeps/fixed_time_step оставлены для совместимости с вызовами
    // старого API; реальное дробление выбирается автоматически.
    void step(float delta_time,
              int max_substeps = 8,
              float fixed_time_step = 1.0f / 60.0f);

    // Только фрагменты (например, если World обновляет item-entity отдельно
    // от игровых сущностей).
    void step_fragments(float delta_time);

    int get_num_collision_objects() const;
    int get_num_contacts() const { return num_contacts_; }
    int get_num_manifolds() const { return num_contacts_; }

    // Удобные аксессоры по id (для игровой логики и старых тестовых сценариев).
    Vec3f get_entity_position(uint32_t object_id) const;
    Vec3f get_entity_render_position(uint32_t object_id,
                                     float alpha = 0.0f) const;
    Vec3f get_entity_velocity(uint32_t object_id) const;
    bool is_entity_grounded(uint32_t object_id) const;
    bool is_entity_walled(uint32_t object_id) const;
    void set_entity_position(uint32_t object_id, const Vec3f& position);

private:
    World* world_ = nullptr;
    Vec3f gravity_ = Vec3f(0.0f, -28.0f, 0.0f);

    std::vector<std::shared_ptr<StaticCollision>> statics_;
    std::vector<std::unique_ptr<EntityCollision>> entities_;
    std::vector<EntityCollision*> external_entities_;
    std::vector<std::unique_ptr<FragmentCollision>> fragments_;
    std::vector<FragmentCollision*> external_fragments_;
    std::vector<std::shared_ptr<TriggerCollision>> triggers_;

    std::unordered_set<uint64_t> chunks_;
    uint32_t next_id_ = 1;
    int num_contacts_ = 0;

    uint32_t assign_id(CollisionObject& object);
    void resolve_entity_pairs();
    void resolve_static_overlap(DynamicCollision& body);
    void refresh_contacts(DynamicCollision& body);
    void update_triggers();
    void reset_trigger_events();
};


// Отдельный шаг фрагмента в воксельном мире. Нужен World::update_items,
// чтобы item-entity не требовали владения целым CollisionWorld.
void step_fragment_against_world(World& world,
                                 FragmentCollision& body,
                                 float delta_time,
                                 const Vec3f& gravity = Vec3f(0.0f, -20.0f, 0.0f));
