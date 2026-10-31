import sys
import os
import math
import time
import random

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'python'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'lib'))

# Коды клавиш GLFW
GLFW_KEY_W = 87
GLFW_KEY_A = 65
GLFW_KEY_S = 83
GLFW_KEY_D = 68
GLFW_KEY_SPACE = 32
GLFW_KEY_LEFT_SHIFT = 340
GLFW_KEY_ESCAPE = 256
GLFW_KEY_U = 85
GLFW_KEY_Q = 81
GLFW_KEY_E = 69
GLFW_KEY_F6 = 295

MOUSE_BUTTON_LEFT = 0
MOUSE_BUTTON_RIGHT = 1

import numpy as np


# Полный цикл 4800 тактов (4 минуты при 20 TPS):
# 20 с рассвета, 1:40 дня, 20 с заката и 1:40 ночи.
DAY_CYCLE_TICKS = 4800
DAWN_TICKS = 400
DAY_TICKS = 2000
SUNSET_TICKS = 400
NIGHT_TICKS = 2000


def day_light_factor(tick):
    phase = tick % DAY_CYCLE_TICKS
    if phase < DAWN_TICKS:
        return phase / DAWN_TICKS
    if phase < DAWN_TICKS + DAY_TICKS:
        return 1.0
    if phase < DAWN_TICKS + DAY_TICKS + SUNSET_TICKS:
        return 1.0 - (phase - DAWN_TICKS - DAY_TICKS) / SUNSET_TICKS
    return 0.0


