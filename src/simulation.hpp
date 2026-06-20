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

    float_t scale = M_PI * std::pow(radius, 5);
    float_t diff = radius - distance;
    return -30.0f * diff * diff / scale;
}

/// @brief W_Poly6 kernel value. Used for density estimation.
/// @param radius    Support radius h (influence cutoff)
/// @param distance  Distance d between two particles
/// @return (h²-d²)³ normalized over the 2D disk; 0 when d >= h
inline float_t smoothing_kernel(float_t radius, float_t distance) {
    if (distance >= radius) {
        return 0.0f;
    }

    float_t volume = (M_PI * std::pow(radius, 8)) / 4.0f;
    float_t diff = (radius * radius) - (distance * distance);
    return (diff * diff * diff) / volume;
}

class Simulation {
public:
    explicit Simulation(std::vector<Particle>& particles);

    void create_particles(uint32_t seed);
    void simulation_step();

    glm::vec2 calculate_pressure_force(Particle& particle);

private:
    glm::vec2 get_random_dir();
    float_t calculate_density(glm::vec2 sample_point);
    float_t density_to_pressure(float_t density);

    // Reference to the particle vector
    std::vector<Particle>& particles_;
};
