import math
import os
import sys

import numpy as np

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT, "python"))
sys.path.insert(0, os.path.join(ROOT, "lib"))

import vec_module as vm
import mesh_module as mm
import block_module as bm
import chunk_module as cm
import texture_module as tm
import world_module as wm
import item_module as items
import collision_module as pm
import dummy_module as dm
import mesh_lighting_module as ml


EPS = 1e-3


def close(a, b, eps=EPS):
    return abs(float(a) - float(b)) <= eps


def vec_close(a, b, eps=EPS):
    return (close(a.x, b.x, eps) and close(a.y, b.y, eps) and
            close(a.z, b.z, eps))


def make_world(chunks_x=2, floor_y=10):
    block_mgr = bm.BlockManager()
    tex_blocks_mgr = tm.TextureBlocksManager()
    tex_mgr = tm.TextureManager()
    world = wm.World(block_mgr, tex_blocks_mgr, tex_mgr)

    for cx in range(chunks_x):
        world.set_chunk(vm.Vec2i(cx, 0), cm.Chunk(vm.Vec2i(cx, 0)))

    dirt = bm.Block(0, "dirt")
    dirt.opacity = 255
    dirt.status = 1
    dirt_id = block_mgr.register_block(dirt)
    dirt = block_mgr.get_block(dirt_id)

    for cx in range(chunks_x):
        for x in range(32):
            for z in range(32):
                world.set_block(vm.Vec3i(cx * 32 + x, floor_y, z), dirt)

    # Возвращаем и менеджеры: World хранит ссылки на них, поэтому они должны
    # жить столько же, сколько World.
    return world, dirt, block_mgr, tex_blocks_mgr, tex_mgr


def make_collision_world(world, chunks_x=2, floor_y=10):
    physics = pm.CollisionWorld()
    physics.set_world(world)
    for cx in range(chunks_x):
        physics.add_chunk(world, vm.Vec2i(cx, 0))
    physics.set_gravity(vm.Vec3f(0.0, -28.0, 0.0))
    return physics


def step_n(world, count, dt=1.0 / 60.0):
    for _ in range(count):
        world.step(dt)


# ---------------------------------------------------------------------------
# 1. Чистая математика AABB
# ---------------------------------------------------------------------------
def test_aabb_math():
    a = pm.Aabb(vm.Vec3f(0.0, 0.0, 0.0), vm.Vec3f(1.0, 1.0, 1.0))
    b = pm.Aabb(vm.Vec3f(1.0, 0.0, 0.0), vm.Vec3f(2.0, 1.0, 1.0))

    assert not pm.aabb_overlap(a, b), "касание не должно считаться пересечением"
    assert pm.aabb_touch(a, b), "соседние AABB должны касаться"
    normal = pm.aabb_contact_normal(a, b)
    assert close(normal.x, -1.0), "нормаль касания направлена от b к a"

    above = pm.Aabb(vm.Vec3f(0.0, 1.0, 0.0), vm.Vec3f(1.0, 2.0, 1.0))
    up_normal = pm.aabb_contact_normal(above, a)
    assert close(up_normal.y, 1.0), "нормаль пола должна смотреть вверх"

    overlap = pm.Aabb(vm.Vec3f(0.9, 0.0, 0.0), vm.Vec3f(2.0, 1.0, 1.0))
    push = pm.aabb_penetration(a, overlap)
    assert close(push.x, -0.1), f"неверное выталкивание: {push}"

    centered = pm.Aabb.from_center_half(
        vm.Vec3f(5.0, 6.0, 7.0), vm.Vec3f(1.0, 2.0, 3.0))
    assert vec_close(centered.min, vm.Vec3f(4.0, 4.0, 4.0))
    assert vec_close(centered.max, vm.Vec3f(6.0, 8.0, 10.0))
    assert vec_close(centered.center(), vm.Vec3f(5.0, 6.0, 7.0))
    assert not pm.Aabb.invalid().valid()
    print("  AABB: overlap/touch/penetration OK")


