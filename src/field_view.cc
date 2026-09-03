#include "field_view.hpp"

#include <cmath>

#include <glm/vec2.hpp>

#include "simulation.hpp"

namespace fluid {
namespace gl {

FieldView::FieldView(int32_t width, int32_t height)
    : width_(width), height_(height), sample_(SHADER_DIR "/field.comp"),
      present_(SHADER_DIR "/field_view.vert", SHADER_DIR "/field_view.frag") {
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width_, height_);

    // Bilinear: the grid is coarser than the window and the field is smooth.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenVertexArrays(1, &vao_);  // empty: the vertex shader builds the triangle
}

FieldView::~FieldView() {
    glDeleteTextures(1, &texture_);
    glDeleteVertexArrays(1, &vao_);
}

void FieldView::update(const Simulation& sim, const SimParams& params) {
    const Resolution& res = sim.resolution();
    const float_t period = sim.period_x();
    const float_t radius = params.render_smoothing * res.kernel_radius;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sim.position_buffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sim.density_buffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, sim.cell_counts_buffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, sim.cell_starts_buffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, sim.sorted_indices_buffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, sim.speed_buffer());

    sample_.use();
    sample_.set_ivec2("u_field_size", width_, height_);
    sample_.set_float("u_render_radius", radius);
    sample_.set_float("u_render_radius_sq", radius * radius);

    // Cells are h across; a support wider than h needs a wider walk to match.
    sample_.set_int("u_rings", static_cast<int32_t>(std::ceil(radius / res.cell_size)));

    sample_.set_float("u_target_density", params.target_density);
    sample_.set_float("u_pressure_multiplier", params.pressure_multiplier);

    // The grid uniforms Simulation sets on its own programs; this one is separate.
    sample_.set_float("u_mass", res.mass);
    sample_.set_float("u_cell_size", res.cell_size);
    sample_.set_int("u_grid_w", res.grid_w);
    sample_.set_int("u_grid_h", res.grid_h);
    sample_.set_float("u_period_x", period);
    sample_.set_vec2("u_grid_origin",
                     glm::vec2(period > 0.0f ? -0.5f * period : -DOMAIN_HALF_X, DOMAIN_MIN));
    sample_.set_float("u_domain_half_x", period > 0.0f ? 0.5f * period : DOMAIN_HALF_X);
    sample_.set_float("u_domain_min", DOMAIN_MIN);
    sample_.set_float("u_domain_max", DOMAIN_MAX);

    glBindImageTexture(0, texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

}

void FieldView::draw(ColorField field, const SimParams& params) const {
    present_.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);

    present_.set_int("u_field", 0);
    present_.set_int("u_color_field", static_cast<int32_t>(field));
    present_.set_float("u_max_speed", SPEED_COLOR_MAX);
    present_.set_float("u_min_pressure", params.pressure_min);
    present_.set_float("u_max_pressure", params.pressure_max);
    present_.set_float("u_coverage_min", 0.5f);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

}  // namespace gl
}  // namespace fluid
