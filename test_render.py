import sys
sys.path.append('python')

import numpy as np
import pygame
from pygame.locals import *
import moderngl
import math

# Импортируем наши модули
import vec_module
import block_module
import chunk_module
import world_module
import mesh_module
import mesh_builder_module
import texture_module

# ============================================================
# Создание тестовых текстур и блоков
# ============================================================

def setup_textures():
    """Создаёт TextureManager с тестовыми текстурами"""
    tm = texture_module.TextureManager()
    
    # Создаём 6 тестовых текстур (по одной на каждую грань)
    colors = [
        (230, 50, 50),    # 0: красный (+X)
        (50, 230, 50),    # 1: зелёный (-X)
        (50, 50, 230),    # 2: синий (+Y)
        (230, 230, 50),   # 3: жёлтый (-Y)
        (230, 50, 230),   # 4: фиолетовый (+Z)
        (50, 230, 230),   # 5: циановый (-Z)
    ]
    ###
    colors[2] = (0, 0, 0)
    
    texture_layers = []  # numpy-массивы для загрузки в GPU
    
    for i, (r, g, b) in enumerate(colors):
        tex = texture_module.Texture(f"face_{i}")
        tex.id = i
        
        layer_id = tm.register_texture(tex)
        
        # Создаём тестовую текстуру 16x16 с этим цветом
        img_data = np.zeros((16, 16, 3), dtype=np.uint8)
        img_data[:, :, 0] = r
        img_data[:, :, 1] = g
        img_data[:, :, 2] = b
        texture_layers.append(img_data)
    
    return tm, texture_layers

def create_face_mesh(face_index, layer_id):
    """Создаёт меш для конкретной грани куба с правильным порядком обхода (CCW)"""
    mesh = mesh_module.Mesh()
    mesh.layer_id = layer_id
    
    # Порядок вершин: CCW (против часовой стрелки) при взгляде снаружи
    # Формат: x, y, z, u, v
    if face_index == 0:  # +X (Правая) - смотрим с +X
        coords = [
            1.0, 0.0, 1.0,  0.0, 0.0,  # 0: правый нижний задний
            1.0, 0.0, 0.0,  1.0, 0.0,  # 1: правый нижний передний
            1.0, 1.0, 0.0,  1.0, 1.0,  # 2: правый верхний передний
            1.0, 1.0, 1.0,  0.0, 1.0   # 3: правый верхний задний
        ]
    elif face_index == 1:  # -X (Левая) - смотрим с -X
        coords = [
            0.0, 0.0, 0.0,  0.0, 0.0,  # 0: левый нижний передний
            0.0, 0.0, 1.0,  1.0, 0.0,  # 1: левый нижний задний
            0.0, 1.0, 1.0,  1.0, 1.0,  # 2: левый верхний задний
            0.0, 1.0, 0.0,  0.0, 1.0   # 3: левый верхний передний
        ]
    elif face_index == 2:  # +Y (Верхняя) - смотрим с +Y
        coords = [
            0.0, 1.0, 0.0,  0.0, 0.0,  # 0: левый верхний передний
            1.0, 1.0, 0.0,  1.0, 0.0,  # 1: правый верхний передний
            1.0, 1.0, 1.0,  1.0, 1.0,  # 2: правый верхний задний
            0.0, 1.0, 1.0,  0.0, 1.0   # 3: левый верхний задний
        ]
    elif face_index == 3:  # -Y (Нижняя) - смотрим с -Y
        coords = [
            0.0, 0.0, 1.0,  0.0, 0.0,  # 0: левый нижний задний
            1.0, 0.0, 1.0,  1.0, 0.0,  # 1: правый нижний задний
            1.0, 0.0, 0.0,  1.0, 1.0,  # 2: правый нижний передний
            0.0, 0.0, 0.0,  0.0, 1.0   # 3: левый нижний передний
        ]
    elif face_index == 4:  # +Z (Передняя) - смотрим с +Z
        coords = [
            0.0, 0.0, 1.0,  0.0, 0.0,  # 0: левый нижний передний
            1.0, 0.0, 1.0,  1.0, 0.0,  # 1: правый нижний передний
            1.0, 1.0, 1.0,  1.0, 1.0,  # 2: правый верхний передний
            0.0, 1.0, 1.0,  0.0, 1.0   # 3: левый верхний передний
        ]
    else:  # 5: -Z (Задняя) - смотрим с -Z
        coords = [
            1.0, 0.0, 0.0,  0.0, 0.0,  # 0: правый нижний задний
            0.0, 0.0, 0.0,  1.0, 0.0,  # 1: левый нижний задний
            0.0, 1.0, 0.0,  1.0, 1.0,  # 2: левый верхний задний
            1.0, 1.0, 0.0,  0.0, 1.0   # 3: правый верхний задний
        ]
    
    # Формируем аргументы для add_quad
    args = []
    for i in range(4):
        args.extend([
            coords[i*5],     # x
            coords[i*5 + 1], # y
            coords[i*5 + 2], # z
            coords[i*5 + 3], # u
            coords[i*5 + 4], # v
            layer_id         # layer
        ])
    
    mesh.add_quad(*args)
    return mesh

