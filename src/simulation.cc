#include "simulation.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>

#include <algorithm>

#include "defs.hpp"

namespace fluid {

Simulation::Simulation(const Resolution& res)
    : step_(SHADER_DIR "/step.comp"),
      midpoint_(SHADER_DIR "/midpoint.comp"),
      density_(SHADER_DIR "/density.comp"),
      force_(SHADER_DIR "/force.comp"),
      count_shader_(SHADER_DIR "/count.comp"),
      scan_(SHADER_DIR "/scan.comp"),
      scatter_(SHADER_DIR "/scatter.comp"),
      reduce_max_(SHADER_DIR "/reduce_max.comp"),
      res_(res), count_(res.count) {
    // Allocate only; spawn_particles() fills positions/velocities.
    glGenBuffers(1, &position_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, position_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count_ * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &velocity_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocity_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count_ * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    // RK2 midpoint state, written and read entirely on the GPU.
    glGenBuffers(1, &mid_position_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mid_position_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count_ * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &mid_velocity_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mid_velocity_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count_ * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &density_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, density_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count_ * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &acceleration_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, acceleration_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count_ * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &speed_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, speed_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count_ * sizeof(float_t), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &max_kinematics_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, max_kinematics_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(GLuint), nullptr, GL_DYNAMIC_READ);

    // Solid geometry. Sized for real at spawn_solids(); one element keeps the
    // bindings legal while a scene has no obstacles.
    for (GLuint* b : {&solid_position_ssbo_, &solid_sorted_indices_}) {
        glGenBuffers(1, b);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, *b);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2), nullptr, GL_STATIC_DRAW);
    }

    for (GLuint* b : {&solid_cell_counts_, &solid_cell_starts_}) {
        glGenBuffers(1, b);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, *b);
        glBufferData(GL_SHADER_STORAGE_BUFFER, res_.num_cells * sizeof(GLuint), nullptr, GL_STATIC_DRAW);
    }

    glGenBuffers(1, &cell_counts_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cell_counts_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, res_.num_cells * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &cell_starts_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cell_starts_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, res_.num_cells * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &cell_cursors_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cell_cursors_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, res_.num_cells * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &sorted_indices_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorted_indices_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count_ * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    set_static_uniforms();
    set_periodic_x(0.0f);
}

Simulation::~Simulation() {
    glDeleteBuffers(1, &position_ssbo_);
    glDeleteBuffers(1, &velocity_ssbo_);
    glDeleteBuffers(1, &mid_position_ssbo_);
    glDeleteBuffers(1, &mid_velocity_ssbo_);
    glDeleteBuffers(1, &density_ssbo_);
    glDeleteBuffers(1, &acceleration_ssbo_);
    glDeleteBuffers(1, &speed_ssbo_);
    glDeleteBuffers(1, &max_kinematics_ssbo_);
    glDeleteBuffers(1, &solid_position_ssbo_);
    glDeleteBuffers(1, &solid_cell_counts_);
    glDeleteBuffers(1, &solid_cell_starts_);
    glDeleteBuffers(1, &solid_sorted_indices_);
    glDeleteBuffers(1, &cell_counts_);
    glDeleteBuffers(1, &cell_starts_);
    glDeleteBuffers(1, &cell_cursors_);
    glDeleteBuffers(1, &sorted_indices_);
}

