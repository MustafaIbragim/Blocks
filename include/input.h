#pragma once

#include <cstdint>

struct GLFWwindow;

class Input {
public:
    explicit Input(uintptr_t window_handle);

    bool is_key_pressed(int key) const;
    void get_mouse_delta(float& dx, float& dy);
    void set_cursor_visible(bool visible);
    void poll_events();

private:
    GLFWwindow* window;
    
    float mouse_dx = 0.0f;
    float mouse_dy = 0.0f;
    double last_mouse_x = 0.0;
    double last_mouse_y = 0.0;
    bool first_mouse = true;

    void handle_mouse_move(double xpos, double ypos);
    static void mouse_callback_static(GLFWwindow* window, double xpos, double ypos);
};


