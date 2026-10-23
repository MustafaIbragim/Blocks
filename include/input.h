#pragma once

#include <array>
#include <cstdint>

struct GLFWwindow;

class Input {
public:
    static constexpr int MAX_KEYS = 512;
    static constexpr int MAX_MOUSE_BUTTONS = 8;

    explicit Input(uintptr_t window_handle);

    bool is_key_pressed(int key) const;
    bool is_key_just_pressed(int key) const;
    bool is_key_just_released(int key) const;

    bool is_mouse_button_pressed(int button) const;
    bool is_mouse_button_just_pressed(int button) const;
    bool is_mouse_button_just_released(int button) const;

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

    std::array<bool, MAX_KEYS> key_current{};
    std::array<bool, MAX_KEYS> key_previous{};
    std::array<bool, MAX_MOUSE_BUTTONS> mouse_current{};
    std::array<bool, MAX_MOUSE_BUTTONS> mouse_previous{};

    static bool valid_key(int key) {
        return key >= 0 && key < MAX_KEYS;
    }
    static bool valid_mouse_button(int button) {
        return button >= 0 && button < MAX_MOUSE_BUTTONS;
    }

    void handle_mouse_move(double xpos, double ypos);
    void handle_key(int key, int action);
    void handle_mouse_button(int button, int action);

    static void mouse_callback_static(GLFWwindow* window, double xpos, double ypos);
    static void key_callback_static(GLFWwindow* window, int key, int scancode,
                                    int action, int mods);
    static void mouse_button_callback_static(GLFWwindow* window, int button,
                                             int action, int mods);
};