# ---------------------------------------------------------------------------
# 2. Данные мешей: графика и физика разделены
# ---------------------------------------------------------------------------
def test_mesh_split():
    block_mgr = bm.BlockManager()

    cube = mm.Mesh()
    cube.add_vertex(0.0, 0.0, 0.0)
    cube.add_vertex(1.0, 1.0, 1.0)
    physics_mesh = pm.CollisionMesh.from_mesh(cube)
    assert physics_mesh.physics_used()
    b = physics_mesh.bounds()
    assert vec_close(b.min, vm.Vec3f(0.0, 0.0, 0.0))
    assert vec_close(b.max, vm.Vec3f(1.0, 1.0, 1.0))

    render_only = pm.CollisionMesh.from_mesh(cube, pm.MeshPurpose.RENDER_ONLY)
    assert not render_only.physics_used()
    assert not pm.block_has_collision(
        bm.Block(0, "temporary"))  # у незарегистрированного блока ещё нет меша

    big = mm.Mesh()
    big.add_vertex(0.0, 0.0, 0.0)
    big.add_vertex(3.0, 3.0, 3.0)

    block = bm.Block(0, "mesh_split")
    block.opacity = 255
    block.set_inner_mesh(big)
    block_id = block_mgr.register_block(block)
    block = block_mgr.get_block(block_id)

    # По умолчанию физика использует unit cube, а не визуальный меш.
    assert pm.block_has_collision(block)
    collision = pm.get_block_collision_mesh(block)
    assert vec_close(collision.bounds().max, vm.Vec3f(1.0, 1.0, 1.0))

    # Та же геометрия, но теперь назначение — физика.
    block.build_collision_mesh_from_inner()
    collision = pm.get_block_collision_mesh(block)
    assert vec_close(collision.bounds().max, vm.Vec3f(3.0, 3.0, 3.0))
    assert not collision.empty()

    # RenderOnly явно исключает меш из физики.
    pm.set_block_collision_mesh(block, render_only)
    assert not pm.block_has_collision(block)
    print("  Mesh split: Mesh -> CollisionMesh, RenderOnly ignored by physics")


# ---------------------------------------------------------------------------
# 3. StaticCollision, EntityCollision и пол
# ---------------------------------------------------------------------------
def test_entity_on_static():
    physics = pm.CollisionWorld()
    static_id = physics.add_static_box(
        vm.Vec3f(-20.0, -1.0, -20.0), vm.Vec3f(20.0, 0.0, 20.0))
    static_obj = physics.get_static(static_id)
    assert static_obj is not None
    assert static_obj.blocks_physics()

    entity_id = physics.add_entity(
        vm.Vec3f(0.3, 0.9, 0.3), vm.Vec3f(0.0, 5.0, 0.0), 70.0)
    step_n(physics, 300)

    body = physics.get_entity(entity_id)
    assert vec_close(body.position, vm.Vec3f(0.0, 0.9, 0.0))
    assert body.grounded, "сущность должна стоять на StaticCollision"
    assert not body.walled
    assert physics.get_num_contacts() > 0
    print("  StaticCollision: падение и устойчивое приземление OK")


# ---------------------------------------------------------------------------
# 4. Воксельный мир, стены и барьеры незагруженных чанков
# ---------------------------------------------------------------------------
def test_voxel_world_collision():
    world, dirt, block_mgr, tex_blocks_mgr, tex_mgr = make_world()
    physics = make_collision_world(world)

    entity_id = physics.add_entity(
        vm.Vec3f(0.3, 0.9, 0.3), vm.Vec3f(16.0, 20.0, 16.0), 1.0)
    step_n(physics, 300)
    body = physics.get_entity(entity_id)
    assert close(body.position.y, 11.9), f"не приземлился: {body.position}"
    assert body.grounded
    assert not body.walled

    # Стена из блоков на x=18.
    for y in range(11, 15):
        for z in range(14, 19):
            world.set_block(vm.Vec3i(18, y, z), dirt)

    physics.set_entity_position(entity_id, vm.Vec3f(16.0, 11.9, 16.0))
    body.velocity = vm.Vec3f(8.0, 0.0, 0.0)
    step_n(physics, 120)
    assert body.position.x <= 18.0 - 0.3 + EPS, \
        f"сущность прошла сквозь стену: {body.position}"
    assert body.walled, "isWalled должен сработать у стены"

    # За пределами загруженных чанков World отдаёт barrier с AABB-кубом.
    outside = world.get_block(vm.Vec3i(64, 15, 16))
    assert outside is not None and outside.name == "barrier"
    assert pm.block_has_collision(outside)

    ball_id = physics.add_entity(
        vm.Vec3f(0.4, 0.4, 0.4), vm.Vec3f(60.0, 15.0, 16.0), 1.0)
    ball = physics.get_entity(ball_id)
    ball.gravity_enabled = False
    ball.velocity = vm.Vec3f(20.0, 0.0, 0.0)
    step_n(physics, 120)
    assert ball.position.x <= 64.0 - 0.4 + EPS, \
        f"тело пересекло границу чанка: {ball.position}"
    print("  World AABB: пол, стена, chunk barrier OK")


