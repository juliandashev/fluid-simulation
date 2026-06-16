#pragma once

#include <math.h>
#include <glm/vec2.hpp>
#include <vector>

#include "defs.hpp"
#include "particle.hpp"

inline float_t smoothing_kernel_derivative(float_t radius, float_t distance) {
    if (distance >= radius) {
        return 0.0f;
    }

    float_t f = radius * radius - distance * distance;
    float_t scale = -24.0f / (M_PI * std::pow(radius, 8));
    return scale * distance * f * f;
}

/// @brief Poly6 kernel for density estimation. Polynomial of degree 6
/// @param r2 Squared distance between particles
/// @param h Kernel radius
/// @return Kernel value
inline float_t smoothing_kernel(float_t radius, float_t distance) {
    float_t volume = M_PI * std::pow(radius, 8) / 4.0f;  // volume of the kernel support
    float_t value = std::max(0.0f, radius * radius - distance * distance);
    return (value * value * value) / volume;
}

class Simulation {
public:
    explicit Simulation(std::vector<Particle>& particles);
    void create_particles(uint32_t seed);
    void simulation_step();

    glm::vec2 calculate_pressure_force(glm::vec2 sample_point);

private:
    void compute_densities();
    void apply_pressure_force();
    void apply_gravity();
    void update_positions();

    float_t calculate_density(glm::vec2 sample_point);
    float_t density_to_pressure(float_t density);

    // Reference to the particle vector
    std::vector<Particle>& particles_;
};