def create_cube_meshes(texture_manager):
    """Создаёт 6 отдельных мешей для куба (по одному на каждую грань)"""
    meshes = []
    for face_index in range(6):
        # layer_id совпадает с face_index в нашем тесте (0..5)
        layer_id = texture_manager.get_layer_id(face_index)
        meshes.append(create_face_mesh(face_index, layer_id))
    return meshes

def setup_blocks(texture_manager):
    """Создаёт BlockManager с тестовыми блоками"""
    block_manager = block_module.BlockManager()
    
    # Воздух (id=0)
    air = block_module.Block(0, "air")
    air.opacity = 0
    air.face_flags = 0
    block_manager.register_block(air)
    
    # Создаём меши для куба
    cube_meshes = create_cube_meshes(texture_manager)
    
    # Камень (id=1)
    stone = block_module.Block(1, "stone")
    stone.opacity = 255
    stone.face_flags = 0x3F
    for i in range(6):
        stone.set_face_mesh(i, cube_meshes[i])
    block_manager.register_block(stone)
    
    # Трава (id=2)
    grass = block_module.Block(2, "grass")
    grass.opacity = 255
    grass.face_flags = 0x3F
    for i in range(6):
        grass.set_face_mesh(i, cube_meshes[i])
    block_manager.register_block(grass)
    
    return block_manager

# ============================================================
# Генерация тестового мира
# ============================================================

def generate_world(world, block_manager, radius=2):
    """Генерирует мир с холмами и столбами"""
    print(f"Генерация мира {radius*2+1}x{radius*2+1} чанков...")
    
    for tx in range(-radius, radius + 1):
        for tz in range(-radius, radius + 1):
            chunk = chunk_module.Chunk()
            chunk.chunk_xz = vec_module.Vec2i(tx, tz)
            world.set_chunk(vec_module.Vec2i(tx, tz), chunk)
    
    # Синусоидальные холмы
    for wx in range(-radius * 32, (radius + 1) * 32):
        for wz in range(-radius * 32, (radius + 1) * 32):
            height = int(10 + 5 * math.sin(wx * 0.1) + 5 * math.cos(wz * 0.1))
            for wy in range(20): # height):
                block_id = 2 if wy == height - 1 else 1
                world.set_block(vec_module.Vec3i(wx, wy, wz), block_id)
    
    # Столбы
    for i in range(10):
        px = int(20 * math.sin(i * 1.5))
        pz = int(20 * math.cos(i * 1.5))
        height = 15 + i * 2
        for wy in range(height):
            world.set_block(vec_module.Vec3i(px, wy, pz), 1)
    
    print("Мир сгенерирован")

# ============================================================
# Построение мешей
# ============================================================

def build_chunk_meshes(world, block_manager):
    """Строит меши для всех чанков с учётом соседей"""
    print("Построение мешей...")
    
    chunk_meshes = {}
    
    # Сначала перестраиваем меши для всех чанков
    for tx in range(-2, 3):
        for tz in range(-2, 3):
            chunk = world.get_chunk(vec_module.Vec2i(tx, tz))
            if not chunk:
                continue
            
            # Собираем 26 соседей
            neighbor_chunks = []
            for dy in [-1, 0, 1]:
                for dx in [-1, 0, 1]:
                    for dz in [-1, 0, 1]:
                        if dx == 0 and dy == 0 and dz == 0:
                            continue  # Пропускаем центральный чанк
                        
                        nx, nz = tx + dx, tz + dz
                        neighbor = world.get_chunk(vec_module.Vec2i(nx, nz))
                        neighbor_chunks.append(neighbor)
            
            # Перестраиваем меши чанка
            chunk.rebuild_meshes(block_manager, neighbor_chunks)
    
    # Теперь собираем меши из всех чанков
    for tx in range(-2, 3):
        for tz in range(-2, 3):
            chunk = world.get_chunk(vec_module.Vec2i(tx, tz))
            if not chunk:
                continue
            
            chunk_meshes[(tx, tz)] = chunk.get_meshes()
    
    total_meshes = sum(len(m) for m in chunk_meshes.values())
    print(f"Построено мешей: {total_meshes}")
    return chunk_meshes

# ============================================================
# Рендер с TextureArray
# ============================================================

