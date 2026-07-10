#include "simulation.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <random>

#include "defs.hpp"

Simulation::Simulation(uint32_t count)
    : step_(SHADER_DIR "/step.comp"),
      midpoint_(SHADER_DIR "/midpoint.comp"),
      density_(SHADER_DIR "/density.comp"),
      force_(SHADER_DIR "/force.comp"),
      count_shader_(SHADER_DIR "/count.comp"),
      scan_(SHADER_DIR "/scan.comp"),
      scatter_(SHADER_DIR "/scatter.comp"),
      reduce_max_(SHADER_DIR "/reduce_max.comp"),
      count_(count) {
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

    glGenBuffers(1, &cell_counts_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cell_counts_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_CELLS * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &cell_starts_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cell_starts_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_CELLS * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &cell_cursors_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cell_cursors_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_CELLS * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &sorted_indices_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorted_indices_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(GLuint), nullptr,
                 GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    set_static_uniforms();
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

    const GLuint groups = (count_ + 255) / 256;

    // per-frame uniforms; they persist, so both pipeline runs see them
    force_.use();
    force_.set_float("u_target_density", params.target_density);
    force_.set_float("u_pressure_multiplier", params.pressure_multiplier);
    force_.set_float("u_near_pressure_multiplier", params.near_pressure_multiplier);
    force_.set_float("u_viscosity", params.viscosity);
    force_.set_float("u_tension_threshold", params.tension_threshold);
    force_.set_vec2("u_interaction_point", interaction_point_);
    force_.set_float("u_interaction_strength", interaction_strength_);
    force_.set_float("u_tension_strength", params.tension_strength);
    force_.set_float("u_cohesion_strength", params.cohesion_strength);

    midpoint_.use();
    midpoint_.set_float("u_dt", delta_time);
    midpoint_.set_float("u_gravity", params.gravity);

    step_.use();
    step_.set_float("u_dt", delta_time);
    step_.set_float("u_gravity", params.gravity);

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
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

// Grid rebuild + density + force at the given state; accelerations land in
// acceleration_ssbo_. All shaders read state via slots 0/1, so evaluating at
// the midpoint is just a matter of which buffers get bound here.
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
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);  // densities land before force reads

    force_.use();
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);  // accels land before the next pass reads
}

// A NaN anywhere in the field wins the bitwise max; report "very fast" so
// the CFL clamp drives dt to its floor instead of computing with NaN. The
// check reads the exponent bits because -ffast-math breaks std::isfinite.
static float_t decode_max_bits(GLuint bits) {
    if ((bits & 0x7F800000u) == 0x7F800000u) {
        return 1e6f;
    }

    float_t value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// Reduce speeds and acceleration magnitudes to their maxima and read them
// back. Called once per rendered frame (not per substep): one 8-byte
// transfer, and the stall it causes is mostly hidden behind vsync. The
// values are one frame stale, which is fine - dt reacts a frame late, and
// the CFL safety factors absorb that.
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

void Simulation::spawn_particles(bool is_random) {
    if (is_random) {
        std::random_device rd;
        uint32_t seed = rd();
        std::cout << "Seed: " << seed << "\n";
        create_particles(seed);
    } else {
        create_particles();
    }
}

void Simulation::create_particles() {
    std::vector<glm::vec2> positions;
    positions.reserve(count_);

    // Centered square block, same layout as the CPU sim: ceil(sqrt(N)) per
    // side, spacing derived from the domain so the block always fits. The
    // buffers are fixed at count_, so stop there instead of filling the grid.
    const int32_t per_row =
        static_cast<int32_t>(std::ceil(std::sqrt(static_cast<float_t>(count_))));
    const float_t spacing = (DOMAIN_MAX - DOMAIN_MIN) * 0.6f / per_row;
    const float_t offset = (per_row - 1) * spacing * 0.5f;

    for (int32_t row = 0; row < per_row && positions.size() < count_; ++row) {
        for (int32_t col = 0; col < per_row && positions.size() < count_; ++col) {
            positions.emplace_back(col * spacing - offset, row * spacing - offset);
        }
    }

    upload_state(positions);
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

    for (Shader* s : {&count_shader_, &scatter_, &density_, &force_}) {
        s->use();  // uniforms go to the *currently bound* program
        s->set_uint("u_num_particles", count_);
        s->set_vec2("u_grid_origin", origin);
        s->set_float("u_cell_size", CELL_SIZE);
        s->set_int("u_grid_w", GRID_W);
        s->set_int("u_grid_h", GRID_H);
    }

    scan_.use();
    scan_.set_int("u_num_cells", NUM_CELLS);

    midpoint_.use();
    midpoint_.set_uint("u_num_particles", count_);

    reduce_max_.use();
    reduce_max_.set_uint("u_num_particles", count_);

    density_.use();
    density_.set_float("u_kernel_radius", KERNEL_RADIUS);
    density_.set_float("u_kernel_radius_sq", KERNEL_RADIUS_SQ);
    density_.set_float("u_mass", MASS);

    force_.use();
    force_.set_float("u_kernel_radius", KERNEL_RADIUS);
    force_.set_float("u_kernel_radius_sq", KERNEL_RADIUS_SQ);
    force_.set_float("u_mass", MASS);
    force_.set_float("u_interaction_radius", INTERACTION_RADIUS);

    step_.use();
    step_.set_uint("u_num_particles", count_);
    step_.set_float("u_domain_half_x", DOMAIN_HALF_X);
    step_.set_float("u_domain_min", DOMAIN_MIN);
    step_.set_float("u_domain_max", DOMAIN_MAX);
    step_.set_float("u_bounce_damping", BOUNCE_DAMPING);
    step_.set_float("u_eps", EPS);
    step_.set_float("u_max_speed", MAX_SPEED);
}
