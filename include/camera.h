#pragma once

#include <cstdint>

#include "entity.h"
#include "view_cam.h"


class Renderer;


// Камера-сущность. Хранит ViewCam и не владеющую ссылку на рендерер,
// чтобы её можно было удобно передавать в игровую логику.
class Camera : public Entity {
public:
    ViewCam view;
    Renderer* renderer = nullptr;

    Camera();
    explicit Camera(Renderer* renderer);
    Camera(Renderer* renderer, const ViewCam& view);

    void set_renderer(Renderer* value) { renderer = value; }
    Renderer* get_renderer() const { return renderer; }

    void set_view(const ViewCam& value) { view = value; }
    ViewCam& get_view() { return view; }
    const ViewCam& get_view() const { return view; }

    // Синхронизация между Transform сущности и ViewCam.
    void sync_view_from_transform();
    void sync_transform_from_view();

    void update(uint32_t tick, float gravity = 0.0f) override;

    // Отрисовка через сохранённый рендерер.
    // day_light: 0 — ночь, 1 — день.
    void render(uint64_t current_tick, float min_light = 0.1f,
                float day_light = 1.0f);
};
