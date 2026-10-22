#include "camera.h"
#include "world.h"
#include <cmath>

Camera::Camera() = default;

Camera::Camera(Render* renderer, const Vec3f& position, const Vec3r& rotation)
    : renderer(renderer) {
    transform.position = position;
    transform.rotation = rotation;
}

void Camera::set_renderer(Render* renderer) {
    this->renderer = renderer;
}

void Camera::update_matrices() {
    // View matrix: из позиции и вращения камеры
    Vec3f front = Vec3f(0.0f, 0.0f, -1.0f).rotate(transform.rotation);
    Vec3f target = transform.position + front;
    Vec3f up(0.0f, 1.0f, 0.0f);
    
    view_matrix = Mat4x4::look_at(transform.position, target, up);
    
    // Projection matrix
    projection_matrix = Mat4x4::perspective(
        fov, aspect_ratio, near_plane, far_plane
    );
}

void Camera::render_frame(World* world) {
    if (!renderer || !world) return;
    
    update_matrices();
    
    renderer->set_view_matrix(view_matrix);
    renderer->set_projection_matrix(projection_matrix);
    
    // Отрисовка всех чанков
    // (реализация будет зависеть от конкретной реализации Render)
}