void Simulation::step(float_t delta_time, const SimParams& params) {
    // buffers that keep their slot across the whole step
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, density_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, acceleration_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cell_counts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, cell_starts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, cell_cursors_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, sorted_indices_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, speed_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, mid_position_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, mid_velocity_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, solid_position_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, solid_cell_counts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, solid_cell_starts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, solid_sorted_indices_);

    const GLuint groups = (count_ + 255) / 256;

    // per-frame uniforms; they persist, so both pipeline runs see them
    force_.use();
    force_.set_float("u_target_density", params.target_density);
    force_.set_float("u_pressure_multiplier", params.pressure_multiplier);
    force_.set_float("u_viscosity", params.viscosity);
    force_.set_float("u_tension_threshold", params.tension_threshold);
    force_.set_vec2("u_interaction_point", interaction_point_);
    force_.set_float("u_interaction_strength", interaction_strength_);
    force_.set_float("u_tension_strength", params.tension_strength);
    force_.set_float("u_cohesion_strength", params.cohesion_strength);

    const glm::vec2 body_accel(params.body_accel_x, 0.0f);

    midpoint_.use();
    midpoint_.set_float("u_dt", delta_time);
    midpoint_.set_float("u_gravity", params.gravity);
    midpoint_.set_vec2("u_body_accel", body_accel);

    step_.use();
    step_.set_float("u_dt", delta_time);
    step_.set_float("u_gravity", params.gravity);
    step_.set_vec2("u_body_accel", body_accel);

    // RK2 stage 1: a1 at the current state, then the half-step state from it
    compute_accelerations(position_ssbo_, velocity_ssbo_);

    midpoint_.use();
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // RK2 stage 2: a2 at the midpoint state
    compute_accelerations(mid_position_ssbo_, mid_velocity_ssbo_);

    // final advance of the real state: pos moves with mid velocity, vel with a2
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, position_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocity_ssbo_);

    step_.use();
    glDispatchCompute(groups, 1, 1);
    // VERTEX_ATTRIB for the renderer, BUFFER_UPDATE for history copies and readbacks.
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
}

// Grid + density + force at the given state. All shaders read slots 0/1, so
// the midpoint evaluation is just a matter of which buffers get bound.
void Simulation::spawn_at(const std::vector<glm::vec2>& positions) {
    upload_state(positions);
    debug_density_stats(TARGET_DENSITY);
}

// Moves the grid origin with it: the cells have to tile the wrapped span, not
// the window, or a neighbour crossing the seam arrives at the wrong offset.
void Simulation::set_periodic_x(float_t period) {
    period_x_ = period;

    const glm::vec2 origin(period > 0.0f ? -0.5f * period : -DOMAIN_HALF_X, DOMAIN_MIN);

    for (gl::Shader* s : {&count_shader_, &scatter_, &density_, &force_}) {
        s->use();
        s->set_vec2("u_grid_origin", origin);
    }

    for (gl::Shader* s : {&density_, &force_, &step_}) {
        s->use();
        s->set_float("u_period_x", period);
    }
}

void Simulation::set_wall_slip(glm::vec2 slip) {
    force_.use();
    force_.set_vec2("u_wall_slip", slip);
}

void Simulation::spawn_solids(const std::vector<glm::vec2>& positions) {
    solid_count_ = static_cast<uint32_t>(positions.size());
    const std::size_t n = std::max<std::size_t>(1, solid_count_);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, solid_position_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, n * sizeof(glm::vec2),
                 solid_count_ > 0 ? positions.data() : nullptr, GL_STATIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, solid_sorted_indices_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, n * sizeof(GLuint), nullptr, GL_STATIC_DRAW);

    for (gl::Shader* s : {&density_, &force_}) {
        s->use();
        s->set_uint("u_num_solids", solid_count_);
    }

    build_solid_grid();

    if (solid_count_ > 0) {
        std::cout << "Solid geometry: " << solid_count_ << " boundary particles\n";
    }
}

// Solids never move, so this runs once per spawn rather than once per step -
// no count/scan/scatter in the hot loop, and no barrier to sequence it against.
void Simulation::build_solid_grid() {
    GLuint zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, solid_cell_counts_);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    if (solid_count_ > 0) {
        // The fluid's own three grid shaders, pointed at the solid buffers.
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, solid_position_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, solid_cell_counts_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, solid_cell_starts_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, cell_cursors_);  // scratch either way
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, solid_sorted_indices_);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        for (gl::Shader* s : {&count_shader_, &scatter_}) {
            s->use();
            s->set_uint("u_num_particles", solid_count_);
        }

        const GLuint groups = (solid_count_ + 255) / 256;

        count_shader_.use();
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        scan_.use();
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        scatter_.use();
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Restore the fluid count; step() reuses these same two shaders.
        for (gl::Shader* s : {&count_shader_, &scatter_}) {
            s->use();
            s->set_uint("u_num_particles", count_);
        }
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, solid_position_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, solid_cell_counts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, solid_cell_starts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, solid_sorted_indices_);
}

