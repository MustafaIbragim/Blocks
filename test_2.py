import math
import numpy as np
import pygame
from pygame.locals import *


class Cube:
    __slots__ = ['val']
    def __init__(self, val=1):
        self.val = val


class Fragment:
    SIZE = 16
    BLOCKS_COUNT = SIZE * SIZE * SIZE
    
    def __init__(self):
        self.blocks = [None] * self.BLOCKS_COUNT
        self.left = self.right = self.up = self.down = self.front = self.back = None
    
    def coords_to_index(self, lx, ly, lz):
        return lx + ly * self.SIZE + lz * self.SIZE * self.SIZE
    
    def get_block(self, lx, ly, lz):
        if 0 <= lx < self.SIZE and 0 <= ly < self.SIZE and 0 <= lz < self.SIZE:
            return self.blocks[self.coords_to_index(lx, ly, lz)]
        return None
    
    def set_block(self, lx, ly, lz, value):
        if 0 <= lx < self.SIZE and 0 <= ly < self.SIZE and 0 <= lz < self.SIZE:
            self.blocks[self.coords_to_index(lx, ly, lz)] = value


class Chunk:
    FRAGMENTS_Y = 16
    FRAGMENT_SIZE = Fragment.SIZE
    SIZE = FRAGMENT_SIZE
    HEIGHT = FRAGMENTS_Y * FRAGMENT_SIZE
    
    def __init__(self, cx, cz):
        self.cx, self.cz = cx, cz
        self.fragments = [Fragment() for _ in range(self.FRAGMENTS_Y)]
        for fy in range(self.FRAGMENTS_Y):
            if fy > 0: self.fragments[fy].up = self.fragments[fy - 1]
            if fy < self.FRAGMENTS_Y - 1: self.fragments[fy].down = self.fragments[fy + 1]
    
    def get_block(self, wx, wy, wz):
        lx, lz = wx % self.SIZE, wz % self.SIZE
        fy, ly = wy // self.FRAGMENT_SIZE, wy % self.FRAGMENT_SIZE
        if 0 <= fy < self.FRAGMENTS_Y:
            return self.fragments[fy].get_block(lx, ly, lz)
        return None
    
    def set_block(self, wx, wy, wz, value):
        lx, lz = wx % self.SIZE, wz % self.SIZE
        fy, ly = wy // self.FRAGMENT_SIZE, wy % self.FRAGMENT_SIZE
        if 0 <= fy < self.FRAGMENTS_Y:
            self.fragments[fy].set_block(lx, ly, lz, value)


class World:
    def __init__(self, size_x=5, size_z=5):
        self.chunks = {}
        for cx in range(size_x):
            for cz in range(size_z):
                self.chunks[(cx, cz)] = Chunk(cx, cz)
        
        for cx in range(size_x):
            for cz in range(size_z):
                chunk = self.chunks[(cx, cz)]
                if cx > 0:
                    for fy in range(Chunk.FRAGMENTS_Y): chunk.fragments[fy].left = self.chunks[(cx - 1, cz)].fragments[fy]
                if cx < size_x - 1:
                    for fy in range(Chunk.FRAGMENTS_Y): chunk.fragments[fy].right = self.chunks[(cx + 1, cz)].fragments[fy]
                if cz > 0:
                    for fy in range(Chunk.FRAGMENTS_Y): chunk.fragments[fy].front = self.chunks[(cx, cz - 1)].fragments[fy]
                if cz < size_z - 1:
                    for fy in range(Chunk.FRAGMENTS_Y): chunk.fragments[fy].back = self.chunks[(cx, cz + 1)].fragments[fy]
    
    def get_block(self, wx, wy, wz):
        cx, cz = wx // Chunk.SIZE, wz // Chunk.SIZE
        if (cx, cz) in self.chunks:
            return self.chunks[(cx, cz)].get_block(wx, wy, wz)
        return None
    
    def set_block(self, wx, wy, wz, value):
        cx, cz = wx // Chunk.SIZE, wz // Chunk.SIZE
        if (cx, cz) in self.chunks:
            self.chunks[(cx, cz)].set_block(wx, wy, wz, value)

    def get_fragment(self, wx, wy, wz):
        """Быстрый поиск фрагмента по мировым координатам"""
        cx, cz = wx // Chunk.SIZE, wz // Chunk.SIZE
        if (cx, cz) in self.chunks:
            fy = wy // Fragment.SIZE
            if 0 <= fy < Chunk.FRAGMENTS_Y:
                return self.chunks[(cx, cz)].fragments[fy]
        return None


