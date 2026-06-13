#pragma once

#include <math.h>
#include <glm/vec2.hpp>
#include <vector>

#include "defs.hpp"
#include "particle.hpp"

/// @brief Poly6 kernel for density estimation. Polynomial of degree 6
/// @param r2 Squared distance between particles
/// @param h Kernel radius
/// @return Kernel value
inline float_t W_poly6(float_t r2, float_t h) {
    if (r2 >= h * h) {
        return 0.0f;
    }

    // Integrating the kernel over a circle of radius h gives 1
    // Integrating [C*(h*h - r2)^3 dA] = 1 over the disc of radius h
    // gives the result: C = 4/(PI * h^8)
    return 4.0f / (M_PI * pow(h, 8)) * pow(h * h - r2, 3);
}

/// @brief Gradient of the spiky kernel. Produces a curve with a sharp spike
/// @param r Vector between particles
/// @param rlen Length of r
/// @param h Kernel radius
/// @return Gradient value
inline glm::vec2 grad_W_spiky(glm::vec2 r, float_t rlen, float_t h) {
    // 1e-6 avoids division by zero when two particles are on top of each other
    if (rlen >= h || rlen < 1e-6f) {
        return glm::vec2(0.0f);
    }

    // (r / rlen) is the unit direction
    // (h - rlen)^2 is the magnitude

    // From 10 / (PI - h^5) * (h -r)^3
    // -- 10 / (PI - h^5) is normalization constant
    // d [ (h - r)^3 ] /dr = 3 * (h - r)^2 * (-1)
    // Multiply by the normalization gives the final result
    float coeff = -30.0f / (M_PI * pow(h, 5)) * (h - rlen) * (h - rlen);
    return coeff * (r / rlen);
}

/// @brief Laplacian of the viscosity kernel
/// @param r Distance between particles
/// @param h Kernel radius
/// @return Laplacian value
inline float_t laplacian_W_viscosity(float_t r, float_t h) {
    if (r >= h) {
        return 0.0f;
    }

    // since (h-r) is positive for every r < h
    // this guarantees viscosity always removes energy from velocity differences
    return 40.0f / (M_PI * pow(h, 5)) * (h - r);
}

class Simulation {
public:
    explicit Simulation(std::vector<Particle>& particles);
    void step();

private:
    void compute_density_pressure();
    void compute_forces();
    void integrate();

    // Reference to the particle vector
    std::vector<Particle>& particles_;
};