# ---------------------------------------------------------------------------
# 5. Боковое касание стены не должно поднимать Dummy на её верх
# ---------------------------------------------------------------------------
def test_no_upward_push_on_wall_touch():
    world, dirt, block_mgr, tex_blocks_mgr, tex_mgr = make_world()
    physics = make_collision_world(world)

    # Стена на высоте головы. Dummy идёт в неё по -Z: тело касается стены
    # боком, но по Y уже перекрывает блок. Раньше последующий шаг гравитации
    # выбирал верхнюю грань блока и телепортировал тело наверх.
    world.set_block(vm.Vec3i(16, 12, 14), dirt)

    dummy = dm.Dummy(vm.Vec3f(16.0, 11.9, 20.0),
                     vm.Vec3r(0.0, 0.0, 0.0, False))
    physics.add_entity(dummy.body)

    max_y = dummy.body.position.y
    for _ in range(180):
        dummy.body.velocity = vm.Vec3f(
            0.0, dummy.body.velocity.y, -8.0)
        physics.step(1.0 / 60.0)
        max_y = max(max_y, dummy.body.position.y)

    assert max_y < 12.2, \
        f"боковое касание подбросило Dummy на стену: max_y={max_y}"
    assert close(dummy.body.position.z, 15.3, 1e-3), \
        f"Dummy должен остановиться у стены: {dummy.body.position}"
    print("  Static wall: боковое касание не телепортирует вверх OK")


# ---------------------------------------------------------------------------
# 5b. Движение вдоль стены не должно останавливаться на швах блоков
# ---------------------------------------------------------------------------
def test_wall_sliding():
    world, dirt, block_mgr, tex_blocks_mgr, tex_mgr = make_world()
    physics = make_collision_world(world)

    # Стена на x=15: именно на этой координате float-округление 15.0f + 0.3f
    # оставляло тело на волосок внутри стены. После этого каждый следующий
    # блок ряда воспринимался как препятствие по Z, и Dummy застревал на шве.
    for z in range(5, 35):
        world.set_block(vm.Vec3i(15, 11, z), dirt)

    dummy = dm.Dummy(vm.Vec3f(16.0, 11.9, 10.5),
                     vm.Vec3r(0.0, 0.0, 0.0, False))
    physics.add_entity(dummy.body)

    for _ in range(5):
        dummy.body.velocity = vm.Vec3f(0.0, dummy.body.velocity.y, 0.0)
        physics.step(1.0 / 60.0)

    max_y = dummy.body.position.y
    for _ in range(180):
        # Давим в стену по X и одновременно идём вдоль неё по Z.
        dummy.body.velocity = vm.Vec3f(-8.0, dummy.body.velocity.y, 8.0)
        physics.step(1.0 / 60.0)
        max_y = max(max_y, dummy.body.position.y)

    assert dummy.body.position.z > 30.0, \
        f"Dummy застрял на шве стены: {dummy.body.position}"
    assert max_y < 12.0, \
        f"скольжение вдоль стены подбросило Dummy: max_y={max_y}"

    # Обратное направление вдоль того же ряда.
    dummy.body.set_position(vm.Vec3f(16.0, 11.9, 29.5))
    dummy.body.velocity = vm.Vec3f(0.0, 0.0, 0.0)
    for _ in range(180):
        dummy.body.velocity = vm.Vec3f(-8.0, dummy.body.velocity.y, -8.0)
        physics.step(1.0 / 60.0)

    assert dummy.body.position.z < 10.0, \
        f"Dummy застрял при обратном скольжении: {dummy.body.position}"
    print("  Static wall: непрерывное скольжение вдоль ряда блоков OK")


