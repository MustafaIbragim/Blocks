import vec_module
import world_module
import mesh_module


DEFAULT_SEARCH_RADIUS: int
"""Радиус поиска источников света по умолчанию."""

MAX_SOURCES: int
"""Максимальное количество учитываемых ближайших источников света (обычно 4)."""

def sample_vertex_light(
    world: world_module.World,
    position: vec_module.Vec3f,
    search_radius: int = DEFAULT_SEARCH_RADIUS,
) -> int:
    ...

def update_mesh_lighting(
    world: world_module.World,
    mesh: mesh_module.Mesh,
    origin: vec_module.Vec3f,
    search_radius: int = DEFAULT_SEARCH_RADIUS,
) -> bool: ...


