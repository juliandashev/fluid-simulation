#include "renderer.hpp"

#include <glm/geometric.hpp>

#include "defs.hpp"

Renderer::Renderer(size_t max_particles)
    : shader_(SHADER_DIR "/particle.vert", SHADER_DIR "/particle.frag"),
      line_shader_(SHADER_DIR "/line.vert", SHADER_DIR "/line.frag") {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, max_particles * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
    glEnableVertexAttribArray(0);

    // Per-particle speed at location 1. Configured while vao_ is still bound so
    // the VAO records "location 1 reads from speed_vbo_, one tightly-packed float".
    glGenBuffers(1, &speed_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, speed_vbo_);
    glBufferData(GL_ARRAY_BUFFER, max_particles * sizeof(float_t), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float_t), nullptr);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

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
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
    glDeleteBuffers(1, &speed_vbo_);
    glDeleteVertexArrays(1, &line_vao_);
    glDeleteBuffers(1, &line_vbo_);
}

void Renderer::render(const std::vector<Particle>& particles) {
    positions_.clear();
    speeds_.clear();

    // Per-particle position + speed. Color keys off an absolute reference
    // (SPEED_COLOR_MAX), so a calm fluid stays blue instead of being re-stretched
    // across the whole palette every frame.
    for (const Particle& p : particles) {
        positions_.push_back(p.position);
        speeds_.push_back(glm::length(p.velocity));
    }

    shader_.use();
    shader_.set_float("u_point_size", POINT_SIZE);
    shader_.set_float("u_domain_half", DOMAIN_MAX);
    shader_.set_float("u_max_speed", SPEED_COLOR_MAX);

    // Correct for non-square windows: the viewport stretches NDC's square to the
    // window's aspect, so divide x by w/h to keep the domain (and circles) undistorted.
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    shader_.set_float("u_aspect", static_cast<float_t>(vp[2]) / static_cast<float_t>(vp[3]));

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions_.size() * sizeof(glm::vec2), positions_.data());

    glBindBuffer(GL_ARRAY_BUFFER, speed_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, speeds_.size() * sizeof(float_t), speeds_.data());

    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(positions_.size()));
    glBindVertexArray(0);
}

void Renderer::draw_lines(const std::vector<glm::vec2>& vertices) {
    if (vertices.empty()) {
        return;
    }

    line_shader_.use();
    line_shader_.set_float("u_domain_half", DOMAIN_MAX);

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    line_shader_.set_float("u_aspect", static_cast<float_t>(vp[2]) / static_cast<float_t>(vp[3]));

    glLineWidth(ARROW_THICKNESS);

    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec2), vertices.data());
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
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
