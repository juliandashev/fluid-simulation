#pragma once

#include <glad/gl.h>
#include <glm/vec2.hpp>
#include <vector>

#include "particle.hpp"
#include "shader.hpp"

// Owns all GPU state (VAO, VBO, shader) and handles the CPU -> GPU
// position upload + draw each frame. Requires a current GL context.
class Renderer {
public:
    explicit Renderer(size_t max_particles);
    ~Renderer();

    // GL handles can't be duplicated safely, so copying is forbidden
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void render(const std::vector<Particle>& particles);

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    Shader shader_;
    std::vector<glm::vec2> positions_;  // reused staging buffer, avoids per-frame allocation
};
