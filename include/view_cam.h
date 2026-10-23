#pragma once

#include "vec.h"


// Параметры камеры, которые нужны рендереру.
// Вынесены из renderer.h, чтобы Camera/Dummy могли хранить ViewCam,
// не завися от OpenGL-рендерера.
struct ViewCam {
    Vec3f position;
    Vec3r rotation;
    float fov;
    float near_plane;
    float far_plane;

    ViewCam()
        : position(Vec3f(0, 0, 5))
        , rotation(Vec3r(0, 0, 0, false))
        , fov(60.0f)
        , near_plane(0.1f)
        , far_plane(1000.0f) {}
};