class Vector:
    def __init__(self, start_x, start_y, start_z, dx, dy, dz, world):
        self.start_x, self.start_y, self.start_z = float(start_x), float(start_y), float(start_z)
        length = math.sqrt(dx*dx + dy*dy + dz*dz)
        if length > 0:
            self.dir_x, self.dir_y, self.dir_z = dx/length, dy/length, dz/length
        else:
            self.dir_x = self.dir_y = self.dir_z = 0.0
            
        self.block_x = math.floor(self.start_x)
        self.block_y = math.floor(self.start_y)
        self.block_z = math.floor(self.start_z)
        
        self.step_x = 1 if self.dir_x > 0 else -1
        self.step_y = 1 if self.dir_y > 0 else -1
        self.step_z = 1 if self.dir_z > 0 else -1
        
        self.t_delta_x = abs(1.0 / self.dir_x) if self.dir_x != 0 else float('inf')
        self.t_delta_y = abs(1.0 / self.dir_y) if self.dir_y != 0 else float('inf')
        self.t_delta_z = abs(1.0 / self.dir_z) if self.dir_z != 0 else float('inf')
        
        self.t_max_x = self._calc_t_max(self.start_x, self.block_x, self.dir_x, self.t_delta_x)
        self.t_max_y = self._calc_t_max(self.start_y, self.block_y, self.dir_y, self.t_delta_y)
        self.t_max_z = self._calc_t_max(self.start_z, self.block_z, self.dir_z, self.t_delta_z)
        
        self.steps = 0
        self.last_axis = -1
        
        # === КЕШИРОВАНИЕ ФРАГМЕНТА ===
        self.frag_x = self.block_x >> 4
        self.frag_y = self.block_y >> 4
        self.frag_z = self.block_z >> 4
        self.fragment = world.get_fragment(self.block_x, self.block_y, self.block_z)
        
    @classmethod
    def from_numpy(cls, start_x, start_y, start_z, directions_array, py, px, world):
        dx = float(directions_array[py, px, 0])
        dy = float(directions_array[py, px, 1])
        dz = float(directions_array[py, px, 2])
        return cls(start_x, start_y, start_z, dx, dy, dz, world)
    
    def _calc_t_max(self, start, block, direction, t_delta):
        if direction > 0: return t_delta * (1.0 - (start - block))
        elif direction < 0: return t_delta * (start - block)
        return float('inf')
            
    def step(self, world):
        self.steps += 1
        if self.t_max_x < self.t_max_y:
            if self.t_max_x < self.t_max_z:
                self.block_x += self.step_x; self.t_max_x += self.t_delta_x; self.last_axis = 0
            else:
                self.block_z += self.step_z; self.t_max_z += self.t_delta_z; self.last_axis = 2
        else:
            if self.t_max_y < self.t_max_z:
                self.block_y += self.step_y; self.t_max_y += self.t_delta_y; self.last_axis = 1
            else:
                self.block_z += self.step_z; self.t_max_z += self.t_delta_z; self.last_axis = 2
        
        # === ПРОВЕРКА ПЕРЕХОДА МЕЖДУ ФРАГМЕНТАМИ ===
        new_fx = self.block_x >> 4
        new_fy = self.block_y >> 4
        new_fz = self.block_z >> 4
        
        if new_fx != self.frag_x or new_fy != self.frag_y or new_fz != self.frag_z:
            # DDA гарантирует, что за 1 шаг мы пересекаем только ОДНУ границу
            if new_fx != self.frag_x:
                self.fragment = self.fragment.right if new_fx > self.frag_x else self.fragment.left
                self.frag_x = new_fx
            elif new_fy != self.frag_y:
                self.fragment = self.fragment.down if new_fy > self.frag_y else self.fragment.up
                self.frag_y = new_fy
            elif new_fz != self.frag_z:
                self.fragment = self.fragment.back if new_fz > self.frag_z else self.fragment.front
                self.frag_z = new_fz
                
        return self.last_axis, self.block_x, self.block_y, self.block_z
        
    def get_current_block(self):
        """Прямое чтение блока из кешированного фрагмента"""
        if self.fragment is None:
            return None
        lx = self.block_x & 15
        ly = self.block_y & 15
        lz = self.block_z & 15
        return self.fragment.blocks[self.fragment.coords_to_index(lx, ly, lz)]
        
    def trace(self, world, max_distance):
        # Проверяем стартовый блок
        if self.get_current_block() is not None:
            return True, self.block_x, self.block_y, self.block_z, -1, 0.0
            
        distance = 0.0
        while distance < max_distance:
            axis, bx, by, bz = self.step(world)
            
            if axis == 0: distance = self.t_max_x - self.t_delta_x
            elif axis == 1: distance = self.t_max_y - self.t_delta_y
            else: distance = self.t_max_z - self.t_delta_z
            
            # Если вышли за пределы мира (fragment стал None)
            if self.fragment is None:
                return False, 0, 0, 0, -1, distance
                
            # ПРЯМОЕ ЧТЕНИЕ БЕЗ WORLD.GET_BLOCK!
            if self.get_current_block() is not None:
                return True, bx, by, bz, axis, distance
                
        return False, 0, 0, 0, -1, distance