# ---------------------------------------------------------------------------
# 5c. Освещение мешей сущностей по Chunk::light_data
# ---------------------------------------------------------------------------
def test_mesh_lighting():
    world, dirt, block_mgr, tex_blocks_mgr, tex_mgr = make_world()
    chunk = world.get_chunk(vm.Vec2i(0, 0))
    point = vm.Vec3f(5.5, 5.5, 5.5)

    # В темноте источников нет: меш должен стать полностью тёмным.
    assert ml.sample_vertex_light(world, point) == 0

    mesh = mm.Mesh()
    mesh.add_vertex(point)
    revision = mesh.light_revision
    assert ml.update_mesh_lighting(world, mesh, vm.Vec3f(0.0, 0.0, 0.0))
    assert mesh.vertex_light(0) == 0
    assert mesh.light_revision == revision + 1

    # Один источник отдаёт своё значение, повторный пересчёт ничего не меняет.
    chunk.set_light(5, 5, 5, 200)
    assert ml.update_mesh_lighting(world, mesh, vm.Vec3f(0.0, 0.0, 0.0))
    assert mesh.vertex_light(0) == 200
    revision = mesh.light_revision
    assert not ml.update_mesh_lighting(world, mesh, vm.Vec3f(0.0, 0.0, 0.0))
    assert mesh.light_revision == revision

    # Два источника: (200 при distance=0, 100 при distance=1), вес 1/(1+d).
    chunk.set_light(6, 5, 5, 100)
    expected_two = int((200.0 * 1.0 + 100.0 * 0.5) / 1.5)
    assert ml.sample_vertex_light(world, point) == expected_two == 166

    # Добавляем ещё источники. Учитываются только 4 ближайших, дальние
    # (x=2, x=8, x=9) не должны влиять на результат, даже если x=2
    # встречается при обходе окружения раньше ближайших клеток.
    for x, value in ((2, 250), (4, 10), (7, 20), (8, 30), (9, 40)):
        chunk.set_light(x, 5, 5, value)
    weighted = 200.0 * 1.0 + 10.0 * 0.5 + 100.0 * 0.5 + 20.0 / 3.0
    expected_four = int(weighted / (1.0 + 0.5 + 0.5 + 1.0 / 3.0))
    assert ml.sample_vertex_light(world, point) == expected_four == 112

    # origin — это мировое положение локального (0,0,0) меша.
    offset_mesh = mm.Mesh()
    offset_mesh.add_vertex(vm.Vec3f(0.0, 0.0, 0.0))
    assert ml.update_mesh_lighting(world, offset_mesh, point)
    assert offset_mesh.vertex_light(0) == expected_four

    # Источник за пределами радиуса поиска не учитывается.
    far_mesh = mm.Mesh()
    far_mesh.add_vertex(vm.Vec3f(30.5, 30.5, 30.5))
    assert ml.update_mesh_lighting(world, far_mesh, vm.Vec3f(0.0, 0.0, 0.0))
    assert far_mesh.vertex_light(0) == 0

    print("  Mesh lighting: 4 ближайших источника, среднее и revision OK")


