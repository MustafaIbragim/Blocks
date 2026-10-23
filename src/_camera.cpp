#include "_camera.h"

#include "renderer.h"


Camera::Camera() = default;

Camera::Camera(Renderer* renderer)
    : renderer(renderer) {}

Camera::Camera(Renderer* renderer, const ViewCam& view)
    : view(view), renderer(renderer) {}

void Camera::sync_view_from_transform() {
    view.position = transform.position;
    view.rotation = transform.rotation;
}

void Camera::sync_transform_from_view() {
    transform.position = view.position;
    transform.rotation = view.rotation;
}

void Camera::update(uint32_t tick) {
    (void)tick;
    sync_view_from_transform();
}

void Camera::render(uint64_t current_tick, float min_light) {
    if (!renderer) {
        return;
    }
    renderer->render(current_tick, view, min_light);
}
