import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT, "python"))
sys.path.insert(0, os.path.join(ROOT, "lib"))

import vec_module as vm
import block_module as bm
import chunk_module as cm
import texture_module as tm
import world_module as wm
import physics_module as pm


def main():
    block_mgr = bm.BlockManager()
    tex_blocks_mgr = tm.TextureBlocksManager()
    tex_mgr = tm.TextureManager()
    world = wm.World(block_mgr, tex_blocks_mgr, tex_mgr)

    # Два чанка рядом: проверяем и позиционирование shape, и работу на границе.
    for cx in range(2):
        world.set_chunk(vm.Vec2i(cx, 0), cm.Chunk(vm.Vec2i(cx, 0)))

    dirt = bm.Block(0, "dirt")
    dirt.opacity = 255
    dirt.status = 0x01
    dirt_id = block_mgr.register_block(dirt)

    floor_y = 10
    for cx in range(2):
        for x in range(32):
            for z in range(32):
                world.set_block(vm.Vec3i(cx * 32 + x, floor_y, z), dirt)

    physics = pm.PhysicsWorld()
    physics.add_chunk(world, vm.Vec2i(0, 0))
    physics.add_chunk(world, vm.Vec2i(1, 0))
    assert physics.has_chunk(vm.Vec2i(0, 0))
    assert physics.has_chunk(vm.Vec2i(1, 0))

    # Сфера падает на пол в первом чанке.
    sphere_id = physics.add_dynamic_sphere(
        0.5, 1.0, vm.Vec3f(16.0, 20.0, 16.0))
    # Куб падает на пол во втором чанке (проверка transform чанка).
    box_id = physics.add_dynamic_box(
        vm.Vec3f(0.5, 0.5, 0.5), 1.0, vm.Vec3f(40.0, 22.0, 16.0))
    # Сфера ровно над границей чанков: collision shape обоих чанков должна
    # работать согласованно.
    edge_id = physics.add_dynamic_sphere(
        0.5, 1.0, vm.Vec3f(31.5, 24.0, 16.0))
    # Вертикальная капсула (как корпус Dummy) с заблокированным вращением.
    capsule_id = physics.add_dynamic_capsule(
        0.3, 1.2, 1.0, vm.Vec3f(10.0, 22.0, 16.0))
    physics.set_body_angular_factor(capsule_id, vm.Vec3f(0.0, 0.0, 0.0))
    assert physics.get_body_angular_factor(capsule_id) == vm.Vec3f(0.0, 0.0, 0.0)

    assert physics.get_num_collision_objects() >= 5

    saw_contacts = False
    for _ in range(360):
        physics.step(1.0 / 60.0, 1, 1.0 / 60.0)
        if physics.get_num_manifolds() > 0:
            saw_contacts = True

    sphere_pos = physics.get_body_position(sphere_id)
    box_pos = physics.get_body_position(box_id)
    edge_pos = physics.get_body_position(edge_id)
    capsule_pos = physics.get_body_position(capsule_id)
    capsule_rot = physics.get_body_rotation(capsule_id)

    print("sphere:", sphere_pos)
    print("box:   ", box_pos)
    print("edge:  ", edge_pos)
    print("capsule:", capsule_pos)
    print("manifolds were generated:", saw_contacts)

    assert saw_contacts, "Bullet did not generate any contacts"
    for name, pos in (("sphere", sphere_pos), ("box", box_pos), ("edge", edge_pos)):
        assert 11.2 < pos.y < 11.9, f"{name} did not rest on the voxel floor: {pos}"
        assert abs(pos.x) < 1e6 and abs(pos.z) < 1e6

    # Полная высота капсулы 1.8, значит центр покоится на 11 + 0.9 = 11.9.
    assert 11.7 < capsule_pos.y < 11.95, (
        f"capsule did not rest on the voxel floor: {capsule_pos}")
    # Angular factor (0, 0, 0) не даёт капсуле наклоняться.
    assert abs(capsule_rot.w - 1.0) < 1e-3, (
        f"capsule body rotated unexpectedly: {capsule_rot}")

    # Проверяем flyweight: все блоки одного типа используют один shape.
    first = pm.get_block_collision(dirt)
    second = pm.get_block_collision(dirt)
    assert first is not None
    assert first is second, "block collision shape must be shared (flyweight)"

    print("physics test: OK")


if __name__ == "__main__":
    main()