# ---------------------------------------------------------------------------
# 6. Поведенческое разделение: Entity vs Fragment
# ---------------------------------------------------------------------------
def test_entity_fragment_split():
    world, dirt, block_mgr, tex_blocks_mgr, tex_mgr = make_world()
    physics = make_collision_world(world)

    entity_id = physics.add_entity(
        vm.Vec3f(0.5, 0.5, 0.5), vm.Vec3f(10.0, 15.0, 10.0), 1.0)
    entity = physics.get_entity(entity_id)
    entity.gravity_enabled = False
    entity.velocity = vm.Vec3f(0.0, 0.0, 0.0)

    fragment_id = physics.add_fragment(
        vm.Vec3f(0.2, 0.2, 0.2), vm.Vec3f(10.0, 18.0, 10.0), 0.1)
    fragment = physics.get_fragment(fragment_id)

    step_n(physics, 300)
    assert fragment.position.y < 14.5, \
        f"фрагмент не должен сталкиваться с сущностью: {fragment.position}"
    assert close(fragment.position.y, 11.2), \
        f"фрагмент должен лежать на статике: {fragment.position}"
    assert fragment.grounded
    assert not entity.grounded

    # Две сущности не должны оставаться в одном объёме.
    a_id = physics.add_entity(vm.Vec3f(0.5, 0.5, 0.5),
                              vm.Vec3f(20.0, 18.0, 20.0), 1.0)
    b_id = physics.add_entity(vm.Vec3f(0.5, 0.5, 0.5),
                              vm.Vec3f(20.0, 18.0, 20.0), 1.0)
    a = physics.get_entity(a_id)
    b = physics.get_entity(b_id)
    a.gravity_enabled = False
    b.gravity_enabled = False
    physics.step(1.0 / 60.0)
    assert not a.get_bounds().overlaps(b.get_bounds()), \
        "EntityCollision должны выталкивать друг друга"
    print("  Entity/Fragment: разделение поведения OK")


# ---------------------------------------------------------------------------
# 7. Триггеры: enter/exit без физического отклика
# ---------------------------------------------------------------------------
class RecordingTrigger(pm.TriggerCollision):
    def __init__(self, position, half_extents):
        super().__init__(position, half_extents, "recording")
        self.events = []

    def on_enter(self, other):
        self.events.append(("enter", other.id))

    def on_exit(self, other):
        self.events.append(("exit", other.id))


def test_trigger():
    physics = pm.CollisionWorld()
    trigger = RecordingTrigger(vm.Vec3f(0.0, 0.0, 0.0),
                               vm.Vec3f(2.0, 2.0, 2.0))
    trigger_id = physics.add_trigger(trigger)
    entity_id = physics.add_entity(
        vm.Vec3f(0.5, 0.5, 0.5), vm.Vec3f(0.0, 0.0, 0.0), 1.0)
    entity = physics.get_entity(entity_id)
    entity.gravity_enabled = False
    entity.velocity = vm.Vec3f(0.0, 0.0, 0.0)

    physics.step(1.0 / 60.0)
    assert trigger.contains_id(entity_id), "триггер должен видеть объект внутри"
    assert entity_id in trigger.entered
    assert ("enter", entity_id) in trigger.events

    # Триггер не двигает тело.
    assert vec_close(entity.position, vm.Vec3f(0.0, 0.0, 0.0))

    physics.set_entity_position(entity_id, vm.Vec3f(10.0, 0.0, 0.0))
    physics.step(1.0 / 60.0)
    assert not trigger.contains_id(entity_id)
    assert entity_id in trigger.exited
    assert ("exit", entity_id) in trigger.events

    assert physics.get_trigger(trigger_id) is trigger
    print("  TriggerCollision: enter/exit без физического отклика OK")


