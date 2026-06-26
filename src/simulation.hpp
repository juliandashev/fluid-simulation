#pragma once

#include <math.h>
#include <glm/vec2.hpp>
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
    float_t scale = M_PI * (h_sq * h_sq * h_sq);
    float_t diff = radius - distance;
    return -45.0f * diff * diff / scale;
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

class Simulation {
public:
    explicit Simulation(std::vector<Particle>& particles);

    void set_interaction(glm::vec2 point, float_t strength) {
        interaction_point_ = point;
        interaction_strength_ = strength;
    }

    void create_particles();
    void create_particles(uint32_t seed);
    void simulation_step(float_t delta_time);

private:
    glm::vec2 calculate_pressure_force(const Particle& particle);
    glm::vec2 get_random_dir();
    glm::vec2 interaction_force(const Particle& particle, glm::vec2 input_position, float_t radius,
                                float_t strength);

    float_t calculate_density_at(const glm::vec2& sample_point);
    float_t density_to_pressure(const float_t density);
    float_t calculate_shared_pressure(const float_t density_a, const float_t density_b);


    SpacialGrid grid_;
    glm::vec2 interaction_point_{0.0f};
    float_t interaction_strength_ = 0.0f;

    // Reference to the particle vector
    std::vector<Particle>& particles_;
    std::vector<glm::vec2> predicted_positions_;
};
