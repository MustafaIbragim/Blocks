import sys
import os
import math
import time
import numpy as np
import pygame
from pygame.locals import *

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'python'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'lib'))

try:
    import vec_module as vec
    import block_module as blk
    import chunk_module as chk
    import world_module as wrd
    import texture_module as tex
    import image_module as img
    import raycaster_module as ray
    print("✅ Все модули успешно загружены")
except ImportError as e:
    print(f"❌ Ошибка импорта: {e}")
    sys.exit(1)


def create_test_texture(texture_manager: tex.TextureManager) -> int:
    """Создаёт простую шахматную текстуру для тестирования."""
    size = 64
    test_img = img.Image(size, size)
    
    for y in range(size):
        for x in range(size):
            is_white = ((x // 16) + (y // 16)) % 2 == 0
            color = vec.Vec3u8(255, 255, 255) if is_white else vec.Vec3u8(100, 100, 255)
            test_img.set_pixel(x, y, color)
            
    test_tex = tex.Texture([test_img], "test_checkerboard")
    return texture_manager.register_texture(test_tex)


class RaycasterViewer:
    def __init__(self):
        pygame.init()
        self.width, self.height = 800, 600
        self.screen = pygame.display.set_mode((self.width, self.height), DOUBLEBUF)
        pygame.display.set_caption("Raycaster Test (Press R to toggle resolution)")
        
        # Настройки камеры
        # Камера смотрит вдоль +Z (yaw=0)
        self.cam_pos = np.array([8.0, 12.0, 0.0], dtype=np.float32)
        self.cam_yaw = 0.0    # 0 = вдоль +Z
        self.cam_pitch = 0.0
        self.fov = 60.0
        
        # Режимы рендеринга
        self.res_modes = [
            (64, 48, "LOW (64x48)"),
            (256, 192, "HIGH (256x192)"),
            (512, 384, "ULTRA (512x384)")
        ]
        self.current_res_idx = 0
        
        # Инициализация мира и текстур
        self.world = wrd.World()
        self.textures = tex.TextureManager()
        self.test_tex_id = create_test_texture(self.textures)
        
        self._setup_test_world()
        
        self.render_surface = None
        self.fps_font = pygame.font.SysFont('consolas', 18)

    def _setup_test_world(self):
        """Создаёт простую тестовую сцену."""
        print("Генерация тестового мира...")

        # ВАЖНО: Chunk хранит сырые Block*.
        # Владение блоком должно переживать и эту функцию, и все чанки.
        self.test_block = blk.Block(1, "test_block")
        self.test_block.status = 1
        print(1)
        for i in range(6):
            self.test_block.set_face_texture(i, self.test_tex_id)
        print(2)

        chunk = chk.Chunk(vec.Vec2i(0, 0))
        print(3)

        # for z in range(10, 15):
        #     for y in range(10, 13):
        #         chunk.set_block(8, y, z, self.test_block)
                
        # chunk.set_block(8, y, z, self.test_block)

        chunk.set_block(8, 10, 20, self.test_block)

        self.world.set_chunk(vec.Vec2i(0, 0), chunk)
        print("Мир готов.")

    def _get_render_resolution(self):
        return self.res_modes[self.current_res_idx][0], self.res_modes[self.current_res_idx][1]

    def render_frame(self):
        """Рендерит кадр через C++ raycaster."""
        res_w, res_h = self._get_render_resolution()
        
        # === ОТЛАДКА: Проверка мира ===
        if not hasattr(self, 'debug_printed'):
            self.debug_printed = True
            print("\n=== ОТЛАДКА ===")
            print(f"Позиция камеры: ({self.cam_pos[0]:.2f}, {self.cam_pos[1]:.2f}, {self.cam_pos[2]:.2f})")
            print(f"Yaw: {self.cam_yaw:.2f}°, Pitch: {self.cam_pitch:.2f}°")
            
            # Проверяем, что блоки есть в мире
            test_positions = [(8, 10, 10), (8, 11, 11), (8, 12, 12)]
            for pos in test_positions:
                block = self.world.get_block(vec.Vec3i(*pos))
                print(f"Блок на {pos}: {block}")
                if block:
                    print(f"  status={block.status}, face_texture[0]={block.get_face_texture(0)}")
            
            # Трассируем один луч вручную
            yaw_rad = math.radians(self.cam_yaw)
            pitch_rad = math.radians(self.cam_pitch)
            
            cos_yaw = math.cos(yaw_rad)
            sin_yaw = math.sin(yaw_rad)
            cos_pitch = math.cos(pitch_rad)
            sin_pitch = math.sin(pitch_rad)
            
            forward = vec.Vec3f(sin_yaw * cos_pitch, sin_pitch, cos_yaw * cos_pitch)
            right = vec.Vec3f(cos_yaw, 0.0, -sin_yaw)
            
            print(f"\nForward: ({forward.x:.3f}, {forward.y:.3f}, {forward.z:.3f})")
            print(f"Right: ({right.x:.3f}, {right.y:.3f}, {right.z:.3f})")
            
            # Центральный луч
            origin = vec.Vec3f(float(self.cam_pos[0]), float(self.cam_pos[1]), float(self.cam_pos[2]))
            ray_obj = ray.Ray(origin, forward, self.world, 0.5)
            
            print(f"\nТрассировка центрального луча:")
            for step in range(20):
                ray_obj.step(self.world)
                voxel = ray_obj.get_voxel_pos()
                block = self.world.get_block(voxel)
                has_block = block is not None and block.status != 0
                
                if has_block:
                    print(f"  Шаг {step+1}: voxel=({voxel.x}, {voxel.y}, {voxel.z}) - ПОПАДАНИЕ!")
                    hit = ray_obj.check_hit(self.textures, 0, vec.Vec3u8(128, 128, 128))
                    print(f"    hit={hit.hit}, color=({hit.color.x}, {hit.color.y}, {hit.color.z})")
                    break
                else:
                    if step < 5:  # Выводим только первые 5 шагов
                        print(f"  Шаг {step+1}: voxel=({voxel.x}, {voxel.y}, {voxel.z}) - пусто")
            else:
                print(f"  Луч не попал в блок за 20 шагов")
            
            print("=== КОНЕЦ ОТЛАДКИ ===\n")
        
        # Буфер под результат (height, width, 3)
        pixels = np.zeros((res_h, res_w, 3), dtype=np.uint8)
        
        origin = vec.Vec3f(float(self.cam_pos[0]),
                           float(self.cam_pos[1]),
                           float(self.cam_pos[2]))
        fog_color = vec.Vec3u8(135, 206, 235)
        
        yaw_rad = math.radians(self.cam_yaw)
        pitch_rad = math.radians(self.cam_pitch)
        
        start_time = time.time()
        
        # ОДИН вызов в C++ — вся трассировка всех пикселей
        ray.render_frame(
            self.world,
            self.textures,
            origin,
            yaw_rad,
            pitch_rad,
            self.fov,
            res_w,
            res_h,
            32.0,   # max_distance
            0.5,    # step_size
            0,      # current_tick
            512,    # world_height
            fog_color,
            pixels
        )
        
        render_time = time.time() - start_time
        
        # === ОТЛАДКА: Проверка результата ===
        if not hasattr(self, 'result_printed'):
            self.result_printed = True
            # Проверяем центральный пиксель
            center_x = res_w // 2
            center_y = res_h // 2
            center_color = pixels[center_y, center_x]
            print(f"Цвет центрального пикселя: ({center_color[0]}, {center_color[1]}, {center_color[2]})")
            print(f"Ожидался туман: (135, 206, 235) или красный: (255, 0, 0)")
        
        # Конвертируем в Pygame Surface
        surface = pygame.surfarray.make_surface(np.transpose(pixels, (1, 0, 2)))
        self.render_surface = pygame.transform.scale(surface, (self.width, self.height))
        
        return render_time

    def handle_input(self):
        keys = pygame.key.get_pressed()
        yaw_rad = math.radians(self.cam_yaw)
        
        # Forward — направление взгляда в горизонтальной плоскости
        forward = np.array([math.sin(yaw_rad), 0.0, math.cos(yaw_rad)], dtype=np.float32)
        right = np.array([math.cos(yaw_rad), 0.0, -math.sin(yaw_rad)], dtype=np.float32)
        
        speed = 0.2
        if keys[pygame.K_w] or keys[ord('ц')]: self.cam_pos += forward * speed
        if keys[pygame.K_s] or keys[ord('ы')]: self.cam_pos -= forward * speed
        if keys[pygame.K_a] or keys[ord('ф')]: self.cam_pos -= right * speed
        if keys[pygame.K_d] or keys[ord('в')]: self.cam_pos += right * speed
        if keys[pygame.K_SPACE]: self.cam_pos[1] += speed
        if keys[pygame.K_LSHIFT]: self.cam_pos[1] -= speed
        
        dx, dy = pygame.mouse.get_rel()
        if dx != 0 or dy != 0:
            self.cam_yaw = (self.cam_yaw + dx * 0.15) % 360.0
            self.cam_pitch = max(-89.0, min(89.0, self.cam_pitch + dy * 0.15))

    def run(self):
        clock = pygame.time.Clock()
        running = True
        pygame.mouse.set_visible(False)
        pygame.event.set_grab(True)
        
        print("\n" + "="*60)
        print("УПРАВЛЕНИЕ: WASD/ЦФЫВ - движение, SPACE/SHIFT - вверх/вниз")
        print("Мышь - обзор, R - сменить разрешение рендера, ESC - выход")
        print("="*60)
        
        while running:
            for event in pygame.event.get():
                if event.type == QUIT or (event.type == KEYDOWN and event.key == K_ESCAPE):
                    running = False
                elif event.type == KEYDOWN and event.key == K_r:
                    self.current_res_idx = (self.current_res_idx + 1) % len(self.res_modes)
                    print(f"Разрешение изменено на: {self.res_modes[self.current_res_idx][2]}")

            self.handle_input()
            
            # Рендеринг
            render_time = self.render_frame()
            
            # Отрисовка на экран
            self.screen.fill((0, 0, 0))
            if self.render_surface:
                self.screen.blit(self.render_surface, (0, 0))
            
            # UI
            fps = clock.get_fps()
            res_name = self.res_modes[self.current_res_idx][2]
            ui_text = f"FPS: {fps:.1f} | Render: {render_time*1000:.1f}ms | Res: {res_name} | Pos: ({self.cam_pos[0]:.1f}, {self.cam_pos[1]:.1f}, {self.cam_pos[2]:.1f})"
            text_surf = self.fps_font.render(ui_text, True, (255, 255, 255))
            bg_rect = text_surf.get_rect(topleft=(5, 5))
            pygame.draw.rect(self.screen, (0, 0, 0), bg_rect)
            self.screen.blit(text_surf, (5, 5))
            
            pygame.display.flip()
            clock.tick(60)
            
        pygame.mouse.set_visible(True)
        pygame.event.set_grab(False)
        pygame.quit()


if __name__ == "__main__":
    RaycasterViewer().run()


