#pragma once

#include <glm/vec2.hpp>
#include <string>
#include <vector>

#include "shader.hpp"
#include "defs.hpp"
#include "stats.hpp"

namespace fluid {

// Owns the GPU-resident particle state (position/velocity SSBOs) and the
// compute shader that advances it. Requires a current GL context
class Simulation {
public:
    explicit Simulation(uint32_t count);
    ~Simulation();

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    void step(float_t delta_time, const SimParams& params);
    void spawn_particles(bool is_random, float_t rest_density = TARGET_DENSITY);

    // Dam break initial condition: a column of aspect ratio (height/width)
    // resting against the left wall on the floor, at the rest-density pitch.
    void spawn_dam_break(float_t rest_density = TARGET_DENSITY, float_t aspect = DAM_ASPECT);

    // Static solid geometry. Contributes to density and pressure but never
    // moves, so the grid over it is built here once instead of every step.
    void spawn_solids(const std::vector<glm::vec2>& positions);

    // Explicit initial positions; the caller sizes them to count().
    void spawn_at(const std::vector<glm::vec2>& positions);

    // period > 0 replaces the side walls with a wrap of that width.
    void set_periodic_x(float_t period);

    // Geometry of the last spawned dam column - the reference length the
    // benchmark nondimensionalises against.
    float_t column_width() const { return column_width_; }
    float_t column_height() const { return column_height_; }
    float_t column_origin_x() const { return column_origin_x_; }

    // Host copy of the current positions. Stalls the pipeline, so call it at
    // measurement cadence (every Nth step), never per frame.
    [[nodiscard]] std::vector<glm::vec2> read_positions() const;
    [[nodiscard]] std::vector<float_t> read_speeds() const;
    [[nodiscard]] std::vector<float_t> read_densities() const;

    // Readback, no recompute; measurement cadence only. Holds the RK2 midpoint
    // stage, so it is half a step stale relative to the logged positions.
    [[nodiscard]] log::DensityStats density_stats() const;

    // Takes gravity because the buffer excludes it and these are statistics of
    // the residual |a + g| - see the definition.
    [[nodiscard]] log::AccelStats accel_stats(float_t gravity) const;

    // One row per particle; thresholds are left to analysis.
    void dump_particles(const std::string& path, float_t gravity) const;

    // Max speed (x) and acceleration magnitude (y) of the last stepped frame.
    [[nodiscard]] glm::vec2 max_kinematics();
    void debug_density_stats(float_t rest_density = TARGET_DENSITY);

    void set_interaction(glm::vec2 point, float_t strength) {
        interaction_point_ = point;
        interaction_strength_ = strength;
    }

    GLuint position_buffer() const { return position_ssbo_; }
    GLuint speed_buffer() const { return speed_ssbo_; }

    // vec2 per particle, value in .x; holds the RK2 midpoint stage.
    GLuint density_buffer() const { return density_ssbo_; }
    GLuint velocity_buffer() const { return velocity_ssbo_; }
    GLuint solid_position_buffer() const { return solid_position_ssbo_; }
    uint32_t count() const { return count_; }
    uint32_t solid_count() const { return solid_count_; }

private:
    void create_particles(float_t rest_density);  // centered square block
    void create_particles(uint32_t seed);         // uniform random scatter
    void create_column(float_t rest_density, float_t aspect);  // dam break reservoir
    void upload_state(const std::vector<glm::vec2>& positions);
    void set_static_uniforms();
    void build_solid_grid();
    void compute_accelerations(GLuint position, GLuint velocity);

    // Shaders
    gl::Shader step_;
    gl::Shader midpoint_;
    gl::Shader density_;
    gl::Shader force_;
    gl::Shader count_shader_;
    gl::Shader scan_;
    gl::Shader scatter_;
    gl::Shader reduce_max_;

    // Shader storage buffer objects (ssbo)
    GLuint position_ssbo_ = 0;
    GLuint velocity_ssbo_ = 0;
    GLuint density_ssbo_ = 0;
    GLuint acceleration_ssbo_ = 0;
    GLuint speed_ssbo_ = 0;
    GLuint mid_position_ssbo_ = 0;
    GLuint mid_velocity_ssbo_ = 0;

    GLuint max_kinematics_ssbo_ = 0;  // two uints: float maxima as bit patterns

    GLuint solid_position_ssbo_ = 0;
    GLuint solid_cell_counts_ = 0;
    GLuint solid_cell_starts_ = 0;
    GLuint solid_sorted_indices_ = 0;

    GLuint cell_counts_ = 0;
    GLuint cell_starts_ = 0;
    GLuint cell_cursors_ = 0;
    GLuint sorted_indices_ = 0;

    uint32_t count_ = 0;
    uint32_t solid_count_ = 0;
    float_t period_x_ = 0.0f;

    float_t column_width_ = 0.0f;     // a, set by create_column
    float_t column_height_ = 0.0f;    // n*a as realised, after the row clamp
    float_t column_origin_x_ = 0.0f;  // left face of the column at t=0

    glm::vec2 interaction_point_{0.0f};
    float_t interaction_strength_ = 0.0f;
};

}  // namespace fluid
