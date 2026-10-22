#include "input.h"
#include <GLFW/glfw3.h>


Input::Input(uintptr_t window_handle) 
    : window(reinterpret_cast<GLFWwindow*>(window_handle)) {
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, mouse_callback_static);
}

bool Input::is_key_pressed(int key) const {
    return glfwGetKey(window, key) == GLFW_PRESS;
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

void Input::mouse_callback_static(GLFWwindow* window, double xpos, double ypos) {
    Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (input) {
        input->handle_mouse_move(xpos, ypos);
    }
}


