#pragma once

#include <GLFW/glfw3.h>

#include <glm/vec2.hpp>

namespace fluid {
namespace ui {

class Input {
public:
    explicit Input(GLFWwindow* w) : window_(w) {}
    bool is_space_key_pressed() { return edge(GLFW_KEY_SPACE, prev_space_); }
    bool is_left_arrow_key_pressed() { return edge(GLFW_KEY_LEFT, prev_left_); }
    bool is_right_arrow_key_pressed() { return edge(GLFW_KEY_RIGHT, prev_right_); }
    bool is_R_key_pressed() { return edge(GLFW_KEY_R, prev_r_); }
    bool is_P_key_pressed() { return edge(GLFW_KEY_P, prev_p_); }
    bool is_left_mouse_button_down() {
        return glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    }
    bool is_right_mouse_button_down() {
        return glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    }

    glm::vec2 cursor_pixels() {
        double x, y;
        glfwGetCursorPos(window_, &x, &y);
        return glm::vec2(x, y);
    }

private:
    bool edge(int32_t key, int32_t& prev) {
        int32_t now = glfwGetKey(window_, key);
        bool is_edge = (now == GLFW_PRESS && prev == GLFW_RELEASE);
        prev = now;

        return is_edge;
    }

    GLFWwindow* window_;
    int32_t prev_space_ = GLFW_RELEASE;
    int32_t prev_left_ = GLFW_RELEASE;
    int32_t prev_right_ = GLFW_RELEASE;
    int32_t prev_r_ = GLFW_RELEASE;
    int32_t prev_p_ = GLFW_RELEASE;
};

}  // namespace ui
}  // namespace fluid
