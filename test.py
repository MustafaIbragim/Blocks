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

import numpy as np


def test_renderer():
    print("=== Renderer + Input: Чистая архитектура ===")
    import renderer_module as rm
    import input_module as im
    import world_module as wm
    import block_module as bm
    import texture_module as tm
    import image_module as im_img
    import chunk_module as cm
    import vec_module as vm
    
    # ... (инициализация мира и блоков без изменений) ...
    block_mgr = bm.BlockManager()
    tex_blocks_mgr = tm.TextureBlocksManager()
    tex_mgr = tm.TextureManager()

    def make_noise_texture(base_rgb, noise_amplitude, seed):
        """16x16 тайл: базовый цвет + небольшой нейтральный (серый) шум.

        Одинаковый сдвиг добавляется ко всем каналам RGB, поэтому шум не
        вносит цветной оттенок — только колебания яркости.
        """
        rng = random.Random(seed)
        tex_img = im_img.Image(16, 16)
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

    # Земля: приглушённый коричневый + серый шум.
    dirt_img = make_noise_texture((112, 84, 58), 16, seed=1234)
    # Трава: спокойный (не кислотный) зелёный + серый шум.
    grass_img = make_noise_texture((86, 132, 64), 12, seed=5678)

    dirt_tex = tm.Texture([dirt_img], "dirt")
    grass_tex = tm.Texture([grass_img], "grass")

    # id текстуры в TextureBlocksManager одновременно является слоем
    # в GL_TEXTURE_2D_ARRAY (на одну текстуру — один слой).
    dirt_tex_id = tex_blocks_mgr.register_texture(dirt_tex)
    grass_tex_id = tex_blocks_mgr.register_texture(grass_tex)
    print(f"  Текстуры: dirt id={dirt_tex_id} (слой {dirt_tex_id}), "
          f"grass id={grass_tex_id} (слой {grass_tex_id})")

    dirt = bm.Block(0, "dirt")
    dirt.id = block_mgr.register_block(dirt)
    dirt.opacity = 255
    dirt.status = 0x01

    # Порядок граней в ChunkMesher::build_cube_faces:
    #   0 = -X, 1 = +X, 2 = -Y, 3 = +Y, 4 = -Z, 5 = +Z.
    # Земля — на всех гранях, кроме верхней; трава — только на верхней (+Y).
    for i in range(6):
        dirt.set_face_texture(i, grass_tex_id if i == 3 else dirt_tex_id)
    
    light_block_img = make_noise_texture((158, 198, 253), 2, seed=34)
    np.array(light_block_img, copy=False)[0,:] = (100, 110, 120, 255)
    np.array(light_block_img, copy=False)[15,:] = (100, 110, 120, 255)
    np.array(light_block_img, copy=False)[:,0] = (100, 110, 120, 255)
    np.array(light_block_img, copy=False)[:,15] = (100, 110, 120, 255)
    light_block_tex = tm.Texture([light_block_img], "dirt")
    
    light_block = bm.Block(1, "light_block")
    light_block.id = block_mgr.register_block(light_block)
    light_block.opacity = 255
    light_block.status = 0x01
    light_block.status = 0x01
    light_block.light = 255
    
    light_block_tex_id = tex_blocks_mgr.register_texture(light_block_tex)
    
    for i in range(6):
        light_block.set_face_texture(i, light_block_tex_id)
    
    world = wm.World(block_mgr, tex_blocks_mgr, tex_mgr)
    chunk = cm.Chunk()
    world.set_chunk(vm.Vec2i(0, 0), chunk)
    ###
    chunk_1_0 = cm.Chunk(vm.Vec2i(1, 0))
    chunk_0_1 = cm.Chunk(vm.Vec2i(0, 1))
    chunk_1_1 = cm.Chunk(vm.Vec2i(1, 1))
    world.set_chunk(vm.Vec2i(1, 0), chunk_1_0)
    world.set_chunk(vm.Vec2i(0, 1), chunk_0_1)
    world.set_chunk(vm.Vec2i(1, 1), chunk_1_1)
    ###
    for x in range(40):
        for z in range(40):
            world.set_block(vm.Vec3i(x + 10, 15, z + 10), dirt)
    
    world.set_block(vm.Vec3i(15, 17, 15), dirt)
    
    world.set_block(vm.Vec3i(12, 17, 15), dirt)
    world.set_block(vm.Vec3i(13, 17, 15), dirt)
    
    world.set_block(vm.Vec3i(15, 17, 12), dirt)
    world.set_block(vm.Vec3i(15, 17, 13), dirt)
    
    world.set_block(vm.Vec3i(25, 17, 25), light_block)
    
    print("  Задаём градиент света...")

    world.update_lighting()
    
    # Задаём свет для всех 4 чанков
    # for chunk_x in range(2):
    #     for chunk_z in range(2):
    #         chunk = world.get_chunk(vm.Vec2i(chunk_x, chunk_z))
    #         light_np = chunk.light_np
            
    #         # Обнуляем весь чанк
    #         light_np[:] = 0
            
    #         # Задаём свет для блоков платформы в этом чанке
    #         for local_x in range(32):
    #             for local_z in range(32):
    #                 # Вычисляем мировые координаты
    #                 world_x = chunk_x * 32 + local_x
    #                 world_z = chunk_z * 32 + local_z
                    
    #                 # Проверяем, что блок находится в пределах платформы (X: 10-49, Z: 10-49)
    #                 if 10 <= world_x <= 49 and 10 <= world_z <= 49:
    #                     # Центр платформы: блоки 13-32 по X и Z
    #                     if 13 <= world_x <= 32 and 13 <= world_z <= 32:
    #                         light_val = 255  # Максимальный свет
    #                     else:
    #                         light_val = 0    # Полный мрак
                        
    #                     # Используем ЛОКАЛЬНЫЕ координаты чанка
    #                     idx = cm.Chunk.coords_to_global_index(local_x, 16, local_z)
    #                     light_np[idx] = light_val
    
    # print(f"  Градиент задан для всех 4 чанков")
    
    # # Отладка: проверяем значения
    # print("  Проверка light_data:")
    # for x in [10, 13, 15, 17, 19]:
    #     for z in [10, 13, 15, 17, 19]:
    #         idx = cm.Chunk.coords_to_global_index(x, 16, z)
    #         light_val = light_np[idx]
    #         print(f"    Блок ({x}, 15, {z}): light={light_val}")
    
    # Пересобираем меш, чтобы мезер запёк новый свет в вершины!
    world.rebuild_chunk_mesh(vm.Vec2i(0, 0))
    world.rebuild_chunk_mesh(vm.Vec2i(1, 0))
    world.rebuild_chunk_mesh(vm.Vec2i(0, 1))
    world.rebuild_chunk_mesh(vm.Vec2i(1, 1))
    
    mesh = world.get_chunk_mesh(vm.Vec2i(0, 0))
    if not mesh:
        print("  Меш не построен!")
        return
    mesh_1_0 = world.get_chunk_mesh(vm.Vec2i(1, 0))
    mesh_0_1 = world.get_chunk_mesh(vm.Vec2i(0, 1))
    mesh_1_1 = world.get_chunk_mesh(vm.Vec2i(1, 1))
    
    for cx in range(2):
        for cz in range(2):
            c = world.get_chunk(vm.Vec2i(cx, cz))
            ln = c.light_np
            print(f"Чанк ({cx},{cz}): light_np адрес = {ln.ctypes.data}, shape = {ln.shape}")
    
    # 1. Создаём Рендерер
    renderer = rm.Renderer()
    renderer.init(800, 600, "Voxel Engine - Clean Architecture")
    renderer.upload_block_textures(tex_blocks_mgr)
    renderer.upload_mesh(1, mesh)
    renderer.upload_mesh(2, mesh_1_0)
    renderer.upload_mesh(3, mesh_0_1)
    renderer.upload_mesh(4, mesh_1_1)
    
    # 2. Создаём Ввод, передавая ему дескриптор окна от Рендерера
    input_mgr = im.Input(renderer.get_window_handle())
    input_mgr.set_cursor_visible(False) # Скрываем курсор
    
    # Камера
    camera = rm.ViewCam()
    camera.position = vm.Vec3f(15.0, 18.0, 25.0)
    camera.rotation = vm.Vec3r(0.0, -0.5, 0.0, False)
    camera.fov = 70.0
    
    move_speed = 8.0
    mouse_sensitivity = 0.003
    
    print("\nУправление: WASD, Мышь, Space/Shift, ESC\n")
    
    last_time = time.time()
    tick = 0
    fps_counter = 0
    fps_time = 0.0
    
    while not renderer.should_close():
        current_time = time.time()
        dt = current_time - last_time
        last_time = current_time
        
        # FPS
        fps_counter += 1
        fps_time += dt
        if fps_time >= 1.0:
            print(f"\r  FPS: {fps_counter / fps_time:.1f}", end='', flush=True)
            fps_counter = 0
            fps_time = 0.0
        
        # Обработка ввода
        input_mgr.poll_events()
        
        if input_mgr.is_key_pressed(GLFW_KEY_ESCAPE):
            break
        
        # Движение
        yaw = camera.rotation.x
        
        # Вектор "вперёд" (куда смотрит камера, проекция на горизонталь)
        forward_x = math.cos(yaw)
        forward_z = math.sin(yaw)
        
        # Вектор "вправо" (перпендикулярно forward)
        right_x = -math.sin(yaw)
        right_z = math.cos(yaw)
        
        move_x = 0.0
        move_y = 0.0
        move_z = 0.0
        
        if input_mgr.is_key_pressed(GLFW_KEY_W):
            move_x += forward_x
            move_z += forward_z
        if input_mgr.is_key_pressed(GLFW_KEY_S):
            move_x -= forward_x
            move_z -= forward_z
        if input_mgr.is_key_pressed(GLFW_KEY_A):
            move_x -= right_x
            move_z -= right_z
        if input_mgr.is_key_pressed(GLFW_KEY_D):
            move_x += right_x
            move_z += right_z
        if input_mgr.is_key_pressed(GLFW_KEY_SPACE):
            move_y += 1.0
        if input_mgr.is_key_pressed(GLFW_KEY_LEFT_SHIFT):
            move_y -= 1.0
        
        if move_x != 0.0 or move_z != 0.0:
            length = math.sqrt(move_x ** 2 + move_z ** 2)
            move_x /= length; move_z /= length
        
        camera.position.x += move_x * move_speed * dt
        camera.position.y += move_y * move_speed * dt
        camera.position.z += move_z * move_speed * dt
        
        # Поворот
        mouse_dx, mouse_dy = input_mgr.get_mouse_delta()
        camera.rotation.x += mouse_dx * mouse_sensitivity
        camera.rotation.y += mouse_dy * mouse_sensitivity
        camera.rotation.y = max(-1.5, min(1.5, camera.rotation.y))
        
        # Рендер
        renderer.render(tick, camera, 0.1)
        renderer.swap_buffers()
        renderer.poll_events()
        
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


