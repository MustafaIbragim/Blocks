#include "camera.h"

#include "renderer.h"


Camera::Camera() {
    dynamic = false;
}

Camera::Camera(Renderer* renderer)
    : renderer(renderer) {
    dynamic = false;
}

Camera::Camera(Renderer* renderer, const ViewCam& view)
    : view(view), renderer(renderer) {
    dynamic = false;
}

void Camera::sync_view_from_transform() {
    view.position = transform.position;
    view.rotation = transform.rotation;
}

void Camera::sync_transform_from_view() {
    transform.position = view.position;
    transform.rotation = view.rotation;
}

void Camera::update(uint32_t tick, float gravity) {
    Entity::update(tick, gravity);
    sync_view_from_transform();
}

void Camera::render(uint64_t current_tick, float min_light, float day_light) {
    if (!renderer) {
        return;
    }
    renderer->render(current_tick, view, min_light, day_light);
}
