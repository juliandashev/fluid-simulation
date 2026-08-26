#include "renderer.hpp"

#include <glm/geometric.hpp>

#include "defs.hpp"

namespace fluid {
namespace gl {

Renderer::Renderer(size_t max_particles)
    : shader_(SHADER_DIR "/particle.vert", SHADER_DIR "/particle.frag"),
      line_shader_(SHADER_DIR "/line.vert", SHADER_DIR "/line.frag") {
    // Line buffer: each particle contributes one arrow = 3 segments (shaft + 2
    // barbs) = 6 vertices.
    glGenVertexArrays(1, &line_vao_);
    glGenBuffers(1, &line_vbo_);

    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
    glBufferData(GL_ARRAY_BUFFER, max_particles * 6 * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glEnable(GL_PROGRAM_POINT_SIZE);

    positions_.reserve(max_particles);
}

Renderer::~Renderer() {
    glDeleteVertexArrays(1, &line_vao_);
    glDeleteBuffers(1, &line_vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void Renderer::draw_circle(glm::vec2 center, float_t radius, glm::vec3 color) {
    positions_.clear();
    const int32_t segments = 48;

    for (int32_t i = 0; i < segments; ++i) {
        float_t angle = 2.0f * static_cast<float_t>(M_PI) * static_cast<float_t>(i) / segments;
        glm::vec2 point = center + radius * glm::vec2(std::cos(angle), std::sin(angle));
        positions_.push_back(point);
    }

    line_shader_.use();
    line_shader_.set_float("u_domain_half", DOMAIN_MAX);
    line_shader_.set_vec3("u_color", color);

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    line_shader_.set_float("u_aspect", static_cast<float_t>(vp[2]) / static_cast<float_t>(vp[3]));

    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions_.size() * sizeof(glm::vec2), positions_.data());
    glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(positions_.size()));
    glBindVertexArray(0);
}

// Static geometry, drawn as filled quads. The solver fills these shapes with
// particles; drawing the shape instead keeps the lattice out of the picture.
void Renderer::draw_quads(const std::vector<obstacle::Quad>& quads) {
    if (quads.empty()) {
        return;
    }

    positions_.clear();
    for (const obstacle::Quad& q : quads) {
        for (int32_t i : {0, 1, 2, 0, 2, 3}) {  // convex, so a fan of two triangles
            positions_.push_back(q.p[i]);
        }
    }

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    line_shader_.use();
    line_shader_.set_float("u_aspect", static_cast<float_t>(vp[2]) / static_cast<float_t>(vp[3]));
    line_shader_.set_float("u_domain_half", DOMAIN_MAX);
    line_shader_.set_vec3("u_color", glm::vec3(0.38f, 0.37f, 0.41f));

    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions_.size() * sizeof(glm::vec2), positions_.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(positions_.size()));
    glBindVertexArray(0);
}

void Renderer::render(GLuint position_buffer, GLuint field_buffer, ColorField field,
                      const SimParams& params, size_t count) {
    // VAO reads the sim's SSBOs directly as vertex attributes - no copy.
    if (vao_ == 0 || position_buffer != bound_positions_ || field_buffer != bound_field_) {
        if (vao_ == 0) {
            glGenVertexArrays(1, &vao_);
        }
        glBindVertexArray(vao_);
        bound_positions_ = position_buffer;
        bound_field_ = field_buffer;

        glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
        glEnableVertexAttribArray(0);

        // Densities are vec2 with the value in .x; speeds are bare floats.
        const GLsizei stride = field == ColorField::Pressure
                                   ? static_cast<GLsizei>(sizeof(glm::vec2))
                                   : static_cast<GLsizei>(sizeof(float_t));

        glBindBuffer(GL_ARRAY_BUFFER, field_buffer);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(1);
    }

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    shader_.use();
    shader_.set_float("u_aspect", static_cast<float_t>(vp[2]) / static_cast<float_t>(vp[3]));
    shader_.set_float("u_point_size",
                      params.draw_scale * PARTICLE_DRAW_SIZE * vp[3] / (DOMAIN_MAX - DOMAIN_MIN));
    shader_.set_float("u_domain_half", DOMAIN_MAX);
    shader_.set_float("u_max_speed", SPEED_COLOR_MAX);
    shader_.set_int("u_color_field", static_cast<int32_t>(field));
    shader_.set_float("u_target_density", params.target_density);
    shader_.set_float("u_pressure_multiplier", params.pressure_multiplier);
    shader_.set_float("u_max_pressure", PRESSURE_COLOR_MAX);

    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(count));

    glBindVertexArray(0);
}

}  // namespace gl
}  // namespace fluid
