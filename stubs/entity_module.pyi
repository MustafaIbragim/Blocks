from vec_module import Transform


class Entity:
    transform: Transform
    dynamic: bool
    speed: float
    drag: float
    gravity: float

    def __init__(self) -> None: ...
    def __init__(self, transform: Transform) -> None: ...

    def update(self, tick: int, gravity: float = 0.0) -> None: ...

    def __repr__(self) -> str: ...
