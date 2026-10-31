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
import chunk_mesher_module as cmm
import movement_module as mov


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

    dummy = dm.Dummy(vm.Vec3f(16.0, 12.8, 20.0),
                     vm.Vec3r(0.0, 0.0, 0.0, False))
    physics.add_entity(dummy.body)

    max_y = dummy.body.position.y
    for _ in range(180):
        dummy.body.velocity = vm.Vec3f(
            0.0, dummy.body.velocity.y, -8.0)
        physics.step(1.0 / 60.0)
        max_y = max(max_y, dummy.body.position.y)

    assert max_y < 13.0, \
        f"боковое касание подбросило Dummy на стену: max_y={max_y}"
    assert close(dummy.body.position.z, 15.6, 1e-3), \
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

    dummy = dm.Dummy(vm.Vec3f(16.7, 12.8, 10.5),
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
    assert max_y < 13.0, \
        f"скольжение вдоль стены подбросило Dummy: max_y={max_y}"

    # Обратное направление вдоль того же ряда.
    dummy.body.set_position(vm.Vec3f(16.7, 12.8, 29.5))
    dummy.body.velocity = vm.Vec3f(0.0, 0.0, 0.0)
    for _ in range(180):
        dummy.body.velocity = vm.Vec3f(-8.0, dummy.body.velocity.y, -8.0)
        physics.step(1.0 / 60.0)

    assert dummy.body.position.z < 10.0, \
        f"Dummy застрял при обратном скольжении: {dummy.body.position}"
    print("  Static wall: непрерывное скольжение вдоль ряда блоков OK")


# ---------------------------------------------------------------------------
# 5b. Частичное обновление блоков: общая перестановка индексов World
# ---------------------------------------------------------------------------
def test_slow_tick():
    block_mgr = bm.BlockManager()
    tex_blocks_mgr = tm.TextureBlocksManager()
    tex_mgr = tm.TextureManager()
    world = wm.World(block_mgr, tex_blocks_mgr, tex_mgr)

    for cx in range(2):
        for cz in range(2):
            world.set_chunk(vm.Vec2i(cx, cz), cm.Chunk(vm.Vec2i(cx, cz)))

    sequence = np.asarray(world.slow_tick_sequence_np)
    expected = np.arange(world.SLOW_TICK_BLOCK_COUNT, dtype=np.uint32)
    assert sequence.shape == expected.shape
    assert sequence.dtype == np.uint32
    assert np.array_equal(np.sort(sequence), expected), \
        "slow_tick_sequence_np должен быть перестановкой всех индексов"

    grass = bm.Block(0, "grass")
    grass.status = 1
    grass.opacity = 255
    grass.id = block_mgr.register_block(grass)

    calls = []

    class GrassifyBlock(bm.Block):
        def on_slow_tick(self, pos, tick, nbt=0):
            calls.append((pos.x, pos.y, pos.z, tick, nbt))
            # Рекурсивный вызов из Python-подкласса обратно в World.
            neighbor = world.get_block(vm.Vec3i(pos.x + 1, pos.y, pos.z))
            if neighbor is not None and neighbor.name == "grass":
                world.set_block(pos, grass)
            return (nbt + 1) & 0xFFFF

    target = GrassifyBlock(0, "grassify")
    target.status = 1
    target.opacity = 255
    target.id = block_mgr.register_block(target)

    target_pos = vm.Vec3i(5, 20, 5)
    world.set_block(target_pos, target)
    world.set_block(vm.Vec3i(6, 20, 5), grass)

    target_index = cm.Chunk.coords_to_global_index(5, 20, 5)
    sequence[:world.SLOW_TICK_BATCH_SIZE] = target_index
    world.slow_tick_cursor = 0
    world.slow_tick(123)

    assert calls and calls[0][:4] == (5, 20, 5, 123)
    assert world.get_block(target_pos).name == "grass"
    assert world.get_nbt(target_pos) == 1
    assert world.slow_tick_cursor == world.SLOW_TICK_BATCH_SIZE

    # Тот же механизм должен запускаться автоматическим тактом World.
    second_pos = vm.Vec3i(7, 20, 7)
    world.set_block(second_pos, target)
    world.set_block(vm.Vec3i(8, 20, 7), grass)

    second_index = cm.Chunk.coords_to_global_index(7, 20, 7)
    sequence[:world.SLOW_TICK_BATCH_SIZE] = second_index
    world.slow_tick_cursor = 0
    world.update(1.0)

    assert world.get_block(second_pos).name == "grass"
    assert world.slow_tick_cursor == world.SLOW_TICK_BATCH_SIZE
    assert len(calls) == 2

    # Конец массива: следующий вызов пересобирает перестановку и начинает
    # цикл с нуля.
    world.slow_tick_cursor = (world.SLOW_TICK_BLOCK_COUNT -
                              world.SLOW_TICK_BATCH_SIZE)
    world.slow_tick(0)
    assert world.slow_tick_cursor == world.SLOW_TICK_BLOCK_COUNT
    world.slow_tick(0)
    assert world.slow_tick_cursor == world.SLOW_TICK_BATCH_SIZE
    assert np.array_equal(np.sort(sequence), expected)

    print("  Slow tick: перестановка, 32 индекса/такт и Python-блок OK")


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
# 5c2. FaceData: цвета по умолчанию и цвета углов в меше
# ---------------------------------------------------------------------------
def test_face_data_colors():
    face = vm.FaceData()
    for channel in ("r", "g", "b"):
        for corner in range(3):
            assert getattr(face, f"{channel}_{corner}") == 255, \
                "цвета FaceData по умолчанию должны быть белыми"

    face.r_0 = 10
    face.g_1 = 20
    face.b_2 = 30
    face.layer = 7
    face.anim_data = 3

    mesh = mm.Mesh()
    mesh.add_vertex(vm.Vec3f(0.0, 0.0, 0.0))
    mesh.add_vertex(vm.Vec3f(1.0, 0.0, 0.0))
    mesh.add_vertex(vm.Vec3f(0.0, 1.0, 0.0))
    mesh.add_triangle(0, 1, 2, face)

    colors = np.asarray(mesh.face_colors_np)
    assert colors.shape == (1, 9), f"неожиданная форма цветов: {colors.shape}"
    assert colors[0, 0] == 10
    assert colors[0, 4] == 20
    assert colors[0, 8] == 30
    assert colors[0, 1] == 255

    faces = np.asarray(mesh.faces_np)
    assert faces.shape == (1, 8)
    assert int(faces[0, 6]) == 7
    assert int(faces[0, 7]) == 3
    print("  FaceData: белые по умолчанию, цвета углов и faces_np OK")


# ---------------------------------------------------------------------------
# 5d. Небесный свет: прямое распространение вниз и затухание на блоках
# ---------------------------------------------------------------------------
def test_sky_light():
    block_mgr = bm.BlockManager()
    tex_blocks_mgr = tm.TextureBlocksManager()
    tex_mgr = tm.TextureManager()
    world = wm.World(block_mgr, tex_blocks_mgr, tex_mgr)
    world.set_chunk(vm.Vec2i(0, 0), cm.Chunk(vm.Vec2i(0, 0)))
    chunk = world.get_chunk(vm.Vec2i(0, 0))

    # В открытом воздухе свет неба не затухает по высоте.
    assert chunk.get_sky_light(5, 0, 5) == 255
    assert chunk.get_sky_light(5, 20, 5) == 255
    assert chunk.get_sky_light(5, 511, 5) == 255

    opaque = bm.Block(0, "opaque")
    opaque.opacity = 255
    opaque_id = block_mgr.register_block(opaque)
    opaque = block_mgr.get_block(opaque_id)

    world.set_block(vm.Vec3i(5, 20, 5), opaque)
    world.update_lighting()
    assert chunk.get_sky_light(5, 20, 5) == 0, \
        "непрозрачный блок должен полностью гасить свет неба"
    assert chunk.get_sky_light(5, 21, 5) == 255

    world.remove_block(vm.Vec3i(5, 20, 5))
    world.update_lighting()
    assert chunk.get_sky_light(5, 20, 5) == 255, \
        "после удаления блока свет неба должен восстановиться"
    assert chunk.get_sky_light(5, 19, 5) == 255

    # Окружаем центральный столбец непрозрачными стенами на всю высоту,
    # чтобы проверить именно прямое затухание, без боковой подсветки.
    for y in range(21):
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                if dx == 0 and dz == 0:
                    continue
                world.set_block(vm.Vec3i(5 + dx, y, 5 + dz), opaque)

    glass = bm.Block(0, "glass")
    glass.opacity = 100
    glass_id = block_mgr.register_block(glass)
    glass = block_mgr.get_block(glass_id)

    world.set_block(vm.Vec3i(5, 20, 5), glass)
    world.update_lighting()
    assert chunk.get_sky_light(5, 20, 5) == 155, \
        "полупрозрачный блок должен ослабить свет на свою opacity"
    assert chunk.get_sky_light(5, 19, 5) == 155, \
        "свет неба под полупрозрачным блоком не должен затухать по высоте"

    world.set_block(vm.Vec3i(5, 19, 5), glass)
    world.update_lighting()
    assert chunk.get_sky_light(5, 19, 5) == 55, \
        "opacity нескольких блоков должна суммироваться"

    # В открытом столбце боковой свет тоже должен вычитать opacity самой
    # клетки, иначе полупрозрачный блок пересвечивается до 255 - opacity.
    world.set_block(vm.Vec3i(15, 20, 15), glass)
    world.set_block(vm.Vec3i(15, 19, 15), glass)
    world.update_lighting()
    assert chunk.get_sky_light(15, 20, 15) == 155
    assert chunk.get_sky_light(15, 19, 15) == 147, \
        "боковой свет в полупрозрачный блок должен вычитать его opacity"

    # Отложенное увеличение света отменяется, если до update() над клеткой
    # успели поставить непрозрачный блок. Иначе старый queue-уровень
    # восстановил бы прямой свет неба сквозь новую крышу.
    world.set_block(vm.Vec3i(20, 20, 20), opaque)
    world.update_lighting()
    world.remove_block(vm.Vec3i(20, 20, 20))
    world.set_block(vm.Vec3i(20, 21, 20), opaque)
    world.update_lighting()
    assert chunk.get_sky_light(20, 21, 20) == 0
    assert chunk.get_sky_light(20, 20, 20) == 247, \
        "отложенный прямой свет не должен проходить сквозь новую крышу"
    print("  Sky light: 255 в воздухе, гашение и полупрозрачные блоки OK")


# ---------------------------------------------------------------------------
# 5d2. Освещение вершин меша чанка на стыках соседних чанков
# ---------------------------------------------------------------------------
def test_chunk_border_lighting():
    block_mgr = bm.BlockManager()
    tex_blocks_mgr = tm.TextureBlocksManager()
    tex_mgr = tm.TextureManager()
    world = wm.World(block_mgr, tex_blocks_mgr, tex_mgr)

    for cx in range(2):
        for cz in range(2):
            world.set_chunk(vm.Vec2i(cx, cz), cm.Chunk(vm.Vec2i(cx, cz)))

    dirt = bm.Block(0, "dirt")
    dirt.opacity = 255
    dirt.status = 1
    dirt_id = block_mgr.register_block(dirt)
    dirt = block_mgr.get_block(dirt_id)

    # Блок у +X границы чанка (0,0) и блок в его углу. Их грани смотрят в
    # соседние чанки, а вершины должны читать свет из этих чанков.
    world.set_block(vm.Vec3i(31, 10, 16), dirt)
    world.set_block(vm.Vec3i(31, 10, 31), dirt)

    chunk = world.get_chunk(vm.Vec2i(0, 0))
    right = world.get_chunk(vm.Vec2i(1, 0))
    diagonal = world.get_chunk(vm.Vec2i(1, 1))

    # По одному источнику в соседнем и в диагональном чанке. Вершина на
    # границе усредняет 8 клеток, поэтому одна ненулевая даёт 200 / 8 = 25.
    right.set_light(0, 10, 16, 200)
    diagonal.set_light(0, 10, 0, 200)

    mesh = cmm.ChunkMesher.build_mesh(chunk, block_mgr, tex_blocks_mgr)
    vertices = np.asarray(mesh.vertices_np)

    def vertex_lights_at(x, y, z):
        matches = np.all(np.isclose(vertices[:, :3], (x, y, z), atol=EPS),
                         axis=1)
        indices = np.where(matches)[0]
        assert len(indices) > 0, f"нет вершины ({x}, {y}, {z})"
        return [mesh.vertex_light(int(i)) for i in indices]

    adjacent_lights = vertex_lights_at(32, 10, 16)
    assert adjacent_lights and all(light == 25 for light in adjacent_lights), \
        f"свет соседнего чанка не учтён: {adjacent_lights}"

    diagonal_lights = vertex_lights_at(32, 10, 32)
    assert diagonal_lights and all(light == 25 for light in diagonal_lights), \
        f"свет диагонального чанка не учтён: {diagonal_lights}"

    print("  Chunk border lighting: соседний и диагональный чанки OK")


def test_chunk_light_dirty_propagation():
    block_mgr = bm.BlockManager()
    tex_blocks_mgr = tm.TextureBlocksManager()
    tex_mgr = tm.TextureManager()
    world = wm.World(block_mgr, tex_blocks_mgr, tex_mgr)

    for cx in range(2):
        for cz in range(2):
            world.set_chunk(vm.Vec2i(cx, cz), cm.Chunk(vm.Vec2i(cx, cz)))

    dirt = bm.Block(0, "dirt")
    dirt.opacity = 255
    dirt.status = 1
    dirt_id = block_mgr.register_block(dirt)
    dirt = block_mgr.get_block(dirt_id)
    world.set_block(vm.Vec3i(31, 10, 16), dirt)
    world.set_block(vm.Vec3i(31, 10, 31), dirt)

    # Сбрасываем начальные dirty-флаги, чтобы дальше проверять только
    # реакцию на изменение света.
    world.rebuild_dirty_meshes()
    assert not world.is_chunk_dirty(vm.Vec2i(0, 0))
    assert not world.is_chunk_dirty(vm.Vec2i(1, 0))

    torch = bm.Block(1, "torch")
    torch.opacity = 0
    torch.status = 0
    torch.light = 20
    torch_id = block_mgr.register_block(torch)
    torch = block_mgr.get_block(torch_id)

    # Источник далеко от границы с (0,0): сам соседний чанк не меняет свет,
    # но его меш обязан пересобраться, потому что читает границу (1,0).
    world.set_block(vm.Vec3i(63, 10, 18), torch)
    world.update_lighting()

    assert world.is_chunk_dirty(vm.Vec2i(1, 0))
    assert world.is_chunk_dirty(vm.Vec2i(0, 0)), \
        "изменение света не пометило соседний чанк dirty"
    assert world.is_chunk_dirty(vm.Vec2i(0, 1)), \
        "изменение света не пометило диагональный чанк dirty"

    world.rebuild_dirty_meshes()
    assert not world.is_chunk_dirty(vm.Vec2i(0, 0))
    assert not world.is_chunk_dirty(vm.Vec2i(1, 0))
    assert not world.is_chunk_dirty(vm.Vec2i(0, 1))

    print("  Chunk lighting: соседние меши помечаются dirty OK")


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


# ---------------------------------------------------------------------------
# 9b. Квадратичное сопротивление (drag) замедляет падение
# ---------------------------------------------------------------------------
def test_vertical_drag():
    physics = pm.CollisionWorld()
    free_id = physics.add_entity(
        vm.Vec3f(0.5, 0.5, 0.5), vm.Vec3f(0.0, 20.0, 0.0), 1.0)
    drag_id = physics.add_entity(
        vm.Vec3f(0.5, 0.5, 0.5), vm.Vec3f(10.0, 20.0, 0.0), 1.0)
    free_body = physics.get_entity(free_id)
    drag_body = physics.get_entity(drag_id)
    drag_body.drag = 0.5
    assert close(drag_body.effective_drag, 0.5)

    for _ in range(60):
        physics.step(1.0 / 60.0)

    assert drag_body.velocity.y > free_body.velocity.y + 1.0, \
        f"drag не замедлил падение: {drag_body.velocity.y} vs " \
        f"{free_body.velocity.y}"

    # У Dummy drag хранится в Entity; физическое тело должно читать его
    # через owner без ручной синхронизации.
    entity_physics = pm.CollisionWorld()
    dummy = dm.Dummy(vm.Vec3f(0.0, 20.0, 0.0),
                     vm.Vec3r(0.0, 0.0, 0.0, False))
    dummy.drag = 0.5
    entity_physics.add_entity(dummy.body)
    for _ in range(60):
        entity_physics.step(1.0 / 60.0)
    assert close(dummy.body.effective_drag, 0.5)
    assert close(dummy.body.velocity.y, drag_body.velocity.y, 1e-4), \
        "Entity::drag не дошёл до физического тела"
    print("  Drag: квадратичное сопротивление и Entity.drag OK")


# ---------------------------------------------------------------------------
# 9c. Сглаживание пути и постоянная скорость движения
# ---------------------------------------------------------------------------
def test_straight_line_movement():
    world, dirt, block_mgr, tex_blocks_mgr, tex_mgr = make_world(chunks_x=1)
    physics = make_collision_world(world, chunks_x=1)

    # Один блок = 1.0 мировой единицы = 0.5 м. Манекен — 3.6 единицы.
    assert close(dm.Dummy.BODY_HEIGHT, 3.6), \
        f"неверная высота Dummy: {dm.Dummy.BODY_HEIGHT}"
    assert close(dm.Dummy.BODY_RADIUS, 0.6), \
        f"неверный радиус Dummy: {dm.Dummy.BODY_RADIUS}"

    dummy = dm.Dummy(vm.Vec3f(10.5, 12.8, 10.5),
                     vm.Vec3r(0.0, 0.0, 0.0, False))
    physics.add_entity(dummy.body)

    start = vm.Vec3i(10, 11, 10)
    target = vm.Vec3i(14, 11, 14)
    dummy.mover.speed = 3.0
    assert dummy.move_to(world, start, target, 0.05, 0.6), \
        "Dummy не принял диагональный путь"
    assert close(dummy.mover.speed, 3.0), \
        "duration не должен пересчитывать скорость по длине пути"

    prev = vm.Vec3f(dummy.body.position.x,
                    dummy.body.position.y,
                    dummy.body.position.z)
    diagonal = False
    for _ in range(60):
        dummy.update_movement(world, 1.0 / 60.0)
        physics.step(1.0 / 60.0)
        pos = dummy.body.position
        dx = pos.x - prev.x
        dz = pos.z - prev.z
        if abs(dx) > 1e-5 and abs(dz) > 1e-5:
            diagonal = True
        prev = vm.Vec3f(pos.x, pos.y, pos.z)

    assert diagonal, "Dummy должен идти по диагонали, а не по осям X/Z"
    distance = math.hypot(prev.x - 10.5, prev.z - 10.5)
    assert close(distance, 3.0, 0.2), \
        f"скорость не постоянная: за 1 с пройдено {distance}"

    for _ in range(180):
        dummy.update_movement(world, 1.0 / 60.0)
        physics.step(1.0 / 60.0)

    assert close(dummy.body.position.x, 14.5, 0.2), \
        f"Dummy не дошёл до цели по X: {dummy.body.position}"
    assert close(dummy.body.position.z, 14.5, 0.2), \
        f"Dummy не дошёл до цели по Z: {dummy.body.position}"
    assert not dummy.is_moving()
    print("  Movement: прямая диагональ и постоянная скорость OK")


# ---------------------------------------------------------------------------
# 10. Movement: поиск пути A -> B, ступеньки, полёт и item-entity
# ---------------------------------------------------------------------------
def test_movement_module():
    start = vm.Vec3i(14, 11, 16)
    target = vm.Vec3i(18, 11, 16)
    half = vm.Vec3f(0.3, 0.9, 0.3)

    # Стена на всю загруженную ширину по Z: обойти её нельзя, можно только
    # перешагнуть. Без достаточной высоты шага пути быть не должно.
    world, dirt, block_mgr, tex_blocks_mgr, tex_mgr = make_world()
    for z in range(32):
        world.set_block(vm.Vec3i(16, 11, z), dirt)

    low = mov.GroundMover(0.3, 1.8, 0.6)
    assert not low.find_path(world, start, target, half, 0.6), \
        "наземный путь не должен находиться сквозь стену при шаге 0.6"
    assert len(low.path) == 0

    high = mov.GroundMover(0.3, 1.8, 1.0)
    assert high.find_path(world, start, target, half, 1.0), \
        "путь через ступеньку высотой 1.0 не найден"
    assert len(high.path) >= 3
    assert high.path[0].block.x == start.x
    assert high.path[-1].block.x == target.x
    assert any(close(node.feet_y, 12.0) for node in high.path), \
        "путь должен содержать узел на верхушке стены"
    assert high.path_blocks, "path_blocks должен возвращать клетки пути"

    # Dummy идёт по этому же пути и реально забирается на стену.
    physics = make_collision_world(world)
    dummy = dm.Dummy(vm.Vec3f(14.5, 12.8, 16.5),
                     vm.Vec3r(0.0, 0.0, 0.0, False))
    physics.add_entity(dummy.body)
    assert dummy.dynamic
    assert close(dummy.speed, 8.0)
    assert close(dummy.drag, 0.0)
    dummy.update(1, 9.8)
    assert close(dummy.gravity, 9.8)

    assert dummy.move_to(world, start, target, 2.0, 1.0), \
        "Dummy::move_to не принял проходимый путь"
    max_y = dummy.body.position.y
    for _ in range(180):
        dummy.update_movement(world, 1.0 / 60.0)
        physics.step(1.0 / 60.0)
        dummy.sync_view_from_body(0.0)
        max_y = max(max_y, dummy.body.position.y)

    assert max_y > 13.6, f"Dummy не поднялся на стену: max_y={max_y}"
    assert close(dummy.body.position.x, 18.5, 0.2), \
        f"Dummy не дошёл до цели: {dummy.body.position}"
    assert close(dummy.body.position.y, 12.8, 0.2), \
        f"Dummy не спустился с стены: {dummy.body.position}"
    assert not dummy.is_moving()

    # Полёт: тот же A -> B, но по объёму и с временным отключением гравитации.
    fly_world, fly_dirt, fly_block_mgr, fly_tex_blocks_mgr, fly_tex_mgr = \
        make_world()
    for z in range(32):
        fly_world.set_block(vm.Vec3i(16, 11, z), fly_dirt)
    fly_physics = make_collision_world(fly_world)
    fly = mov.FlyMover(0.3, 1.8)
    assert fly.find_path(fly_world, start, target, half, 0.0), \
        "воздушный путь не найден"
    assert max(node.feet_y for node in fly.path) >= 12.0

    fly_dummy = dm.Dummy(vm.Vec3f(14.5, 12.8, 16.5),
                         vm.Vec3r(0.0, 0.0, 0.0, False))
    fly_physics.add_entity(fly_dummy.body)
    assert fly.move_to(fly_world, fly_dummy.body, start, target, 2.0, 0.0)
    assert not fly_dummy.body.gravity_enabled, \
        "на время полёта гравитация должна отключаться"

    for _ in range(180):
        fly.update(fly_world, fly_dummy.body, 1.0 / 60.0)
        fly_physics.step(1.0 / 60.0)
        fly_dummy.sync_view_from_body(0.0)

    assert close(fly_dummy.body.position.x, 18.5, 0.2), \
        f"летящий Dummy не дошёл до цели: {fly_dummy.body.position}"
    assert close(fly_dummy.body.position.y, 12.8, 0.2), \
        f"летящий Dummy не опустился: {fly_dummy.body.position}"
    assert fly_dummy.body.gravity_enabled, \
        "после полёта гравитация должна восстановиться"
    assert not fly.is_moving()

    # ItemEntity использует тот же GroundMover и тот же World::update_items.
    item_world, item_dirt, item_block_mgr, item_tex_blocks_mgr, item_tex_mgr = \
        make_world()
    stone = items.Item("stone", None, 64)
    stack = items.ItemStack(stone, 1)
    entity_id = item_world.spawn_item_entity(
        stack, vm.Vec3f(14.5, 11.2, 16.5), vm.Vec3f(0.0, 0.0, 0.0))
    item_entity = item_world.get_item_entity(entity_id)
    assert item_entity is not None
    assert item_entity.move_to(item_world, start, target, 2.0, 0.6), \
        "item-entity не принял проходимый путь"

    for _ in range(180):
        item_world.update_items(1.0 / 60.0)

    assert close(item_entity.transform.position.x, 18.5, 0.2), \
        f"item-entity не дошёл до цели: {item_entity.transform.position}"
    assert close(item_entity.transform.position.y, 11.2, 0.2), \
        f"item-entity не остался на полу: {item_entity.transform.position}"
    assert not item_entity.is_moving()

    print("  Movement: ground step-up, fly, item path A->B OK")


def main():
    print("=== Collision engine: AABB, entities, fragments, triggers, meshes ===")
    test_aabb_math()
    test_mesh_split()
    test_entity_on_static()
    test_voxel_world_collision()
    test_no_upward_push_on_wall_touch()
    test_wall_sliding()
    test_slow_tick()
    test_mesh_lighting()
    test_face_data_colors()
    test_sky_light()
    test_chunk_border_lighting()
    test_chunk_light_dirty_propagation()
    test_entity_fragment_split()
    test_trigger()
    test_interpolation()
    test_item_physics()
    test_vertical_drag()
    test_straight_line_movement()
    test_movement_module()
    print("ALL COLLISION TESTS PASSED")


if __name__ == "__main__":
    main()
