#pragma once

#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>

#include <glm/vec2.hpp>

namespace fluid {
namespace ui {

class Input {
public:
    explicit Input(GLFWwindow* w) : window_(w) {}

    // True only on the frame the key goes down. One slot per keycode, so a new
    // binding costs nothing here - see bindings.hpp.
    bool pressed(int32_t key) {
        if (key < 0 || key > GLFW_KEY_LAST) {
            return false;
        }

        const int32_t now = glfwGetKey(window_, key);
        const bool is_edge = (now == GLFW_PRESS && prev_[key] == GLFW_RELEASE);
        prev_[key] = static_cast<uint8_t>(now);

        return is_edge;
    }

    bool held(int32_t key) const { return glfwGetKey(window_, key) == GLFW_PRESS; }

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
    GLFWwindow* window_;
    std::array<uint8_t, GLFW_KEY_LAST + 1> prev_{};
};

}  // namespace ui
}  // namespace fluid
