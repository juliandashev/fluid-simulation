#pragma once

#include <glad/gl.h>
#include <glm/vec2.hpp>
#include <vector>

#include "particle.hpp"
#include "shader.hpp"

// Owns all GPU state (VAOs, VBOs, shaders) and handles the CPU -> GPU
// uploads + draws each frame. Requires a current GL context.
class Renderer {
public:
    explicit Renderer(size_t max_particles);
    ~Renderer();

    // GL handles can't be duplicated safely, so copying is forbidden
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Fullscreen background coloured by the density field. Draw this FIRST
    // each frame (it clears the screen); particles/lines paint on top.
    void draw_density_field(const std::vector<Particle>& particles);

    void render(const std::vector<Particle>& particles);

    // Draws a flat list of vertices as GL_LINES (each pair is one segment).
    // Call after render() so the lines paint on top of the particles.
    void draw_lines(const std::vector<glm::vec2>& vertices);

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint speed_vbo_ = 0;
    Shader shader_;

    GLuint line_vao_ = 0;
    GLuint line_vbo_ = 0;
    Shader line_shader_;

    GLuint quad_vao_ = 0;
    GLuint quad_vbo_ = 0;
    Shader density_shader_;

    std::vector<glm::vec2> positions_;  // reused staging buffer, avoids per-frame allocation
    std::vector<float> speeds_;         // per-particle speed, uploaded alongside positions
};
