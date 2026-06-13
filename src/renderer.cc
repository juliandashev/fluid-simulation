#include "renderer.hpp"

#include "defs.hpp"

Renderer::Renderer(size_t max_particles)
    : shader_(SHADER_DIR "/particle.vert", SHADER_DIR "/particle.frag") {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, max_particles * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glEnable(GL_PROGRAM_POINT_SIZE);

    positions_.reserve(max_particles);
}

Renderer::~Renderer() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
}

void Renderer::render(const std::vector<Particle>& particles) {
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    positions_.clear();
    for (const Particle& p : particles) {
        positions_.push_back(p.position);
    }

    shader_.use();
    shader_.set_float("u_point_size", POINT_SIZE);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions_.size() * sizeof(glm::vec2), positions_.data());
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(positions_.size()));
    glBindVertexArray(0);
}
