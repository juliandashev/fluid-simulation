#include "renderer.hpp"

#include <glm/geometric.hpp>
#include <algorithm>

#include "defs.hpp"

Renderer::Renderer(size_t max_particles)
    : shader_(SHADER_DIR "/particle.vert", SHADER_DIR "/particle.frag"),
      line_shader_(SHADER_DIR "/line.vert", SHADER_DIR "/line.frag"),
      density_shader_(SHADER_DIR "/density.vert", SHADER_DIR "/density.frag") {
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
    glBufferData(GL_ARRAY_BUFFER, max_particles * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);
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

    // Fullscreen quad (triangle strip) for the density-field background.
    const float quad[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    glGenVertexArrays(1, &quad_vao_);
    glGenBuffers(1, &quad_vbo_);

    glBindVertexArray(quad_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
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
    glDeleteVertexArrays(1, &quad_vao_);
    glDeleteBuffers(1, &quad_vbo_);
}

void Renderer::draw_density_field(const std::vector<Particle>& particles) {
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    positions_.clear();
    std::vector<float> props, dens;

    // Auto-range the color scale to the data, so we don't retune a fixed
    // constant every time particle count or mass changes. Seed it tiny to
    // avoid a divide-by-zero when every density happens to equal the target.
    float max_dev = 1e-6f;
    for (const Particle& p : particles) {
        positions_.push_back(p.position);
        props.push_back(p.property);
        dens.push_back(p.density);

        max_dev = std::max(max_dev, std::abs(p.density - TARGET_DENSITY));
    }

    float color_scale = 1.0f / max_dev;

    // The shader's u_positions array is capped at MAX_RENDERED_PARTICLES (see density.frag).
    int count = positions_.size() > MAX_RENDERED_PARTICLES ? MAX_RENDERED_PARTICLES
                                                           : static_cast<int>(positions_.size());

    density_shader_.use();
    density_shader_.set_int("u_count", count);
    density_shader_.set_float("u_radius", KERNEL_RADIUS);
    density_shader_.set_float("u_mass", MASS);
    density_shader_.set_float("u_target", TARGET_DENSITY);
    density_shader_.set_float("u_scale", color_scale);
    density_shader_.set_float("u_domain_half", DOMAIN_MAX);
    density_shader_.set_vec2_array("u_positions", positions_.data(), count);
    density_shader_.set_float_array("u_properties", props.data(), count);
    density_shader_.set_float_array("u_densities", dens.data(), count);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void Renderer::render(const std::vector<Particle>& particles) {
    positions_.clear();
    speeds_.clear();

    // Auto-range the color scale to the frame's fastest particle, so the gradient
    // always spans blue->red without a hand-tuned constant. Tiny seed avoids a
    // divide-by-zero in the shader when every particle is at rest.
    float max_speed = 1e-6f;
    for (const Particle& p : particles) {
        positions_.push_back(p.position);
        float speed = glm::length(p.velocity);
        speeds_.push_back(speed);
        max_speed = std::max(max_speed, speed);
    }

    shader_.use();
    shader_.set_float("u_point_size", POINT_SIZE);
    shader_.set_float("u_domain_half", DOMAIN_MAX);
    shader_.set_float("u_max_speed", max_speed);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions_.size() * sizeof(glm::vec2), positions_.data());

    glBindBuffer(GL_ARRAY_BUFFER, speed_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, speeds_.size() * sizeof(float), speeds_.data());

    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(positions_.size()));
    glBindVertexArray(0);
}

void Renderer::draw_lines(const std::vector<glm::vec2>& vertices) {
    if (vertices.empty()) {
        return;
    }

    line_shader_.use();
    line_shader_.set_float("u_domain_half", DOMAIN_MAX);
    glLineWidth(ARROW_THICKNESS);

    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec2), vertices.data());
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}
