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

    ViewCam();
};