void Simulation::compute_accelerations(GLuint positions, GLuint velocities) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positions);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocities);

    const GLuint groups = (count_ + 255) / 256;

    // grid rebuild: zero counts, count, scan, scatter
    GLuint zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cell_counts_);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    count_shader_.use();
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    scan_.use();
    glDispatchCompute(1, 1, 1);  // deliberately single-invocation
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    scatter_.use();
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    density_.use();
    glDispatchCompute(groups, 1, 1);
    // BUFFER_UPDATE too: density_stats() reads this back with glGetBufferSubData.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    force_.use();
    glDispatchCompute(groups, 1, 1);
    // BUFFER_UPDATE too: accel_stats() reads this back with glGetBufferSubData.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
}

// Reads exponent bits, not std::isfinite, which -ffast-math breaks.
namespace {

float_t decode_max_bits(GLuint bits) {
    if ((bits & 0x7F800000u) == 0x7F800000u) {
        return 1e6f;
    }

    float_t value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace

// Once per rendered frame, not per substep. Values are one frame stale; the
// CFL safety factors absorb that.
glm::vec2 Simulation::max_kinematics() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, acceleration_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, speed_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, max_kinematics_ssbo_);

    GLuint zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, max_kinematics_ssbo_);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    reduce_max_.use();
    glDispatchCompute((count_ + 255) / 256, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);  // atomics land before the readback

    GLuint bits[2] = {0, 0};
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(bits), bits);

    return glm::vec2(decode_max_bits(bits[0]), decode_max_bits(bits[1]));
}

void Simulation::spawn_particles(bool is_random, float_t rest_density) {
    if (is_random) {
        std::random_device rd;
        uint32_t seed = rd();
        std::cout << "Seed: " << seed << "\n";
        create_particles(seed);
    } else {
        create_particles(rest_density);
    }

    debug_density_stats(rest_density);
}

void Simulation::create_particles(float_t rest_density) {
    std::vector<glm::vec2> positions;
    positions.reserve(count_);

    // Centered block at the rest-density pitch sqrt(m/rho_0), so it spawns in
    // equilibrium. Buffers are fixed at count_, so stop there.
    const int32_t per_row =
        static_cast<int32_t>(std::ceil(std::sqrt(static_cast<float_t>(count_))));
    const float_t spacing = std::sqrt(res_.mass / rest_density);
    const float_t offset = (per_row - 1) * spacing * 0.5f;

    for (int32_t row = 0; row < per_row && positions.size() < count_; ++row) {
        for (int32_t col = 0; col < per_row && positions.size() < count_; ++col) {
            positions.emplace_back(col * spacing - offset, row * spacing - offset);
        }
    }

    upload_state(positions);
}

void Simulation::spawn_dam_break(float_t rest_density) {
    create_column(rest_density);
    debug_density_stats(rest_density);
}

// Dam break reservoir: a column of aspect ratio (rows/cols) against the left
// wall, at the same rest-density pitch as the block spawn.
void Simulation::create_column(float_t rest_density) {
    std::vector<glm::vec2> positions;
    positions.reserve(count_);

    const float_t spacing = std::sqrt(res_.mass / rest_density);

    // cols*rows = count_ with rows/cols = aspect. Height is clamped to the
    // domain first and the width re-derived, so nothing spawns out of bounds.
    const int32_t max_rows =
        static_cast<int32_t>((DOMAIN_MAX - DOMAIN_MIN - 2.0f * EPS) / spacing);

    int32_t cols = std::max(1, static_cast<int32_t>(std::floor(
                                   std::sqrt(static_cast<float_t>(count_) / res_.aspect))));
    int32_t rows = static_cast<int32_t>(std::ceil(static_cast<float_t>(count_) / cols));

    if (rows > max_rows) {
        rows = max_rows;
        cols = static_cast<int32_t>(std::ceil(static_cast<float_t>(count_) / rows));
        std::cout << "Dam break: aspect clamped to fit the domain (" << rows << " rows)\n";
    }

    const float_t x0 = -DOMAIN_HALF_X + EPS + 0.5f * spacing;
    const float_t y0 = DOMAIN_MIN + EPS + 0.5f * spacing;

    for (int32_t row = 0; row < rows && positions.size() < count_; ++row) {
        for (int32_t col = 0; col < cols && positions.size() < count_; ++col) {
            positions.emplace_back(x0 + col * spacing, y0 + row * spacing);
        }
    }

    column_origin_x_ = x0;
    column_width_ = cols * spacing;
    column_height_ = rows * spacing;

    std::cout << "Dam break column: a = " << column_width_ << ", height = " << column_height_
              << " (" << cols << " x " << rows << " particles)\n";

    upload_state(positions);
}