class Camera:
    def __init__(self, x, y, z, yaw=0.0, pitch=0.0, fov=60.0):
        self.x, self.y, self.z = float(x), float(y), float(z)
        self.yaw, self.pitch = float(yaw), float(pitch)
        self.fov = float(fov)

    def get_directions(self, width, height):
        x_norm = np.linspace(-1, 1, width)
        y_norm = np.linspace(1, -1, height)
        x_grid, y_grid = np.meshgrid(x_norm, y_norm)
        
        cos_y, sin_y = math.cos(self.yaw), math.sin(self.yaw)
        cos_p, sin_p = math.cos(self.pitch), math.sin(self.pitch)
        
        forward = np.array([sin_y * cos_p, sin_p, cos_y * cos_p])
        right = np.array([cos_y, 0.0, -sin_y])
        up = np.array([0.0, 1.0, 0.0])
        
        fov_scale = math.tan(math.radians(self.fov / 2.0))
        directions = (forward + right * (x_grid * fov_scale)[..., np.newaxis] + up * (y_grid * fov_scale)[..., np.newaxis])
        return directions / np.sqrt(np.sum(directions ** 2, axis=2, keepdims=True))


def get_hit_color(axis, step_x, step_y, step_z):
    if axis == 0: return (255, 100, 100) if step_x > 0 else (180, 50, 50)
    if axis == 1: return (100, 255, 100) if step_y > 0 else (50, 180, 50)
    if axis == 2: return (100, 100, 255) if step_z > 0 else (50, 50, 180)
    return (0, 0, 0)


def main():
    pygame.init()
    width, height = 64, 48
    screen = pygame.display.set_mode((width * 10, height * 10))
    pygame.display.set_caption("3D Raycaster (Fragment Caching) - WASD, Mouse, ESC")
    
    pygame.mouse.set_visible(False)
    pygame.event.set_grab(True)

    world = World(5, 5)
    
    for x in range(10, 70):
        for z in range(10, 70):
            world.set_block(x, 100, z, Cube(val=1))
            
    for y in range(101, 110):
        world.set_block(40, y, 40, Cube(val=1))
        world.set_block(30, y, 30, Cube(val=1))
        world.set_block(50, y, 50, Cube(val=1))

    camera = Camera(40.0, 105.0, 20.0, yaw=0.0, pitch=-0.2, fov=70.0)
    clock = pygame.time.Clock()
    font = pygame.font.SysFont('consolas', 16)
    
    running = True
    while running:
        dt = clock.tick(60) / 1000.0
        
        for event in pygame.event.get():
            if event.type == QUIT or (event.type == KEYDOWN and event.key == K_ESCAPE):
                running = False
            elif event.type == MOUSEBUTTONDOWN:
                pygame.event.set_grab(True)

        keys = pygame.key.get_pressed()
        dx, dy = pygame.mouse.get_rel()
        
        camera.yaw += dx * 0.002
        camera.pitch -= dy * 0.002
        camera.pitch = max(-1.5, min(1.5, camera.pitch))
        
        cos_y, sin_y = math.cos(camera.yaw), math.sin(camera.yaw)
        forward = np.array([sin_y, 0.0, cos_y])
        right = np.array([cos_y, 0.0, -sin_y])
        
        speed = 5.0 * dt
        if keys[pygame.K_w]: camera.x += forward[0]*speed; camera.z += forward[2]*speed
        if keys[pygame.K_s]: camera.x -= forward[0]*speed; camera.z -= forward[2]*speed
        if keys[pygame.K_a]: camera.x -= right[0]*speed; camera.z -= right[2]*speed
        if keys[pygame.K_d]: camera.x += right[0]*speed; camera.z += right[2]*speed
        if keys[pygame.K_SPACE]: camera.y += speed
        if keys[pygame.K_LSHIFT]: camera.y -= speed

        render_surface = pygame.Surface((width, height))
        directions = camera.get_directions(width, height)
        
        for py in range(height):
            for px in range(width):
                # Передаем world только для инициализации кеширования
                vec = Vector.from_numpy(camera.x, camera.y, camera.z, directions, py, px, world)
                hit, bx, by, bz, axis, dist = vec.trace(world, max_distance=100.0)
                
                if hit:
                    color = get_hit_color(axis, vec.step_x, vec.step_y, vec.step_z)
                else:
                    color = (30, 30, 40)
                
                render_surface.set_at((px, py), color)
        
        scaled_surface = pygame.transform.scale(render_surface, (width * 10, height * 10))
        screen.blit(scaled_surface, (0, 0))
        
        info = f"FPS: {clock.get_fps():.1f} | Pos: ({camera.x:.1f}, {camera.y:.1f}, {camera.z:.1f})"
        text = font.render(info, True, (255, 255, 255))
        screen.blit(text, (5, 5))
        
        pygame.display.flip()

    pygame.mouse.set_visible(True)
    pygame.event.set_grab(False)
    pygame.quit()

if __name__ == "__main__":
    main()


