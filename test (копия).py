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
        pygame.display.set_caption("Optimized Raycaster (Press R to toggle resolution)")
        
        # Камера теперь выше (y=40), чтобы смотреть на мир сверху/сбоку и не застревать в блоках
        self.cam_pos = np.array([0.0, 40.0, 0.0], dtype=np.float32)
        self.cam_yaw = 0.0
        self.cam_pitch = 0.0
        self.fov = 60.0
        
        self.res_modes = [
            (64, 48, "LOW (64x48)"),
            (160, 120, "TARGET (160x120)"),  # Целевое разрешение для проверки
            (256, 192, "HIGH (256x192)"),
            (512, 384, "ULTRA (512x384)")
        ]
        self.current_res_idx = 1  # Сразу запускаем на TARGET для теста
        
        self.world = wrd.World()
        self.textures = tex.TextureManager()
        self.test_tex_id = create_test_texture(self.textures)
        
        self._setup_test_world()
        self.render_surface = None
        self.fps_font = pygame.font.SysFont('consolas', 18)

    def _setup_test_world(self):
        print("Генерация тестового мира 5x5 чанков...")
        self.test_block = blk.Block(1, "test_block")
        self.test_block.status = 1
        for i in range(6):
            self.test_block.set_face_texture(i, self.test_tex_id)

        # Создаём 5x5 чанков (от -2 до 2)
        for cx in range(-2, 3):
            for cz in range(-2, 3):
                chunk = chk.Chunk(vec.Vec2i(cx, cz))
                
                # Заполняем только нижний слой (y=0) блоками для теста
                for x in range(32):
                    for z in range(32):
                        chunk.set_block(x, 0, z, self.test_block)
                
                self.world.set_chunk(vec.Vec2i(cx, cz), chunk)
                
        print(f"Мир готов: {5*5} чанков создано.")
    
    def _get_render_resolution(self):
        return self.res_modes[self.current_res_idx][0], self.res_modes[self.current_res_idx][1]

    def render_frame(self):
        res_w, res_h = self._get_render_resolution()
        pixels = np.zeros((res_h, res_w, 3), dtype=np.uint8)
        
        origin = vec.Vec3f(float(self.cam_pos[0]), float(self.cam_pos[1]), float(self.cam_pos[2]))
        fog_color = vec.Vec3u8(135, 206, 235)
        
        yaw_rad = math.radians(self.cam_yaw)
        pitch_rad = math.radians(self.cam_pitch)
        
        start_time = time.time()
        
        ray.render_frame(
            self.world,
            self.textures,
            origin,
            yaw_rad,
            pitch_rad,
            self.fov,
            res_w,
            res_h,
            512.0,      # ИЗМЕНЕНО: было 32.0, теперь 512.0 для соответствия GPU-сценарию
            0,
            512,
            fog_color,
            pixels
        )
        
        render_time = time.time() - start_time
        
        surface = pygame.surfarray.make_surface(np.transpose(pixels, (1, 0, 2)))
        self.render_surface = pygame.transform.scale(surface, (self.width, self.height))
        
        return render_time

    def handle_input(self):
        keys = pygame.key.get_pressed()
        yaw_rad = math.radians(self.cam_yaw)
        
        forward = np.array([math.sin(yaw_rad), 0.0, math.cos(yaw_rad)], dtype=np.float32)
        right = np.array([math.cos(yaw_rad), 0.0, -math.sin(yaw_rad)], dtype=np.float32)
        
        speed = 0.5 # Чуть увеличил скорость для большого мира
        if keys[pygame.K_w] or keys[ord('ц')]: self.cam_pos += forward * speed
        if keys[pygame.K_s] or keys[ord('ы')]: self.cam_pos -= forward * speed
        if keys[pygame.K_a] or keys[ord('ф')]: self.cam_pos -= right * speed
        if keys[pygame.K_d] or keys[ord('в')]: self.cam_pos += right * speed
        if keys[pygame.K_SPACE]: self.cam_pos[1] += speed
        if keys[pygame.K_LSHIFT]: self.cam_pos[1] -= speed
        
        dx, dy = pygame.mouse.get_rel()
        if dx != 0 or dy != 0:
            self.cam_yaw = (self.cam_yaw + dx * 0.15) % 360.0
            # ИНВЕРСИЯ ОСИ Y: меняем + на - перед dy
            self.cam_pitch = max(-89.0, min(89.0, self.cam_pitch - dy * 0.15))

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
                    print(f"Разрешение: {self.res_modes[self.current_res_idx][2]}")

            self.handle_input()
            render_time = self.render_frame()
            
            self.screen.fill((0, 0, 0))
            if self.render_surface:
                self.screen.blit(self.render_surface, (0, 0))
            
            fps = clock.get_fps()
            res_name = self.res_modes[self.current_res_idx][2]
            ui_text = f"FPS: {fps:.1f} | Render: {render_time*1000:.1f}ms | Res: {res_name}"
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


