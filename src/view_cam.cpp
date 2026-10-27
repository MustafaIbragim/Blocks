#include "view_cam.h"


ViewCam::ViewCam()
    : position(Vec3f(0, 0, 5))
    , rotation(Vec3r(0, 0, 0, false))
    , fov(60.0f)
    , near_plane(0.1f)
    , far_plane(1000.0f) {}
