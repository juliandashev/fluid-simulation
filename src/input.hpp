#pragma once

#include <glm/vec2.hpp>
#include <GLFW/glfw3.h>

class Input {
public:
    explicit Input(GLFWwindow* w) : window_(w) {}
    bool space_pressed() { return edge(GLFW_KEY_SPACE, prev_space_); }
    bool left_pressed() { return edge(GLFW_KEY_LEFT, prev_left_); }
    bool right_pressed() { return edge(GLFW_KEY_RIGHT, prev_right_); }
    bool left_mouse_held() {
        return glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    }
    bool right_mouse_held() {
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
    int prev_space_ = GLFW_RELEASE, prev_left_ = GLFW_RELEASE, prev_right_ = GLFW_RELEASE;
};