VERTEX_SHADER = """
#version 330
in vec3 in_position;
in vec2 in_uv;
in float in_layer;

uniform mat4 m_proj;
uniform mat4 m_view;

out vec2 v_uv;
out float v_layer;

void main() {
    gl_Position = m_proj * m_view * vec4(in_position, 1.0);
    v_uv = in_uv;
    v_layer = in_layer;
}
"""

FRAGMENT_SHADER = """
#version 330
in vec2 v_uv;
in float v_layer;

uniform sampler2DArray texture_array;

out vec4 frag_color;

void main() {
    // Сэмплим из TextureArray по UV и слою
    frag_color = texture(texture_array, vec3(v_uv, v_layer));
}
"""

def create_look_at_matrix(eye, target, up):
    """Создаёт look-at матрицу"""
    eye = np.array(eye, dtype='f4')
    target = np.array(target, dtype='f4')
    up = np.array(up, dtype='f4')
    
    f = target - eye
    f_len = np.linalg.norm(f)
    if f_len > 0: f = f / f_len
        
    s = np.cross(f, up)
    s_len = np.linalg.norm(s)
    if s_len > 0: s = s / s_len
        
    u = np.cross(s, f)
    
    m = np.zeros((4, 4), dtype='f4')
    m[0, 0] = s[0];  m[1, 0] = s[1];  m[2, 0] = s[2];  m[3, 0] = -np.dot(s, eye)
    m[0, 1] = u[0];  m[1, 1] = u[1];  m[2, 1] = u[2];  m[3, 1] = -np.dot(u, eye)
    m[0, 2] = -f[0]; m[1, 2] = -f[1]; m[2, 2] = -f[2]; m[3, 2] =  np.dot(f, eye)
    m[0, 3] = 0.0;   m[1, 3] = 0.0;   m[2, 3] = 0.0;   m[3, 3] =  1.0
    return m.T

