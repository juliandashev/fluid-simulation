#pragma once

#include <math.h>

#include <glm/vec2.hpp>
#include <utility>
#include <vector>

#include "defs.hpp"
#include "particle.hpp"
#include "spacial_grid.hpp"

/// @brief Derivative of the W_Spiky kernel. Used for the pressure force.
/// @note Gradient stays non-zero as d->0, so close particles still repel instead of clustering.
/// @param radius Support radius h
/// @param distance Distance d between two particles
/// @return d/dd of (h - d)^3 normalized; negative for d < h, 0 when d >= h
inline float_t spiky_kernel_derivative(float_t radius, float_t distance) {
    if (distance >= radius) {
        return 0.0f;
    }

    float_t h_sq = radius * radius;
    float_t scale = M_PI * (h_sq * h_sq * radius);
    float_t diff = radius - distance;

    return -30.0f * diff * diff / scale;
}

/// @brief W_Poly6 kernel value. Used for density estimation.
/// @param radius_sq Squared support radius h^2
/// @param distance_sq Squared distance d^2 between two particles
/// @return (h^2 - d^2)^3 normalized over the 2D disk; 0 when d >= h
inline float_t smoothing_kernel(float_t radius_sq, float_t distance_sq) {
    if (distance_sq >= radius_sq) {
        return 0.0f;
    }

    float_t volume = M_PI * (radius_sq * radius_sq * radius_sq * radius_sq) / 4.0f;
    float_t diff = radius_sq - distance_sq;
    return (diff * diff * diff) / volume;
}

/// @brief Laplacian of the W_viscosity kernel. Used for the viscosity force.
/// @note Peaks as d approaches 0 and falls linearly to 0 at d = h, so the closest
///       neighbors exchange the most momentum. Always non-negative.
/// @param radius Support radius h
/// @param distance Distance d between two particles
/// @return (h - d) normalized; 0 when d >= h
inline float_t viscosity_kernel_laplacian(float_t radius, float_t distance) {
    if (distance >= radius) {
        return 0.0f;
    }

    float_t h_sq = radius * radius;
    float_t scale = M_PI * (h_sq * h_sq * radius);

    // Divergence of the gradient
    return 40.0f * (radius - distance) / scale;
}

/// @brief W_near kernel value. Used for near-density estimation
/// @note Higher power than Poly6 ((h-d)^4 vs (h^2-d^2)^3) meaning it falls off faster and weights
///       very close neighbors more heavily. Near-density feeds an always-positive
///       near-pressure, giving short-range repulsion that stops particles clumping.
/// @param radius Support radius h
/// @param distance Distance d between two particles
/// @return (h - d)^4 normalized; 0 when d >= h
inline float_t near_smoothing_kernel(float_t radius, float_t distance) {
    if (distance >= radius) {
        return 0.0f;
    }

    float_t h_sq = radius * radius;
    float_t scale = M_PI * (h_sq * h_sq * h_sq);
    float_t diff = radius - distance;

    return 15.0f * (diff * diff) * (diff * diff) / scale;
}

/// @brief Derivative of the W_near kernel. Used for the near-pressure force.
/// @note Steeper than spiky_kernel_derivative, so repulsion ramps up sharply as d->0.
///       This enforces the minimum inter-particle spacing that prevents clustering.
/// @param radius Support radius h
/// @param distance Distance d between two particles
/// @return d/dd of (h - d)^4 normalized; negative for d < h, 0 when d >= h
inline float_t near_spiky_kernel_derivative(float_t radius, float_t distance) {
    if (distance >= radius) {
        return 0.0f;
    }

    float_t h_sq = radius * radius;
    float_t scale = M_PI * (h_sq * h_sq * h_sq);
    float_t diff = radius - distance;

    return -60.0f * diff * diff * diff / scale;
}

struct DensityPair {
    float_t density;
    float_t near_density;
};

struct PressurePair {
    float_t pressure;
    float_t near_pressure;
};

struct ParticleField {
    float_t density;
    float_t near_density;
    float_t pressure;
    float_t near_pressure;
};

class Simulation {
public:
    explicit Simulation(std::vector<Particle>& particles);

    void set_interaction(glm::vec2 point, float_t strength) {
        interaction_point_ = point;
        interaction_strength_ = strength;
    }
    void simulation_step(float_t delta_time);
    void spawn_particles(bool is_random);

    SimParams& params() { return params_; }  // live tuning handle

private:
    void create_particles();
    void create_particles(uint32_t seed);

    float_t calculate_shared_pressure(const float_t pressure_a, const float_t pressure_b) const;

    glm::vec2 calculate_forces(size_t particle_index) const;
    glm::vec2 get_random_dir() const;
    glm::vec2 interaction_force(glm::vec2 position, glm::vec2 velocity, glm::vec2 input_position,
                                float_t radius, float_t strength) const;

    // Evaluate a(pos, vel) for every particle into accel_out. Rebuilds the grid
    // and density field on the passed state so any RK stage can be evaluated.
    void accelerations(const std::vector<glm::vec2>& pos, const std::vector<glm::vec2>& vel,
                       std::vector<glm::vec2>& accel_out);

    DensityPair calculate_density_at(const glm::vec2& sample_point) const;
    PressurePair density_to_pressure(float_t density, float_t near_density) const;

    SpacialGrid grid_;
    glm::vec2 interaction_point_{0.0f};
    float_t interaction_strength_ = 0.0f;

    // Reference to the particle vector
    std::vector<Particle>& particles_;
    std::vector<glm::vec2> predicted_positions_;
    std::vector<ParticleField> fields_;

    // Integrator working buffers (RK2): current state, midpoint state, and the
    // two acceleration evaluations. sample_velocities_ feeds the viscosity term
    // the velocities of whichever RK stage is currently being evaluated.
    std::vector<glm::vec2> sample_velocities_;
    std::vector<glm::vec2> cur_pos_;
    std::vector<glm::vec2> cur_vel_;
    std::vector<glm::vec2> mid_pos_;
    std::vector<glm::vec2> mid_vel_;
    std::vector<glm::vec2> accel1_;
    std::vector<glm::vec2> accel2_;

    SimParams params_;
};
