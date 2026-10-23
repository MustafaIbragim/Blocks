#include "physics_world.h"

#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <btBulletDynamicsCommon.h>

#include "chunk.h"
#include "voxel_collision_shape.h"
#include "world.h"


namespace {

inline btVector3 to_bt(const Vec3f& v) {
    return btVector3(v.x, v.y, v.z);
}

inline Vec3f to_vec(const btVector3& v) {
    return Vec3f(v.x(), v.y(), v.z());
}

} // namespace


struct PhysicsWorld::Impl {
    struct ChunkEntry {
        std::shared_ptr<VoxelCollisionShape> shape;
        btCollisionObject* object = nullptr;
    };

    struct BodyEntry {
        bool alive = false;
        btRigidBody* body = nullptr;
        btDefaultMotionState* motion_state = nullptr;
        std::shared_ptr<btCollisionShape> shape;
    };

    btDbvtBroadphase broadphase;
    btDefaultCollisionConfiguration collision_configuration;
    btCollisionDispatcher dispatcher;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld dynamics_world;

    std::unordered_map<Vec2i, ChunkEntry, Vec2iHash> chunks;
    std::vector<BodyEntry> bodies;

    Impl()
        : dispatcher(&collision_configuration),
          dynamics_world(&dispatcher, &broadphase, &solver, &collision_configuration) {
        dynamics_world.setGravity(btVector3(btScalar(0), btScalar(-9.81), btScalar(0)));
    }

    ~Impl() {
        clear_chunks();
        for (BodyEntry& entry : bodies) {
            if (!entry.alive) {
                continue;
            }
            dynamics_world.removeRigidBody(entry.body);
            delete entry.body;
            delete entry.motion_state;
            entry.shape.reset();
            entry.alive = false;
        }
    }

    void clear_chunks() {
        for (auto& item : chunks) {
            ChunkEntry& entry = item.second;
            if (entry.object) {
                dynamics_world.removeCollisionObject(entry.object);
                delete entry.object;
                entry.object = nullptr;
            }
            entry.shape.reset();
        }
        chunks.clear();
    }

    BodyEntry& get_body(uint32_t body_id) {
        if (body_id >= bodies.size() || !bodies[body_id].alive) {
            throw std::out_of_range("PhysicsWorld: invalid body id");
        }
        return bodies[body_id];
    }

    const BodyEntry& get_body(uint32_t body_id) const {
        if (body_id >= bodies.size() || !bodies[body_id].alive) {
            throw std::out_of_range("PhysicsWorld: invalid body id");
        }
        return bodies[body_id];
    }

    uint32_t add_body(std::shared_ptr<btCollisionShape> shape,
                      btScalar mass,
                      const btVector3& position) {
        btVector3 inertia(btScalar(0), btScalar(0), btScalar(0));
        if (mass > btScalar(0)) {
            shape->calculateLocalInertia(mass, inertia);
        }

        btTransform transform;
        transform.setIdentity();
        transform.setOrigin(position);

        btDefaultMotionState* motion_state = new btDefaultMotionState(transform);

        btRigidBody::btRigidBodyConstructionInfo info(
            mass, motion_state, shape.get(), inertia);
        btRigidBody* body = new btRigidBody(info);
        body->setFriction(btScalar(0.8));
        body->setRestitution(btScalar(0.0));

        dynamics_world.addRigidBody(body);

        uint32_t id = static_cast<uint32_t>(bodies.size());
        BodyEntry entry;
        entry.alive = true;
        entry.body = body;
        entry.motion_state = motion_state;
        entry.shape = std::move(shape);
        bodies.push_back(std::move(entry));
        return id;
    }
};


PhysicsWorld::PhysicsWorld() : impl(std::make_unique<Impl>()) {}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::add_chunk(World& world, const Vec2i& chunk_pos) {
    if (impl->chunks.find(chunk_pos) != impl->chunks.end()) {
        return;
    }

    auto shape = std::make_shared<VoxelCollisionShape>(world, chunk_pos);

    btCollisionObject* object = new btCollisionObject();
    object->setCollisionShape(shape.get());

    // Shape построена в локальных координатах чанка, поэтому объект
    // перемещаем в мировую позицию чанка.
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(
        btScalar(chunk_pos.x * static_cast<int32_t>(Chunk::SIZE_XZ)),
        btScalar(0),
        btScalar(chunk_pos.y * static_cast<int32_t>(Chunk::SIZE_XZ))));
    object->setWorldTransform(transform);
    object->setCollisionFlags(object->getCollisionFlags() |
                              btCollisionObject::CF_STATIC_OBJECT);

    impl->dynamics_world.addCollisionObject(
        object,
        btBroadphaseProxy::StaticFilter,
        btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter);

    Impl::ChunkEntry entry;
    entry.shape = std::move(shape);
    entry.object = object;
    impl->chunks.emplace(chunk_pos, std::move(entry));
}

void PhysicsWorld::remove_chunk(const Vec2i& chunk_pos) {
    auto it = impl->chunks.find(chunk_pos);
    if (it == impl->chunks.end()) {
        return;
    }

    Impl::ChunkEntry& entry = it->second;
    if (entry.object) {
        impl->dynamics_world.removeCollisionObject(entry.object);
        delete entry.object;
    }
    impl->chunks.erase(it);
}

bool PhysicsWorld::has_chunk(const Vec2i& chunk_pos) const {
    return impl->chunks.find(chunk_pos) != impl->chunks.end();
}

void PhysicsWorld::clear_chunks() {
    impl->clear_chunks();
}