class WorldViewer:
    def __init__(self, width=1280, height=720):
        pygame.init()
        pygame.display.set_mode((width, height), DOUBLEBUF | OPENGL)
        pygame.display.set_caption("Voxel World Test - TextureArray")
        
        pygame.mouse.set_visible(False)
        pygame.event.set_grab(True)
        
        self.ctx = moderngl.create_context()
        self.ctx.enable(moderngl.DEPTH_TEST)
        self.ctx.disable(moderngl.CULL_FACE)
        
        self.program = self.ctx.program(
            vertex_shader=VERTEX_SHADER, 
            fragment_shader=FRAGMENT_SHADER
        )
        
        self.cam_pos = np.array([0.0, 20.0, 60.0], dtype='f4')
        self.cam_yaw = 180.0
        self.cam_pitch = -25.0
        
        self.mesh_vaos = []
    
    def create_texture_array(self, texture_layers):
        """Создаёт GL_TEXTURE_2D_ARRAY из списка numpy-массивов"""
        num_layers = len(texture_layers)
        tex_size = texture_layers[0].shape[0]  # предполагаем квадратные текстуры
        
        # Собираем все слои в один массив (H, W, layers, 3)
        array_data = np.zeros((tex_size, tex_size, num_layers, 3), dtype=np.uint8)
        for i, layer in enumerate(texture_layers):
            array_data[:, :, i, :] = layer
        
        # ModernGL требует формат (layers, H, W, channels)
        # Транспонируем в нужный порядок
        array_data = array_data.transpose((2, 0, 1, 3))  # (layers, H, W, 3)
        array_data = array_data.reshape((-1,))  # flatten
        
        # Создаём текстуру
        texture = self.ctx.texture_array(
            (tex_size, tex_size, num_layers),
            3,  # 3 компонента (RGB)
            array_data.tobytes()
        )
        texture.filter = (moderngl.NEAREST, moderngl.NEAREST)
        texture.use()
        
        print(f"✓ TextureArray создан: {tex_size}x{tex_size}x{num_layers}")
        return texture
    
    def setup_meshes(self, chunk_meshes):
        """Создаёт VAO для всех мешей"""
        print("Создание VAO...")
        
        for chunk_key, meshes_dict in chunk_meshes.items():
            for layer_id, mesh_data in meshes_dict.items():
                vertices = mesh_data['vertices']
                indices = mesh_data['indices']
                
                if len(vertices) == 0:
                    continue
                
                verts_np = np.array(vertices, dtype='f4').reshape((-1, 6))
                idxs_np = np.array(indices, dtype='i4')
                
                vbo = self.ctx.buffer(verts_np.tobytes())
                ibo = self.ctx.buffer(idxs_np.tobytes())
                
                vao = self.ctx.vertex_array(
                    self.program,
                    [(vbo, '3f 2f 1f', 'in_position', 'in_uv', 'in_layer')],
                    index_buffer=ibo,
                    index_element_size=4,
                )
                
                self.mesh_vaos.append({
                    'vao': vao,
                    'vbo': vbo,
                    'ibo': ibo,
                    'count': len(idxs_np)
                })
        
        total_verts = sum(m['vbo'].size // 24 for m in self.mesh_vaos)
        total_tris = sum(m['count'] // 3 for m in self.mesh_vaos)
        print(f"✓ Создано: {total_verts:,} вершин, {total_tris:,} треугольников")
    
    def handle_input(self):
        """Обработка ввода"""
        keys = pygame.key.get_pressed()
        yaw_rad = math.radians(self.cam_yaw)
        pitch_rad = math.radians(self.cam_pitch)
        
        forward = np.array([-math.sin(yaw_rad)*math.cos(pitch_rad), 
                           math.sin(pitch_rad), 
                           -math.cos(yaw_rad)*math.cos(pitch_rad)], dtype='f4')
        forward /= np.linalg.norm(forward)
        right = np.cross(forward, np.array([0.0, 1.0, 0.0], dtype='f4'))
        right /= np.linalg.norm(right)
        
        speed = 0.5
        if keys[pygame.K_w]: self.cam_pos += forward * speed
        if keys[pygame.K_s]: self.cam_pos -= forward * speed
        if keys[pygame.K_a]: self.cam_pos -= right * speed
        if keys[pygame.K_d]: self.cam_pos += right * speed
        if keys[pygame.K_SPACE]: self.cam_pos[1] += speed
        if keys[pygame.K_LSHIFT]: self.cam_pos[1] -= speed
        
        dx, dy = pygame.mouse.get_rel()
        if dx != 0 or dy != 0:
            self.cam_yaw = (self.cam_yaw - dx * 0.15) % 360.0
            self.cam_pitch = max(-89.0, min(89.0, self.cam_pitch - dy * 0.15))
    
    def run(self):
        """Главный цикл"""
        clock = pygame.time.Clock()
        running = True
        
        print("\n" + "="*60)
        print("УПРАВЛЕНИЕ: WASD - движение, SPACE/SHIFT - вверх/вниз")
        print("Мышь - вращение камеры, ESC - выход")
        print("="*60)
        
        while running:
            for event in pygame.event.get():
                if event.type == QUIT or (event.type == KEYDOWN and event.key == K_ESCAPE):
                    running = False
            
            self.handle_input()
            
            # Вычисляем матрицы
            yaw_rad = math.radians(self.cam_yaw)
            pitch_rad = math.radians(self.cam_pitch)
            front = np.array([-math.sin(yaw_rad)*math.cos(pitch_rad), 
                             math.sin(pitch_rad), 
                             -math.cos(yaw_rad)*math.cos(pitch_rad)], dtype='f4')
            front /= np.linalg.norm(front)
            look_target = self.cam_pos + front
            
            view_mat = create_look_at_matrix(self.cam_pos, look_target, np.array([0.0, 1.0, 0.0], dtype='f4'))
            
            fov, aspect, near, far = 60.0, 1280 / 720, 0.1, 1000.0
            f = 1.0 / math.tan(math.radians(fov / 2))
            proj_mat = np.zeros((4, 4), dtype='f4')
            proj_mat[0, 0] = f / aspect
            proj_mat[1, 1] = f
            proj_mat[2, 2] = (far + near) / (near - far)
            proj_mat[2, 3] = (2 * far * near) / (near - far)
            proj_mat[3, 2] = -1.0
            
            # Рендер
            self.ctx.clear(0.53, 0.81, 0.92)
            self.program['m_proj'].write(proj_mat.T.tobytes())
            self.program['m_view'].write(view_mat.T.tobytes())
            
            for mesh_data in self.mesh_vaos:
                mesh_data['vao'].render(moderngl.TRIANGLES, mesh_data['count'])
            
            pygame.display.flip()
            clock.tick(60)
        
        pygame.quit()

# ============================================================
# Главная функция
# ============================================================

def main():
    print("Инициализация...")
    
    # Создаём текстуры
    texture_manager, texture_layers = setup_textures()
    print(f"Создано текстур: {texture_manager.size()}")
    
    # Создаём блоки
    block_manager = setup_blocks(texture_manager)
    print(f"Создано блоков: {block_manager.size()}")
    
    # Создаём мир
    world = world_module.World()
    generate_world(world, block_manager, radius=2)
    
    # Строим меши
    chunk_meshes = build_chunk_meshes(world, block_manager)
    
    # Создаём рендерер
    viewer = WorldViewer()
    
    # Создаём TextureArray и загружаем в GPU
    texture_array = viewer.create_texture_array(texture_layers)
    
    # Создаём VAO для мешей
    viewer.setup_meshes(chunk_meshes)
    
    # Запускаем
    viewer.run()

if __name__ == "__main__":
    main()


