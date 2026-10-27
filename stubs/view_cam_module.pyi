from vec_module import Vec3f, Vec3r


class ViewCam:
    position: Vec3f
    rotation: Vec3r
    fov: float
    near_plane: float
    far_plane: float
    
    def __init__(self) -> None: ...
    
    def __repr__(self) -> str: ...


