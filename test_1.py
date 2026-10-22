import sys
import os
import math

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'python'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'lib'))

import vec_module as vec
import block_module as blk
import chunk_module as chk
import world_module as wrd
import texture_module as tex
import image_module as img
import raycaster_module as ray

print("=== Тест: сетка 16x16 лучей ===\n")

# Создаём мир
world = wrd.World()
textures = tex.TextureManager()

# Создаём текстуру (красный цвет)
test_img = img.Image(16, 16)
for y in range(16):
    for x in range(16):
        test_img.set_pixel(x, y, vec.Vec3u8(255, 0, 0))

test_tex = tex.Texture([test_img], "red")
test_tex_id = textures.register_texture(test_tex)

# Создаём блок
test_block = blk.Block(1, "test_block")
test_block.status = 1
for i in range(6):
    test_block.set_face_texture(i, test_tex_id)

# Создаём чанк и размещаем блок
chunk = chk.Chunk(vec.Vec2i(0, 0))
chunk.set_block(5, 10, 10, test_block)
world.set_chunk(vec.Vec2i(0, 0), chunk)

print("Блок размещён на (5, 10, 10)")

# Параметры камеры
cam_pos = vec.Vec3f(0.5, 10.5, 10.5)
yaw_rad = math.pi / 2  # Смотрим вдоль оси X
pitch_rad = 0.0
fov_degrees = 60.0

# Вычисляем базовые векторы камеры
cos_yaw = math.cos(yaw_rad)
sin_yaw = math.sin(yaw_rad)
cos_pitch = math.cos(pitch_rad)
sin_pitch = math.sin(pitch_rad)

forward = vec.Vec3f(sin_yaw * cos_pitch, sin_pitch, cos_yaw * cos_pitch)
right = vec.Vec3f(cos_yaw, 0.0, -sin_yaw)
up = vec.Vec3f(0.0, 1.0, 0.0)

fov_scale = math.tan(math.radians(fov_degrees / 2.0))

print(f"Камера: ({cam_pos.x}, {cam_pos.y}, {cam_pos.z})")
print(f"Forward: ({forward.x:.3f}, {forward.y:.3f}, {forward.z:.3f})")
print(f"Right: ({right.x:.3f}, {right.y:.3f}, {right.z:.3f})")
print(f"FOV scale: {fov_scale:.3f}")

# Генерируем сетку 16x16 лучей
width, height = 16, 16
results = []

print(f"\nГенерация {width}x{height} лучей...")

for py in range(height):
    row = []
    for px in range(width):
        # Нормализованные координаты от -1 до 1
        x_norm = (2.0 * px / width) - 1.0
        y_norm = 1.0 - (2.0 * py / height)  # Инвертируем Y
        
        # Вычисляем направление луча
        dir_x = forward.x + right.x * (x_norm * fov_scale) + up.x * (y_norm * fov_scale)
        dir_y = forward.y + right.y * (x_norm * fov_scale) + up.y * (y_norm * fov_scale)
        dir_z = forward.z + right.z * (x_norm * fov_scale) + up.z * (y_norm * fov_scale)
        
        # Нормализуем направление
        length = math.sqrt(dir_x**2 + dir_y**2 + dir_z**2)
        direction = vec.Vec3f(dir_x / length, dir_y / length, dir_z / length)
        
        # Создаём луч
        ray_obj = ray.Ray(cam_pos, direction, world, 1.0)
        
        # Трассируем луч
        hit_found = False
        for _ in range(20):  # Максимум 20 шагов
            ray_obj.step(world)
            
            if ray_obj.get_distance() > 32.0:
                break
            
            if ray_obj.is_out_of_bounds(512):
                break
            
            hit = ray_obj.check_hit(textures, 0, vec.Vec3u8(128, 128, 128))
            if hit.hit:
                hit_found = True
                break
        
        row.append('#' if hit_found else '.')
    results.append(row)

# Выводим результат
print("\nРезультат трассировки (вид сверху):")
print("  # = попадание, . = промах")
print()

for row in results:
    print(''.join(row))

# Подсчитываем попадания
total_hits = sum(row.count('#') for row in results)
total_rays = width * height
print(f"\nПопаданий: {total_hits}/{total_rays} ({total_hits/total_rays*100:.1f}%)")


