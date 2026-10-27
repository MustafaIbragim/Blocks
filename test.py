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


def test_renderer():
    print("=== Renderer + Input + Camera/Dummy + C++ Raycast ===")

    import renderer_module as rm
    import input_module as im
    import world_module as wm
    import block_module as bm
    import texture_module as tm
    import image_module as img_module
    import chunk_module as cm
    import vec_module as vm
    import raycast_module as rc
    import dummy_module as dm
    import camera_module as cam
    import collision_module as pm
    import item_module as items

    # ------------------------------------------------------------------
    # Быстрые проверки хранилища: NBT как uint16, обратная запись из блока
    # ------------------------------------------------------------------
    scratch_chunk = cm.Chunk()
    scratch_chunk.set_nbt(0, 0, 0, 0xABCD)
    assert scratch_chunk.get_nbt(0, 0, 0) == 0xABCD
    scratch_chunk.nuggets[0].set_nbt_by_index(1, 0x1234)
    assert scratch_chunk.nuggets[0].get_nbt_by_index(1) == 0x1234

    class NbtTickBlock(bm.Block):
        def on_tick(self, pos, tick, nbt):
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
    scratch_world.get_item_entity(scratch_id).pickup_delay = 0.0
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
    world.set_chunk(vm.Vec2i(0, 0), cm.Chunk())
    world.set_chunk(vm.Vec2i(1, 0), cm.Chunk(vm.Vec2i(1, 0)))
    world.set_chunk(vm.Vec2i(0, 1), cm.Chunk(vm.Vec2i(0, 1)))
    world.set_chunk(vm.Vec2i(1, 1), cm.Chunk(vm.Vec2i(1, 1)))

    world.set_nbt(vm.Vec3i(0, 15, 0), 0xBEEF)
    assert world.get_nbt(vm.Vec3i(0, 15, 0)) == 0xBEEF
    print("  [OK] NBT: доступен через World без отдельного класса")

    for x in range(40):
        for z in range(40):
            world.set_block(vm.Vec3i(x + 10, 15, z + 10), dirt)
    
    for x in range(6):
        for z in range(10):
            world.set_block(vm.Vec3i(x + 27, 15, z), dirt)

    ###
    for x in range(64):
        for z in range(64):
            world.set_block(vm.Vec3i(x, 15, z), dirt)
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

    def chunk_mesh_id(chunk_pos):
        return int(chunk_pos.x) * 2 + int(chunk_pos.y) + 1

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
        vm.Vec3f(15.0, 16.9, 25.0),
        vm.Vec3r(0.0, -0.2, 0.0, False),
    )
    dummy.view.fov = 70.0

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
    fps_counter = 0
    fps_time = 0.0
    last_break = 0.0
    last_place = 0.0

    while not renderer.should_close():
        now = time.time()
        dt = min(now - last_time, 0.05)
        last_time = now

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
            if vertical > 0.0:
                vertical_velocity = 9.5
            elif vertical < 0.0:
                vertical_velocity = -9.5

            dummy.body.velocity = vm.Vec3f(
                move.x * move_speed,
                vertical_velocity,
                move.z * move_speed,
            )
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
        camera.render(tick, 0.1)

        # Спрайты выброшенных предметов.
        item_sprites = []
        for entity in world.get_item_entities():
            if not entity.alive or entity.stack.empty():
                continue
            pos = entity.render_position
            item_sprites.append(rm.Sprite(
                vm.Vec3f(pos.x, pos.y + 0.2, pos.z),
                entity.sprite_size,
                ITEM_TEXTURE_ID,
            ))
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
