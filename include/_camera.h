#pragma once

#include <memory>
#include "entity.h"
#include "graphics.h"
#include "render.h"


class Render;

class Camera : public Entity {
public:
    float fov = 60.0f;
    float aspect_ratio = 16.0f / 9.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    
    Camera();
    Camera(Render* renderer, const Vec3f& position, const Vec3r& rotation);
    
    void update_matrices();
    void render_frame(class World* world);
    
    void set_renderer(Render* renderer);
    
    const Mat4x4& get_view_matrix() const { return view_matrix; }
    const Mat4x4& get_projection_matrix() const { return projection_matrix; }

private:
    Render* renderer = nullptr;
    Mat4x4 view_matrix;
    Mat4x4 projection_matrix;
};