std::vector<glm::vec2> Simulation::read_positions() const {
    std::vector<glm::vec2> positions(count_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, position_ssbo_);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(glm::vec2), positions.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return positions;
}

void Simulation::create_particles(uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float_t> dist(-0.5f, 0.5f);

    const float_t bound_size = DOMAIN_MAX - DOMAIN_MIN;

    std::vector<glm::vec2> positions;
    positions.reserve(count_);

    for (uint32_t i = 0; i < count_; ++i) {
        positions.emplace_back(dist(rng) * bound_size, dist(rng) * bound_size);
    }

    upload_state(positions);
}

void Simulation::upload_state(const std::vector<glm::vec2>& positions) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, position_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(glm::vec2), positions.data());

    // Velocities start at rest on every (re)spawn.
    std::vector<glm::vec2> zeros(count_, glm::vec2(0.0f));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocity_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(glm::vec2), zeros.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Speeds are per-particle state too: clear them so a fresh spawn
    // renders calm instead of wearing the previous frame's colors.
    float_t zero = 0.0f;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, speed_ssbo_);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32F, GL_RED, GL_FLOAT, &zero);
}

void Simulation::set_static_uniforms() {
    const glm::vec2 origin(-DOMAIN_HALF_X, DOMAIN_MIN);

    for (gl::Shader* s : {&count_shader_, &scatter_, &density_, &force_}) {
        s->use();  // uniforms go to the *currently bound* program
        s->set_uint("u_num_particles", count_);
        s->set_vec2("u_grid_origin", origin);
        s->set_float("u_cell_size", res_.cell_size);
        s->set_int("u_grid_w", res_.grid_w);
        s->set_int("u_grid_h", res_.grid_h);
    }

    scan_.use();
    scan_.set_int("u_num_cells", res_.num_cells);

    midpoint_.use();
    midpoint_.set_uint("u_num_particles", count_);

    reduce_max_.use();
    reduce_max_.set_uint("u_num_particles", count_);

    // Domain bounds reach density/force too: both build the wall images.
    for (gl::Shader* s : {&density_, &force_}) {
        s->use();
        s->set_float("u_kernel_radius", res_.kernel_radius);
        s->set_float("u_kernel_radius_sq", res_.kernel_radius_sq);
        s->set_float("u_mass", res_.mass);
        s->set_float("u_domain_half_x", DOMAIN_HALF_X);
        s->set_float("u_domain_min", DOMAIN_MIN);
        s->set_float("u_domain_max", DOMAIN_MAX);
        s->set_float("u_wall_offset", res_.wall_offset);
    }

    for (gl::Shader* s : {&density_, &force_}) {
        s->use();
        s->set_uint("u_num_solids", solid_count_);
    }

    force_.use();
    force_.set_float("u_interaction_radius", INTERACTION_RADIUS);
    force_.set_vec2("u_wall_slip", {WALL_SLIP_FLOOR, WALL_SLIP_SIDE});
    force_.set_float("u_solid_slip", SOLID_SLIP);

    step_.use();
    step_.set_uint("u_num_particles", count_);
    step_.set_float("u_domain_half_x", DOMAIN_HALF_X);
    step_.set_float("u_domain_min", DOMAIN_MIN);
    step_.set_float("u_domain_max", DOMAIN_MAX);
    step_.set_float("u_bounce_damping", BOUNCE_DAMPING);
    step_.set_float("u_eps", EPS);
    step_.set_float("u_max_speed", MAX_SPEED);
}

std::vector<float_t> Simulation::read_speeds() const {
    std::vector<float_t> speeds(count_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, speed_ssbo_);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(float_t), speeds.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return speeds;
}

// The buffer is vec2 with the value in .x, so this strips the padding.
std::vector<float_t> Simulation::read_densities() const {
    std::vector<glm::vec2> raw(count_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, density_ssbo_);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(glm::vec2), raw.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::vector<float_t> out(count_);
    for (uint32_t i = 0; i < count_; ++i) {
        out[i] = raw[i].x;
    }
    return out;
}