uint32_t PhysicsWorld::add_dynamic_box(const Vec3f& half_extents,
                                       float mass,
                                       const Vec3f& position) {
    auto shape = std::make_shared<btBoxShape>(to_bt(half_extents));
    return impl->add_body(std::move(shape), btScalar(mass), to_bt(position));
}

uint32_t PhysicsWorld::add_dynamic_sphere(float radius,
                                          float mass,
                                          const Vec3f& position) {
    auto shape = std::make_shared<btSphereShape>(btScalar(radius));
    return impl->add_body(std::move(shape), btScalar(mass), to_bt(position));
}

uint32_t PhysicsWorld::add_dynamic_capsule(float radius,
                                           float cylinder_height,
                                           float mass,
                                           const Vec3f& position) {
    auto shape = std::make_shared<btCapsuleShape>(
        btScalar(radius), btScalar(cylinder_height));
    return impl->add_body(std::move(shape), btScalar(mass), to_bt(position));
}

void PhysicsWorld::remove_body(uint32_t body_id) {
    Impl::BodyEntry& entry = impl->get_body(body_id);
    impl->dynamics_world.removeRigidBody(entry.body);
    delete entry.body;
    delete entry.motion_state;
    entry.body = nullptr;
    entry.motion_state = nullptr;
    entry.shape.reset();
    entry.alive = false;
}

bool PhysicsWorld::has_body(uint32_t body_id) const {
    return body_id < impl->bodies.size() && impl->bodies[body_id].alive;
}

Vec3f PhysicsWorld::get_body_position(uint32_t body_id) const {
    const Impl::BodyEntry& entry = impl->get_body(body_id);
    return to_vec(entry.body->getWorldTransform().getOrigin());
}

Vec4f PhysicsWorld::get_body_rotation(uint32_t body_id) const {
    const Impl::BodyEntry& entry = impl->get_body(body_id);
    const btQuaternion q = entry.body->getWorldTransform().getRotation();
    return Vec4f(q.x(), q.y(), q.z(), q.w());
}

Vec3f PhysicsWorld::get_body_linear_velocity(uint32_t body_id) const {
    const Impl::BodyEntry& entry = impl->get_body(body_id);
    return to_vec(entry.body->getLinearVelocity());
}

Vec3f PhysicsWorld::get_body_gravity(uint32_t body_id) const {
    const Impl::BodyEntry& entry = impl->get_body(body_id);
    return to_vec(entry.body->getGravity());
}

void PhysicsWorld::set_body_position(uint32_t body_id, const Vec3f& position) {
    Impl::BodyEntry& entry = impl->get_body(body_id);
    btTransform transform = entry.body->getWorldTransform();
    transform.setOrigin(to_bt(position));
    entry.body->setWorldTransform(transform);
    if (entry.motion_state) {
        entry.motion_state->setWorldTransform(transform);
    }
    entry.body->activate(true);
}

void PhysicsWorld::set_body_linear_velocity(uint32_t body_id, const Vec3f& velocity) {
    Impl::BodyEntry& entry = impl->get_body(body_id);
    entry.body->setLinearVelocity(to_bt(velocity));
    entry.body->activate(true);
}

void PhysicsWorld::set_body_gravity(uint32_t body_id, const Vec3f& gravity) {
    Impl::BodyEntry& entry = impl->get_body(body_id);
    entry.body->setGravity(to_bt(gravity));
    entry.body->activate(true);
}

void PhysicsWorld::set_body_angular_factor(uint32_t body_id, const Vec3f& factor) {
    Impl::BodyEntry& entry = impl->get_body(body_id);
    entry.body->setAngularFactor(to_bt(factor));
    entry.body->activate(true);
}

Vec3f PhysicsWorld::get_body_angular_factor(uint32_t body_id) const {
    const Impl::BodyEntry& entry = impl->get_body(body_id);
    return to_vec(entry.body->getAngularFactor());
}

void PhysicsWorld::set_body_collisions_enabled(uint32_t body_id, bool enabled) {
    Impl::BodyEntry& entry = impl->get_body(body_id);
    int flags = entry.body->getCollisionFlags();
    if (enabled) {
        flags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
    } else {
        flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
    }
    entry.body->setCollisionFlags(flags);
    entry.body->activate(true);
}

bool PhysicsWorld::get_body_collisions_enabled(uint32_t body_id) const {
    const Impl::BodyEntry& entry = impl->get_body(body_id);
    return (entry.body->getCollisionFlags() &
            btCollisionObject::CF_NO_CONTACT_RESPONSE) == 0;
}

void PhysicsWorld::activate_body(uint32_t body_id) {
    Impl::BodyEntry& entry = impl->get_body(body_id);
    entry.body->activate(true);
}

void PhysicsWorld::set_gravity(const Vec3f& gravity) {
    impl->dynamics_world.setGravity(to_bt(gravity));
}

void PhysicsWorld::step(float delta_time,
                        int max_substeps,
                        float fixed_time_step) {
    if (delta_time <= 0.0f) {
        return;
    }
    impl->dynamics_world.stepSimulation(
        btScalar(delta_time), max_substeps, btScalar(fixed_time_step));
}

int PhysicsWorld::get_num_collision_objects() const {
    return impl->dynamics_world.getNumCollisionObjects();
}

int PhysicsWorld::get_num_manifolds() const {
    return impl->dispatcher.getNumManifolds();
}

btDiscreteDynamicsWorld* PhysicsWorld::get_native_world() const {
    return const_cast<btDiscreteDynamicsWorld*>(&impl->dynamics_world);
}
