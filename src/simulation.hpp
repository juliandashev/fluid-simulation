#pragma once

#include <math.h>
#include <glm/vec2.hpp>
#include <vector>

#include "defs.hpp"
#include "particle.hpp"

/// @brief Derivative of the W_Spiky kernel. Used for the pressure force.
/// @note Gradient stays non-zero as d->0, so close particles still repel instead of clustering.
/// @param radius Support radius h
/// @param distance Distance d between two particles
/// @return d/dd of (h - d)^3 normalized; negative for d < h, 0 when d >= h
inline float_t spiky_kernel_derivative(float_t radius, float_t distance) {
    if (distance >= radius) {
        return 0.0f;
    }

    float_t scale = M_PI * std::pow(radius, 6);
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

    void create_particles(uint32_t seed);
    void simulation_step();

private:
    glm::vec2 calculate_pressure_force(Particle& particle);
    glm::vec2 calculate_property_gradient(glm::vec2 sample_point);
    glm::vec2 get_random_dir();

    float_t calculate_density_at(glm::vec2 sample_point);
    float_t density_to_pressure(float_t density);
    float_t calculate_property(glm::vec2 sample_particle);
    float_t calculate_shared_pressure(float_t density_a, float_t density_b);

    // Reference to the particle vector
    std::vector<Particle>& particles_;
};