def test_renderer():
    print("=== Renderer + Input + Camera/Dummy + C++ Raycast ===")
    
    import vec_module as vm
    import renderer_module as rm
    import input_module as im
    import world_module as wm
    import block_module as bm
    import texture_module as tm
    import image_module as img_module
    import chunk_module as cm
    import raycast_module as rc
    import dummy_module as dm
    import camera_module as cam
    import collision_module as pm
    import item_module as items
    import light_engine_module as lem
    import movement_module as mov
    
    # ------------------------------------------------------------------
    # Быстрые проверки хранилища: NBT как uint16, обратная запись из блока
    # ------------------------------------------------------------------
    scratch_chunk = cm.Chunk()
    scratch_chunk.set_nbt(0, 0, 0, 0xABCD)
    assert scratch_chunk.get_nbt(0, 0, 0) == 0xABCD
    scratch_chunk.nuggets[0].set_nbt_by_index(1, 0x1234)
    assert scratch_chunk.nuggets[0].get_nbt_by_index(1) == 0x1234
    
    class NbtTickBlock(bm.Block):
        def on_tick(self, pos, tick, nbt=0):
            return (nbt + 1) & 0xFFFF
    
    nbt_mgr = bm.BlockManager()
    nbt_block = NbtTickBlock(0, "nbt_tick_test")
    nbt_block.id = nbt_mgr.register_block(nbt_block)
    nbt_chunk = cm.Chunk()
    nbt_chunk.set_block(0, 0, 0, nbt_block)
    nbt_chunk.set_nbt(0, 0, 0, 41)
    nbt_chunk.nuggets[0].add_tick_block(0)
    nbt_chunk.tick_all(1, nbt_mgr)
    assert nbt_chunk.get_nbt(0, 0, 0) == 42
    print("  [OK] NBT: uint16 хранится в чанке и меняется самим блоком")
    
    scratch_mgr = bm.BlockManager()
    scratch_item = items.Item("scratch", None, 64)
    scratch_inv = items.Inventory(2)
    scratch_inv.set_slot(0, items.ItemStack(scratch_item, 5))
    scratch_world = wm.World(scratch_mgr, tm.TextureBlocksManager(), tm.TextureManager())
    scratch_id = scratch_world.spawn_item_entity(
        items.ItemStack(scratch_item, 3), vm.Vec3f(0.0, 0.0, 0.0))
    item = scratch_world.get_item_entity(scratch_id)
    if isinstance(item, items.ItemEntity):
        item.pickup_delay = 0.0
    assert scratch_world.pickup_item_entity(scratch_id, scratch_inv, 0)
    assert scratch_inv.get_slot(0).count == 8
    print("  [OK] Предметы: стак возвращается в предпочтительный слот")
    
    # ------------------------------------------------------------------
    # Мир, блоки, текстуры
    # ------------------------------------------------------------------
    block_mgr = bm.BlockManager()
    tex_blocks_mgr = tm.TextureBlocksManager()
    tex_mgr = tm.TextureManager()

    def make_noise_texture(base_rgb, noise_amplitude, seed):
        """16x16 тайл: базовый цвет + небольшой нейтральный (серый) шум."""
        rng = random.Random(seed)
        tex_img = img_module.Image(16, 16)
        base_r, base_g, base_b = base_rgb
        for py in range(tex_img.height):
            for px in range(tex_img.width):
                shade = rng.randint(-noise_amplitude, noise_amplitude)
                tex_img.set_pixel(px, py, vm.Vec4u8(
                    max(0, min(255, base_r + shade)),
                    max(0, min(255, base_g + shade)),
                    max(0, min(255, base_b + shade)),
                    255,
                ))
        return tex_img

    dirt_img = make_noise_texture((112, 84, 58), 16, seed=1234)
    grass_img = make_noise_texture((86, 132, 64), 12, seed=5678)

    tex_blocks_mgr.register_texture(tm.Texture([dirt_img], "dirt"))
    tex_blocks_mgr.register_texture(tm.Texture([grass_img], "grass"))

    # Получаем id по имени, а не запоминаем порядок регистрации.
    dirt_tex_id = tex_blocks_mgr.get_texture_id_by_name("dirt")
    grass_tex_id = tex_blocks_mgr.get_texture_id_by_name("grass")
    if dirt_tex_id < 0 or grass_tex_id < 0:
        raise RuntimeError("Не удалось найти текстуры dirt/grass по имени")
    print(f"  Текстуры по имени: dirt id={dirt_tex_id}, grass id={grass_tex_id}")

    dirt = bm.Block(0, "dirt")
    dirt.id = block_mgr.register_block(dirt)
    dirt.opacity = 255
    dirt.status = 0x01

    # 0=-X, 1=+X, 2=-Y, 3=+Y, 4=-Z, 5=+Z
    for i in range(6):
        dirt.set_face_texture(i, grass_tex_id if i == 3 else dirt_tex_id)
    
    ###
    class GrassifyBlock(bm.Block):
        def on_slow_tick(self, pos, tick, nbt=0):
            light = 0
            for x in range(3):
                for z in range(3):
                    if x!=1 or z!=1:
                        block = world.get_block(vm.Vec3i(pos.x + x - 1, pos.y, pos.z + z - 1))
                        if isinstance(block, bm.Block) and block.name == "dirt":
                            c_x = pos.x >> 5
                            c_z = pos.z >> 5
                            chunk = world.get_chunk(vm.Vec2i(c_x, c_z))
                            if isinstance(chunk, cm.Chunk):
                                l_0 = chunk.get_light(pos.x - (c_x << 5), pos.y + 1, pos.z - (c_z << 5))
                                l_1 = chunk.get_sky_light(pos.x - (c_x << 5), pos.y + 1, pos.z - (c_z << 5))
                                l_2 = int(l_1 * day_light_factor(tick))
                                light = max(light, l_0, l_2)
            
            new_nbt = nbt + random.randint(light // 8, light)
            
            if new_nbt >= 128:
                world.set_block(pos, dirt)
                new_nbt = world.get_nbt(pos)
            return new_nbt
    ###
    
    grass = GrassifyBlock(0, "grass")
    grass.id = block_mgr.register_block(grass)
    grass.opacity = 255
    grass.status = 0x01
    
    for i in range(6):
        grass.set_face_texture(i, dirt_tex_id)
    
    light_block_img = make_noise_texture((158, 198, 253), 2, seed=34)
    np.array(light_block_img, copy=False)[0, :] = (100, 110, 120, 255)
    np.array(light_block_img, copy=False)[15, :] = (100, 110, 120, 255)
    np.array(light_block_img, copy=False)[:, 0] = (100, 110, 120, 255)
    np.array(light_block_img, copy=False)[:, 15] = (100, 110, 120, 255)
    tex_blocks_mgr.register_texture(tm.Texture([light_block_img], "light_block"))
    light_block_tex_id = tex_blocks_mgr.get_texture_id_by_name("light_block")

    light_block = bm.Block(1, "light_block")
    light_block.id = block_mgr.register_block(light_block)
    light_block.opacity = 255
    light_block.status = 0x01
    light_block.light = 255
    for i in range(6):
        light_block.set_face_texture(i, light_block_tex_id)

    # ------------------------------------------------------------------
    # Предмет: иконка, стак в инвентаре и HUD-текстуры цифр
    # ------------------------------------------------------------------
    ITEM_TEXTURE_ID = 100
    COUNT_TEXTURE_BASE = 10000

    # 16x16 иконка «камня»: светлая рамка и тёмная внутренняя часть.
    item_image = img_module.Image(16, 16)
    for py in range(16):
        for px in range(16):
            edge = px in (0, 15) or py in (0, 15)
            if edge:
                color = vm.Vec4u8(245, 245, 235, 255)
            else:
                shade = 12 if (px + py) % 5 == 0 else 0
                color = vm.Vec4u8(120 + shade, 128 + shade, 138 + shade, 255)
            item_image.set_pixel(px, py, color)

    stone_item = items.Item("stone", item_image, 64)
    # 3D-модель выброшенного предмета: куб 0.8 блока. Иконка выше остаётся
    # для HUD; геометрия используется только рендерером.
    stone_item.mesh = items.Item.create_cube_mesh(0.8)
    stone_item.block = dirt

    player_inventory = items.Inventory(9)
    player_inventory.set_slot(0, items.ItemStack(stone_item, 16))

    DIGIT_GLYPHS = {
        "0": ("111", "101", "101", "101", "111"),
        "1": ("010", "110", "010", "010", "111"),
        "2": ("111", "001", "111", "100", "111"),
        "3": ("111", "001", "111", "001", "111"),
        "4": ("101", "101", "111", "001", "001"),
        "5": ("111", "100", "111", "001", "111"),
        "6": ("111", "100", "111", "101", "111"),
        "7": ("111", "001", "001", "001", "001"),
        "8": ("111", "101", "111", "101", "111"),
        "9": ("111", "101", "111", "001", "111"),
    }
    count_texture_cache = {}

    def make_count_image(count):
        text = str(count)
        width = len(text) * 4 - 1
        image = img_module.Image(width, 5)
        image.fill(vm.Vec4u8(0, 0, 0, 0))
        for char_index, char in enumerate(text):
            glyph = DIGIT_GLYPHS[char]
            x_offset = char_index * 4
            for row, line in enumerate(glyph):
                for col, bit in enumerate(line):
                    if bit == "1":
                        image.set_pixel(x_offset + col, row,
                                        vm.Vec4u8(255, 255, 255, 255))
        return image

    def count_texture(renderer, count):
        if count not in count_texture_cache:
            texture_id = COUNT_TEXTURE_BASE + count
            image = make_count_image(count)
            renderer.upload_texture(texture_id, image)
            count_texture_cache[count] = (texture_id, image.width, image.height)
        return count_texture_cache[count]

    # ------------------------------------------------------------------
    # Чанки и рельеф
    # ------------------------------------------------------------------
    world = wm.World(block_mgr, tex_blocks_mgr, tex_mgr)
    world.reshuffle_slow_tick_sequence()
    Y, X = 8, 8
    for y in range(2*Y+1):
        for x in range(2*X+1):
            world.set_chunk(vm.Vec2i(y - Y, x - X), cm.Chunk(vm.Vec2i(y - Y, x - X)))
    # world.set_chunk(vm.Vec2i(0, 0), cm.Chunk())
    # world.set_chunk(vm.Vec2i(1, 0), cm.Chunk(vm.Vec2i(1, 0)))
    # world.set_chunk(vm.Vec2i(0, 1), cm.Chunk(vm.Vec2i(0, 1)))
    # world.set_chunk(vm.Vec2i(1, 1), cm.Chunk(vm.Vec2i(1, 1)))
    
    world.set_nbt(vm.Vec3i(0, 15, 0), 0xBEEF)
    assert world.get_nbt(vm.Vec3i(0, 15, 0)) == 0xBEEF
    print("  [OK] NBT: доступен через World без отдельного класса")

    # for x in range(40):
    #     for z in range(40):
    #         world.set_block(vm.Vec3i(x + 10, 15, z + 10), dirt)
    
    # for x in range(6):
    #     for z in range(10):
    #         world.set_block(vm.Vec3i(x + 27, 15, z), dirt)

    # ###
    # for x in range(64):
    #     for z in range(64):
    #         world.set_block(vm.Vec3i(x, 15, z), dirt)
    # ###
    
    ###
    for x in range(256):
        for z in range(256):
            if random.randint(0, 10) < 4:
                world.set_block(vm.Vec3i(x, 15, z), dirt)
            else:
                world.set_block(vm.Vec3i(x, 15, z), grass)
    ###
    
    ###
    for x in range(256):
        for z in range(256):
            if x==40 and z==40:
                continue
            if random.randint(0, 10) < 4:
                world.set_block(vm.Vec3i(x, 30, z), dirt)
            else:
                world.set_block(vm.Vec3i(x, 30, z), grass)
    ###
    
    # Небольшие стенки для проверки теней/границ чанков.
    world.set_block(vm.Vec3i(15, 17, 15), dirt)
    world.set_block(vm.Vec3i(12, 17, 15), dirt)
    world.set_block(vm.Vec3i(13, 17, 15), dirt)
    world.set_block(vm.Vec3i(15, 17, 12), dirt)
    world.set_block(vm.Vec3i(15, 17, 13), dirt)
    # world.set_block(vm.Vec3i(25, 17, 25), light_block)
    
    # ------------------------------------------------------------------
    # Рендерер
    # ------------------------------------------------------------------
    renderer = rm.Renderer()
    renderer.init(800, 600, "Voxel Engine - Camera/Dummy + Raycast")
    renderer.upload_block_textures(tex_blocks_mgr)
    renderer.upload_texture(ITEM_TEXTURE_ID, item_image)

    # Красная текстура для тестовой сущности-куба.
    RED_CUBE_TEXTURE_ID = 101
    red_cube_image = img_module.Image(16, 16)
    red_cube_image.fill(vm.Vec4u8(225, 35, 35, 255))
    renderer.upload_texture(RED_CUBE_TEXTURE_ID, red_cube_image)
    
    def chunk_mesh_id(chunk_pos):
        return int(chunk_pos.x + 32768) * 65536 + int(chunk_pos.y + 32768)
    
    # Мир сам пересобирает dirty-меши и отдаёт их рендереру.
    world.set_mesh_update_callback(
        lambda chunk_pos, mesh: renderer.upload_mesh(chunk_mesh_id(chunk_pos), mesh)
    )

    # Первый такт: свет + все стартовые меши (set_chunk/set_block пометили их dirty).
    world.update(1.0)

    for cx in range(2):
        for cz in range(2):
            if not world.has_chunk_mesh(vm.Vec2i(cx, cz)):
                raise RuntimeError(f"Меш чанка ({cx},{cz}) не был построен")

    # ------------------------------------------------------------------
    # Ввод, Dummy (вместо абстрактной ViewCam) и Camera
    # ------------------------------------------------------------------
    input_mgr = im.Input(renderer.get_window_handle())
    input_mgr.set_cursor_visible(False)

    dummy = dm.Dummy(
        vm.Vec3f(15.0, 17.8, 25.0),
        vm.Vec3r(0.0, -0.2, 0.0, False),
    )
    dummy.view.fov = 70.0
    dummy.step_height = 1.05
    
    # Camera хранит ViewCam и ссылку на Renderer; сам ViewCam принадлежит Dummy.
    camera = cam.Camera(renderer, dummy.view)

    directions = rc.ViewDirections(dummy.view.rotation)

    # ------------------------------------------------------------------
    # Физика: вертикальная капсула Dummy. F6 включает/выключает коллизии
    # и гравитацию.
    # ------------------------------------------------------------------
    physics = pm.CollisionWorld()
    physics.set_world(world)
    for cx in range(2):
        for cz in range(2):
            physics.add_chunk(world, vm.Vec2i(cx, cz))

    # Dummy владеет своим AABB-телом; CollisionWorld лишь регистрирует его.
    player_body = physics.add_entity(dummy.body)

    # ------------------------------------------------------------------
    # Тестовая сущность: красный куб. Раз в несколько секунд он выбирает
    # случайную клетку в квадрате вокруг себя и идёт к ней через GroundMover.
    # Для него создан отдельный CollisionWorld, чтобы движение куба не
    # зависело от того, включена ли физика игрока (F6). Меш куба поворачивается
    # в сторону движения, коллизия при этом остаётся неориентированным AABB.
    # ------------------------------------------------------------------
    def add_cube_part(mesh, center, half):
        """Добавляет к мешу ещё один куб (центр и половинные размеры)."""
        cx, cy, cz = center
        hx, hy, hz = half

        def p(x, y, z):
            return (cx + x * hx, cy + y * hy, cz + z * hz)

        def quad(a, b, c, d):
            i0 = mesh.add_vertex(*a)
            i1 = mesh.add_vertex(*b)
            i2 = mesh.add_vertex(*c)
            i3 = mesh.add_vertex(*d)
            mesh.add_triangle(i0, i1, i2)
            mesh.add_triangle(i0, i2, i3)

        quad(p(-1, -1,  1), p(-1,  1,  1), p(-1,  1, -1), p(-1, -1, -1))
        quad(p( 1, -1, -1), p( 1,  1, -1), p( 1,  1,  1), p( 1, -1,  1))
        quad(p(-1, -1, -1), p( 1, -1, -1), p( 1, -1,  1), p(-1, -1,  1))
        quad(p(-1,  1, -1), p(-1,  1,  1), p( 1,  1,  1), p( 1,  1, -1))
        quad(p(-1, -1, -1), p(-1,  1, -1), p( 1,  1, -1), p( 1, -1, -1))
        quad(p( 1, -1,  1), p( 1,  1,  1), p(-1,  1,  1), p(-1, -1,  1))

    class RedCube:
        def __init__(self, position):
            half = vm.Vec3f(0.5, 0.5, 0.5)
            self.body = pm.EntityCollision(half, position, 1.0)
            self.body.set_interpolation_ticks(2)
            self.body.drag = 0.5
            self.mover = mov.GroundMover(0.5, 1.0, 0.55)
            self.mover.half_extents = half
            # Постоянная скорость: длинный путь занимает больше времени.
            self.mover.speed = 3.0
            # Куб + небольшой «нос» вдоль локальной +Z, чтобы поворот меша
            # был виден: меш смотрит в сторону движения.
            self.mesh = items.Item.create_cube_mesh(1.0)
            add_cube_part(self.mesh, (0.0, 0.0, 0.7), (0.15, 0.15, 0.3))
            self.yaw = 0.0
            self.radius = 6
            self.step_height = 0.55
            self.next_move_time = time.time() + random.uniform(2.0, 6.0)

        def _plan_next_move(self, world, now):
            current = mov.PathMover.current_block(self.body)
            for _ in range(24):
                tx = current.x + random.randint(-self.radius, self.radius)
                tz = current.z + random.randint(-self.radius, self.radius)
                if tx == current.x and tz == current.z:
                    continue
                target = vm.Vec3i(tx, current.y, tz)
                duration = random.uniform(1.0, 2.5)
                if self.mover.move_to(world, self.body, target, duration,
                                      self.step_height):
                    self.next_move_time = now + random.uniform(4.0, 8.0)
                    return
            # Если случайная цель оказалась недостижима, попробуем снова
            # чуть позже, не задерживая физику куба.
            self.next_move_time = now + 0.5

        def update(self, world, cube_physics, now, delta_time):
            if now >= self.next_move_time and not self.mover.is_moving():
                self._plan_next_move(world, now)

            if self.mover.is_moving():
                self.mover.update(world, self.body, delta_time)

            cube_physics.step(delta_time)

            # Поворачиваем только графику: коллизия остаётся AABB.
            vx = self.body.velocity.x
            vz = self.body.velocity.z
            if vx * vx + vz * vz > 1e-6:
                # R_y(yaw) переводит локальную +Z в (sin yaw, 0, cos yaw).
                self.yaw = math.atan2(vx, vz)

    cube_physics = pm.CollisionWorld()
    cube_physics.set_world(world)
    for cx in range(2):
        for cz in range(2):
            cube_physics.add_chunk(world, vm.Vec2i(cx, cz))

    red_cube = RedCube(vm.Vec3f(25.5, 16.5, 25.5))
    cube_physics.add_entity(red_cube.body)

    physics_on = False
    dummy.set_collisions_enabled(False)
    dummy.set_gravity_enabled(False)
    dummy.body.velocity = vm.Vec3f(0.0, 0.0, 0.0)

    move_speed = 8.0
    mouse_sensitivity = 0.003
    selected_block = dirt
    selected_is_light = False

    print("\nУправление:")
    print("  WASD / Space / Shift — движение")
    print("  мышь X — поворот корпуса по Y, мышь Y — обзор (pitch)")
    print("  ЛКМ — сломать блок, ПКМ — поставить блок")
    print("  U — сменить устанавливаемый блок (dirt / light_block)")
    print("  Q — выбросить один предмет из слота 0")
    print("  E — подобрать ближайший предмет в слот 0")
    print("  F6 — включить/выключить коллизии и гравитацию")
    print("  ESC — выход\n")

    last_time = time.time()
    tick = 0
    day_tick_accum = 0.0
    fps_counter = 0
    fps_time = 0.0
    last_break = 0.0
    last_place = 0.0

    while not renderer.should_close():
        now = time.time()
        dt = min(now - last_time, 0.05)
        last_time = now
        # День/ночь идут по игровым тактам (20 TPS), а не по кадрам.
        day_tick_accum += dt * 20.0

        fps_counter += 1
        fps_time += dt
        if fps_time >= 1.0:
            print(f"\r  FPS: {fps_counter / fps_time:.1f}", end='', flush=True)
            fps_counter = 0
            fps_time = 0.0

        # Только один poll_events за кадр: Input должен видеть edge-переходы.
        input_mgr.poll_events()

        if input_mgr.is_key_pressed(GLFW_KEY_ESCAPE):
            break

        # --- Поворот: yaw корпуса отдельно от pitch взгляда ---
        mouse_dx, mouse_dy = input_mgr.get_mouse_delta()
        dummy.rotate_yaw(mouse_dx * mouse_sensitivity)
        dummy.rotate_pitch(mouse_dy * mouse_sensitivity)
        dummy.sync_view_from_transform()

        directions.set_rotation(dummy.view.rotation)

        # --- Движение в горизонтальной плоскости ---
        move = vm.Vec3f(0.0, 0.0, 0.0)
        forward = directions.create_forward_flat()
        right = directions.create_right_flat()

        if input_mgr.is_key_pressed(GLFW_KEY_W):
            move = move + forward
        if input_mgr.is_key_pressed(GLFW_KEY_S):
            move = move - forward
        if input_mgr.is_key_pressed(GLFW_KEY_D):
            move = move + right
        if input_mgr.is_key_pressed(GLFW_KEY_A):
            move = move - right
        if move.length > 1e-6:
            move = move.normalized()

        vertical = 0.0
        if input_mgr.is_key_pressed(GLFW_KEY_SPACE):
            vertical += 1.0
        if input_mgr.is_key_pressed(GLFW_KEY_LEFT_SHIFT):
            vertical -= 1.0
        
        if physics_on:
            vertical_velocity = dummy.body.velocity.y
            # Прыжок разрешён только при контакте с полом: grounded
            # выставляется физикой после проверки касания опоры.
            if vertical > 0.0 and dummy.body.grounded:
                vertical_velocity = 9.5
            elif vertical < 0.0:
                vertical_velocity = -9.5
            
            # Горизонтальное движение и шаг на ступеньку — через модуль
            # movement; вертикальная скорость остаётся за физикой.
            dummy.move_towards(
                world, move, move_speed, dt, dummy.step_height)
            dummy.body.velocity.y = vertical_velocity
            physics.step(dt)
            
            # X/Z камеры идут за физикой без задержки; Y отстаёт на 2 такта.
            dummy.sync_view_from_body(0.0)
        else:
            dummy.transform.position.x += move.x * move_speed * dt
            dummy.transform.position.y += vertical * move_speed * dt
            dummy.transform.position.z += move.z * move_speed * dt

            # Держим физическое тело в позиции Dummy, пока физика выключена.
            dummy.sync_body_from_transform()
            dummy.body.velocity = vm.Vec3f(0.0, 0.0, 0.0)
            dummy.sync_view_from_transform()

        # --- Обязательное тактовое обновление мира ---
        # Свет обновляется не чаще 20 Гц, затем автоматически пересобираются
        # только затронутые чанки и отправляются в рендерер через callback.
        world.update(dt)

        # Красный куб: собственный физический шаг и движение по A*.
        red_cube.update(world, cube_physics, now, dt)

        # --- Рейкаст и взаимодействие с блоками ---
        hit = rc.raycast_voxel(
            world,
            dummy.view.position,
            directions.create_forward(),
            8.0,
        )

        if input_mgr.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
            if now - last_break >= 0.25:
                if hit:
                    world.remove_block(hit.block)
                    print(f"\n  Блок сломан: ({hit.block.x}, {hit.block.y}, {hit.block.z})")
                last_break = now

        if input_mgr.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
            if now - last_place >= 0.25:
                if hit and (hit.normal.x or hit.normal.y or hit.normal.z):
                    place_pos = vm.Vec3i(
                        hit.block.x + hit.normal.x,
                        hit.block.y + hit.normal.y,
                        hit.block.z + hit.normal.z,
                    )

                    player_block = vm.Vec3i(
                        int(math.floor(dummy.transform.position.x)),
                        int(math.floor(dummy.transform.position.y)),
                        int(math.floor(dummy.transform.position.z)),
                    )
                    if place_pos != player_block:
                        world.set_block(place_pos, selected_block)
                        print(f"\n  Блок поставлен: ({place_pos.x}, "
                              f"{place_pos.y}, {place_pos.z})")
                last_place = now

        # --- U: выбор блока ---
        if input_mgr.is_key_just_pressed(GLFW_KEY_U):
            selected_is_light = not selected_is_light
            selected_block = light_block if selected_is_light else dirt
            print(f"\n  Устанавливаемый блок: {selected_block.name}")

        # --- Q: выбросить один предмет из слота 0 ---
        if input_mgr.is_key_just_pressed(GLFW_KEY_Q):
            slot = player_inventory.get_slot(0)
            if slot.empty() or not slot.same_item(items.ItemStack(stone_item, 1)):
                print("\n  Слот 0 пуст — нечего выбрасывать")
            else:
                dropped = player_inventory.take(0, 1)
                forward = directions.create_forward()
                drop_pos = dummy.view.position + forward * 0.9
                entity_id = world.spawn_item_entity(
                    dropped, drop_pos, forward * 1.5)
                # В тесте предмет можно подобрать сразу, без задержки.
                entity = world.get_item_entity(entity_id)
                if entity is not None:
                    entity.pickup_delay = 0.0
                print(f"\n  Выброшен предмет: осталось {player_inventory.get_slot(0).count}")

        # --- E: подобрать ближайший предмет обратно в слот 0 ---
        if input_mgr.is_key_just_pressed(GLFW_KEY_E):
            pickup_origin = dummy.transform.position + vm.Vec3f(0.0, 0.5, 0.0)
            if world.pickup_nearest_item(pickup_origin, 4.0, player_inventory, 0):
                print(f"\n  Предмет подобран: в слоте 0 теперь "
                      f"{player_inventory.get_slot(0).count}")
            else:
                print("\n  Рядом нет предметов для подбора")

        # --- F6: коллизии и гравитация ---
        if input_mgr.is_key_just_pressed(GLFW_KEY_F6):
            physics_on = not physics_on
            dummy.set_collisions_enabled(physics_on)
            dummy.set_gravity_enabled(physics_on)
            if physics_on:
                dummy.body.set_position(dummy.transform.position)
                dummy.body.velocity = vm.Vec3f(0.0, 0.0, 0.0)

            state = "ВКЛ" if physics_on else "ВЫКЛ"
            print(f"\n  Коллизии и гравитация: {state}")

        # --- Рендер ---
        camera.set_view(dummy.view)
        camera.render(tick, 0.1, day_light_factor(int(day_tick_accum)))

        # Выброшенные предметы: 3D-меш из Item, если он задан. Спрайт
        # остаётся запасным вариантом для предметов без геометрии.
        item_meshes = []
        item_sprites = []
        for entity in world.get_item_entities():
            if not entity.alive or entity.stack.empty():
                continue
            pos = entity.render_position
            item = entity.stack.item
            # World уже синхронизировал per-instance копию меша и пересчитал
            # её освещение; общий item.mesh здесь использовать нельзя.
            mesh = entity.render_mesh
            if mesh is None and item is not None:
                mesh = item.mesh
            if mesh is not None:
                offset = entity.mesh_offset
                item_meshes.append(rm.MeshInstance(
                    mesh,
                    vm.Vec3f(pos.x + offset.x,
                             pos.y + offset.y,
                             pos.z + offset.z),
                    vm.Vec3f(1.0, 1.0, 1.0),
                    ITEM_TEXTURE_ID,
                ))
            else:
                item_sprites.append(rm.Sprite(
                    vm.Vec3f(pos.x, pos.y + 0.2, pos.z),
                    entity.sprite_size,
                    ITEM_TEXTURE_ID,
                ))
        # Красный куб рисуется тем же проходом, что и предметы, но со своей
        # текстурой и геометрией.
        entity_meshes = list(item_meshes)
        entity_meshes.append(rm.MeshInstance(
            red_cube.mesh,
            red_cube.body.get_render_position(0.0),
            vm.Vec3f(1.0, 1.0, 1.0),
            RED_CUBE_TEXTURE_ID,
            vm.Vec3r(0.0, red_cube.yaw, 0.0, False),
        ))
        renderer.render_meshes(entity_meshes, dummy.view)
        renderer.render_sprites(item_sprites, dummy.view)

        # HUD: иконка предмета в слоте 0 и его количество в левом нижнем углу.
        slot = player_inventory.get_slot(0)
        if not slot.empty():
            renderer.render_screen_texture(ITEM_TEXTURE_ID, 12.0, 12.0, 48.0, 48.0)
            texture_id, texture_w, texture_h = count_texture(renderer, slot.count)
            renderer.render_screen_texture(
                texture_id, 66.0, 18.0,
                float(texture_w) * 6.0, float(texture_h) * 6.0)

        renderer.swap_buffers()

        tick += 1

    renderer.cleanup()
    print("\n\n  [OK]\n")


if __name__ == "__main__":
    try:
        test_renderer()
        print("=" * 40)
        print("TEST PASSED")
        print("=" * 40)
    except Exception as e:
        print(f"\n!!! TEST FAILED: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
