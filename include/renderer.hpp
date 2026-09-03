#pragma once

#include <glad/gl.h>

#include <glm/vec2.hpp>
#include <vector>

#include "color_field.hpp"
#include "defs.hpp"
#include "obstacle.hpp"
#include "shader.hpp"

namespace fluid {
namespace gl {

// What the particle colour ramp maps; the sim buffer differs per field.
// Owns all GPU state (VAOs, VBOs, shaders) and handles the CPU -> GPU
// uploads + draws each frame. Requires a current GL context.
class Renderer {
public:
    explicit Renderer(size_t max_particles);
    ~Renderer();

    // GL handles can't be duplicated safely, so copying is forbidden
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void draw_circle(glm::vec2 center, float_t radius, glm::vec3 color);
    void draw_quads(const std::vector<obstacle::Quad>& quads);
    void render(GLuint position_buffer, GLuint field_buffer, ColorField field,
                const SimParams& params, size_t count);

private:
    Shader shader_;

    GLuint line_vao_ = 0;
    GLuint line_vbo_ = 0;
    Shader line_shader_;

    GLuint vao_ = 0;
    GLuint bound_positions_ = 0;  // handles baked into vao_; a change rebuilds it
    GLuint bound_field_ = 0;


    std::vector<glm::vec2> positions_;  // reused staging buffer, avoids per-frame allocation
};

}  // namespace gl
}  // namespace fluid
