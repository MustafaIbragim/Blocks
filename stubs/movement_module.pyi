from typing import List, Optional, overload

from collision_module import DynamicCollision
from vec_module import Vec3f, Vec3i
from world_module import World


class PathNode:
    block: Vec3i
    feet_y: float

    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, block: Vec3i, feet_y: float) -> None: ...

    def feet_position(self) -> Vec3f: ...
    def body_center(self, half_extents: Vec3f) -> Vec3f: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...
    def __repr__(self) -> str: ...


class PathMover:
    @property
    def path(self) -> List[PathNode]: ...
    @property
    def path_blocks(self) -> List[Vec3i]: ...
    @property
    def path_length(self) -> float: ...
    speed: float
    arrival_radius: float
    step_height: float
    half_extents: Vec3f

    @overload
    def find_path(self, world: World, start: Vec3i, target: Vec3i) -> bool: ...
    @overload
    def find_path(
        self, world: World, start: Vec3i, target: Vec3i, step_height: float
    ) -> bool: ...
    @overload
    def find_path(
        self, world: World, start: Vec3i, target: Vec3i, half_extents: Vec3f
    ) -> bool: ...
    @overload
    def find_path(
        self,
        world: World,
        start: Vec3i,
        target: Vec3i,
        half_extents: Vec3f,
        step_height: float,
    ) -> bool: ...

    @overload
    def move_to(
        self,
        world: World,
        body: DynamicCollision,
        start: Vec3i,
        target: Vec3i,
        duration: float,
        step_height: Optional[float] = None,
        snap_to_start: bool = True,
    ) -> bool: ...
    @overload
    def move_to(
        self,
        world: World,
        body: DynamicCollision,
        target: Vec3i,
        duration: float,
        step_height: Optional[float] = None,
        snap_to_start: bool = True,
    ) -> bool: ...

    def update(
        self, world: World, body: DynamicCollision, delta_time: float
    ) -> bool: ...
    def stop(self) -> None: ...
    def clear_path(self) -> None: ...
    def has_path(self) -> bool: ...
    def is_moving(self) -> bool: ...
    def current_index(self) -> int: ...
    def current_node(self) -> PathNode: ...
    def target_node(self) -> PathNode: ...

    @staticmethod
    def current_block(body: DynamicCollision) -> Vec3i: ...


class GroundMover(PathMover):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, step_height: float) -> None: ...
    @overload
    def __init__(
        self,
        body_radius: float,
        body_height: float,
        step_height: float = 0.6,
    ) -> None: ...

    @staticmethod
    def body_free(
        world: World, feet_position: Vec3f, half_extents: Vec3f
    ) -> bool: ...
    @staticmethod
    def support_height(
        world: World,
        x: int,
        z: int,
        half_extents: Vec3f,
        max_y: float,
        max_drop: float,
    ) -> Optional[float]: ...
    @staticmethod
    def try_step_up(
        world: World,
        body: DynamicCollision,
        desired_delta: Vec3f,
        step_height: float,
    ) -> bool: ...


class FlyMover(PathMover):
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, body_radius: float, body_height: float) -> None: ...
