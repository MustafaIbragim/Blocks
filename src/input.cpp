#include "input.h"

#include <GLFW/glfw3.h>


Input::Input(uintptr_t window_handle) 
    : window(reinterpret_cast<GLFWwindow*>(window_handle)) {
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, mouse_callback_static);
    glfwSetKeyCallback(window, key_callback_static);
    glfwSetMouseButtonCallback(window, mouse_button_callback_static);
}

bool Input::is_key_pressed(int key) const {
    return glfwGetKey(window, key) == GLFW_PRESS;
}

bool Input::is_key_just_pressed(int key) const {
    if (!valid_key(key)) return false;
    return key_current[key] && !key_previous[key];
}

bool Input::is_key_just_released(int key) const {
    if (!valid_key(key)) return false;
    return !key_current[key] && key_previous[key];
}

bool Input::is_mouse_button_pressed(int button) const {
    if (!valid_mouse_button(button)) return false;
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
}

bool Input::is_mouse_button_just_pressed(int button) const {
    if (!valid_mouse_button(button)) return false;
    return mouse_current[button] && !mouse_previous[button];
}

bool Input::is_mouse_button_just_released(int button) const {
    if (!valid_mouse_button(button)) return false;
    return !mouse_current[button] && mouse_previous[button];
}

void Input::get_mouse_delta(float& dx, float& dy) {
    dx = mouse_dx;
    dy = mouse_dy;
    mouse_dx = 0.0f;
    mouse_dy = 0.0f;
}

void Input::set_cursor_visible(bool visible) {
    glfwSetInputMode(window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

void Input::poll_events() {
    // Снимок состояния до poll нужен для just_pressed/just_released:
    // callback'и GLFW обновят current уже внутри glfwPollEvents().
    key_previous = key_current;
    mouse_previous = mouse_current;
    glfwPollEvents();
}

void Input::handle_mouse_move(double xpos, double ypos) {
    if (first_mouse) {
        last_mouse_x = xpos;
        last_mouse_y = ypos;
        first_mouse = false;
    }
    
    mouse_dx = static_cast<float>(xpos - last_mouse_x);
    mouse_dy = static_cast<float>(last_mouse_y - ypos);
    
    last_mouse_x = xpos;
    last_mouse_y = ypos;
}

void Input::handle_key(int key, int action) {
    if (!valid_key(key)) return;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        key_current[key] = true;
    } else if (action == GLFW_RELEASE) {
        key_current[key] = false;
    }
}

void Input::handle_mouse_button(int button, int action) {
    if (!valid_mouse_button(button)) return;
    if (action == GLFW_PRESS) {
        mouse_current[button] = true;
    } else if (action == GLFW_RELEASE) {
        mouse_current[button] = false;
    }
}

void Input::mouse_callback_static(GLFWwindow* window, double xpos, double ypos) {
    Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (input) {
        input->handle_mouse_move(xpos, ypos);
    }
}

void Input::key_callback_static(GLFWwindow* window, int key, int scancode,
                                int action, int mods) {
    (void)scancode;
    (void)mods;
    Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (input) {
        input->handle_key(key, action);
    }
}

void Input::mouse_button_callback_static(GLFWwindow* window, int button,
                                         int action, int mods) {
    (void)mods;
    Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (input) {
        input->handle_mouse_button(button, action);
    }
}