# ---------------------------------------------------------------------------
# 8. Отставание графики на 2 такта и особая камера Dummy
# ---------------------------------------------------------------------------
def test_interpolation():
    physics = pm.CollisionWorld()
    entity_id = physics.add_entity(
        vm.Vec3f(0.5, 0.5, 0.5), vm.Vec3f(0.0, 0.0, 0.0), 1.0)
    body = physics.get_entity(entity_id)
    body.gravity_enabled = False
    body.velocity = vm.Vec3f(1.0, 0.0, 0.0)
    body.set_interpolation_ticks(2)

    positions = []
    for _ in range(5):
        physics.step(0.1)
        # position — ссылка на поле C++; для истории сохраняем значение.
        positions.append(vm.Vec3f(body.position.x, body.position.y,
                                  body.position.z))

    delayed = body.get_render_position(0.0)
    assert vec_close(delayed, positions[-3]), \
        f"графика должна отставать на 2 такта: {delayed} != {positions[-3]}"
    mid = body.get_render_position(0.5)
    expected_mid = positions[-3] + (positions[-2] - positions[-3]) * 0.5
    assert vec_close(mid, expected_mid, 1e-4), "alpha-интерполяция не работает"

    # Dummy: X/Z камеры текущие, Y отстаёт.
    dummy = dm.Dummy(vm.Vec3f(5.0, 10.0, 5.0), vm.Vec3r(0.0, 0.0, 0.0, False))
    dummy.set_gravity_enabled(False)
    dummy.body.velocity = vm.Vec3f(0.0, -2.0, 0.0)
    dummy_id = physics.add_entity(dummy.body)
    assert dummy_id == dummy.body.id

    for _ in range(3):
        physics.step(0.1)

    current = dummy.body.position
    delayed_y = dummy.body.get_render_position(0.0).y
    dummy.sync_view_from_body(0.0)
    assert close(dummy.view.position.x, current.x)
    assert close(dummy.view.position.z, current.z)
    assert close(dummy.view.position.y, delayed_y + dm.Dummy.EYE_OFFSET)
    assert not close(dummy.view.position.y, current.y + dm.Dummy.EYE_OFFSET), \
        "Y камеры Dummy должен отставать"
    print("  Interpolation: 2 такта, Dummy camera-Y lag OK")


# ---------------------------------------------------------------------------
# 9. ItemEntity использует FragmentCollision и отстающую render_position
# ---------------------------------------------------------------------------
def test_item_physics():
    world, dirt, block_mgr, tex_blocks_mgr, tex_mgr = make_world()
    stone = items.Item("stone", None, 64)
    stone.mesh = items.Item.create_cube_mesh(0.8)
    assert stone.mesh is not None
    assert stone.mesh.triangle_count == 12
    vertices = np.asarray(stone.mesh.vertices_np)
    assert close(vertices[:, 0].min(), -0.4)
    assert close(vertices[:, 0].max(), 0.4)
    stack = items.ItemStack(stone, 1)
    entity_id = world.spawn_item_entity(
        stack, vm.Vec3f(16.0, 20.0, 16.0), vm.Vec3f(0.0, 0.0, 0.0))
    entity = world.get_item_entity(entity_id)
    assert entity is not None
    assert isinstance(entity.collision_body, pm.FragmentCollision)
    assert not isinstance(entity.collision_body, pm.EntityCollision)

    for _ in range(4):
        world.update_items(0.05)
    assert entity.render_position.y > entity.transform.position.y + 1e-4, \
        "render_position предмета должна отставать от физики"

    for _ in range(300):
        world.update_items(0.05)
    assert close(entity.transform.position.y, 11.2, 1e-2), \
        f"предмет не лёг на пол: {entity.transform.position}"

    # У каждого предмета своя копия меша, иначе освещение одного предмета
    # перезаписывало бы освещение всех остальных.
    assert entity.render_mesh is not None
    assert entity.render_mesh is not stone.mesh
    chunk = world.get_chunk(vm.Vec2i(0, 0))
    chunk.set_light(16, 11, 16, 200)
    world.update_items(0.05)
    assert entity.render_mesh.vertex_light(0) > 0, \
        "освещение меша предмета не обновилось после изменения света"

    far_id = world.spawn_item_entity(
        stack, vm.Vec3f(40.5, 11.2, 40.5), vm.Vec3f(0.0, 0.0, 0.0))
    far_entity = world.get_item_entity(far_id)
    world.update_items(0.05)
    assert far_entity.render_mesh is not entity.render_mesh
    assert far_entity.render_mesh.vertex_light(0) == 0
    print("  ItemEntity: FragmentCollision + render_position + mesh light OK")


def main():
    print("=== Collision engine: AABB, entities, fragments, triggers, meshes ===")
    test_aabb_math()
    test_mesh_split()
    test_entity_on_static()
    test_voxel_world_collision()
    test_no_upward_push_on_wall_touch()
    test_wall_sliding()
    test_mesh_lighting()
    test_entity_fragment_split()
    test_trigger()
    test_interpolation()
    test_item_physics()
    print("ALL COLLISION TESTS PASSED")


if __name__ == "__main__":
    main()