log::DensityStats Simulation::density_stats() const {
    log::DensityStats stats;
    if (count_ == 0) {
        return stats;
    }

    std::vector<glm::vec2> densities(count_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, density_ssbo_);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(glm::vec2), densities.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::vector<float_t> rho(count_);
    for (uint32_t i = 0; i < count_; ++i) {
        rho[i] = densities[i].x;
    }

    // Two passes: sigma is ~1% of the mean, so E[x^2] - E[x]^2 cancels badly.
    float_t mean = 0.0f;
    for (float_t r : rho) {
        mean += r;
    }
    mean /= static_cast<float_t>(count_);

    float_t variance = 0.0f;
    for (float_t r : rho) {
        variance += (r - mean) * (r - mean);
    }
    stats.sigma = std::sqrt(variance / static_cast<float_t>(count_));

    // Quickselect; the p90 pass starts from the already-partitioned median.
    const auto mid = rho.begin() + count_ / 2;
    std::nth_element(rho.begin(), mid, rho.end());
    stats.median = *mid;

    const auto p90 = rho.begin() + (count_ * 9) / 10;
    std::nth_element(mid, p90, rho.end());
    stats.p90 = *p90;

    const auto [lo, hi] = std::minmax_element(rho.begin(), rho.end());
    stats.min = *lo;
    stats.max = *hi;

    return stats;
}

// Ranked on |a + g|, not |a|: force.comp excludes gravity, so this puts
// equilibrium at 0 rather than 9.81.
log::AccelStats Simulation::accel_stats(float_t gravity) const {
    log::AccelStats stats;
    if (count_ == 0) {
        return stats;
    }

    std::vector<glm::vec2> accels(count_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, acceleration_ssbo_);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(glm::vec2), accels.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::vector<float_t> mag(count_);
    const glm::vec2 g(0.0f, gravity);
    for (uint32_t i = 0; i < count_; ++i) {
        mag[i] = glm::length(accels[i] + g);
    }

    const auto mid = mag.begin() + count_ / 2;
    std::nth_element(mag.begin(), mid, mag.end());
    stats.median = *mid;

    const auto p90 = mag.begin() + (count_ * 9) / 10;
    std::nth_element(mid, p90, mag.end());
    stats.p90 = *p90;

    const auto [lo, hi] = std::minmax_element(mag.begin(), mag.end());
    stats.min = *lo;
    stats.max = *hi;

    return stats;
}

void Simulation::dump_particles(const std::string& path, float_t gravity) const {
    if (count_ == 0) {
        return;
    }

    std::vector<glm::vec2> pos(count_), acc(count_), den(count_);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, position_ssbo_);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(glm::vec2), pos.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, acceleration_ssbo_);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(glm::vec2), acc.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, density_ssbo_);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count_ * sizeof(glm::vec2), den.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    const glm::vec2 g(0.0f, gravity);

    std::ofstream out(path);
    // wall_dist is to the nearest of the four faces.
    out << "x,y,rho,ax,ay,res,wall_dist\n";
    for (uint32_t i = 0; i < count_; ++i) {
        const float_t dx = std::min(DOMAIN_HALF_X - pos[i].x, pos[i].x + DOMAIN_HALF_X);
        const float_t dy = std::min(DOMAIN_MAX - pos[i].y, pos[i].y - DOMAIN_MIN);
        out << pos[i].x << ',' << pos[i].y << ',' << den[i].x << ','
            << acc[i].x << ',' << acc[i].y << ','
            << glm::length(acc[i] + g) << ',' << std::min(dx, dy) << '\n';
    }

    std::cout << "dumped " << count_ << " particles to " << path << std::endl;
}

// One-shot diagnostic: run the pipeline at the current state and report the
// density distribution. Interior particles should sit at TARGET_DENSITY.
void Simulation::debug_density_stats(float_t rest_density) {
    // Slots 2-7 are normally bound at the top of step(); this may run before
    // any step, so the grid/density/force chain needs them bound here too.
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, density_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, acceleration_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cell_counts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, cell_starts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, cell_cursors_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, sorted_indices_);

    compute_accelerations(position_ssbo_, velocity_ssbo_);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);  // densities land before readback

    const log::DensityStats stats = density_stats();
    const float_t spread = rest_density > 0.0f ? 100.0f * stats.sigma / rest_density : 0.0f;

    std::cout << "density  min=" << stats.min << "  median=" << stats.median
              << "  p90=" << stats.p90 << "  max=" << stats.max
              << "  sigma/rho_0=" << spread << "% (target ~1%)" << std::endl;
}

}  // namespace fluid
