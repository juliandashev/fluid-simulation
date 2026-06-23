#include "simulation.hpp"

#include <glm/geometric.hpp>
#include <iostream>
#include <random>
#include <execution>

Simulation::Simulation(std::vector<Particle>& particles) : particles_(particles) {}

void Simulation::simulation_step() {
    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        p.velocity += glm::vec2(0, -1) * GRAVITY;
        p.density = calculate_density_at(p.position);
    });

    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        glm::vec2 pressure_force = calculate_pressure_force(p);
        // a = F/m, so divide by density to get acceleration
        glm::vec2 pressure_acceleration = pressure_force / p.density;
        p.velocity += pressure_acceleration;
    });

    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        // Forward Euler integration
        // p.velocity += DT * p.force / p.density;
        p.position += DT * p.velocity;

        // Enforce boundary conditions
        if (p.position.x < DOMAIN_MIN + EPS) {
            p.position.x = DOMAIN_MIN + EPS;
            p.velocity.x *= BOUNCE_DAMPING;
        } else if (p.position.x > DOMAIN_MAX - EPS) {
            p.position.x = DOMAIN_MAX - EPS;
            p.velocity.x *= BOUNCE_DAMPING;
        }

        if (p.position.y < DOMAIN_MIN) {
            p.position.y = DOMAIN_MIN;
            p.velocity.y *= BOUNCE_DAMPING;
        } else if (p.position.y > DOMAIN_MAX) {
            p.position.y = DOMAIN_MAX;
            p.velocity.y *= BOUNCE_DAMPING;
        }
    });
}

glm::vec2 Simulation::get_random_dir() {
    // thread_local: each TBB worker gets its own RNG so parallel calls don't race
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float_t> angle(0.0f, 2.0f * static_cast<float_t>(M_PI));

    // random direction angle
    float_t phi = angle(rng);
    return glm::vec2(std::cos(phi), std::sin(phi));
}

float_t Simulation::density_to_pressure(float_t density) {
    // how far off target (signed)
    float_t density_error = density - TARGET_DENSITY;
    return std::max(0.0f, density_error * PRESSURE_MULTIPLIER);
}

float_t Simulation::calculate_density_at(glm::vec2 location) {
    float_t density = 0.0f;

    for (Particle& pi : particles_) {
        glm::vec2 offset = pi.position - location;
        float_t dst2 = glm::dot(offset, offset);

        if (dst2 < KERNEL_RADIUS_SQ) {
            // this computation is symmetric
            density += MASS * smoothing_kernel(KERNEL_RADIUS_SQ, dst2);
        }
    }

    return density;
}

glm::vec2 Simulation::calculate_pressure_force(Particle& particle) {
    glm::vec2 pressure_force(0, 0);

    for (Particle& pi : particles_) {
        if (&particle == &pi) {
            continue;
        }

        glm::vec2 offset = pi.position - particle.position;
        float_t dst = glm::length(offset);
        glm::vec2 dir = dst < 1e-6f ? get_random_dir() : offset / dst;

        float_t slope = spiky_kernel_derivative(KERNEL_RADIUS, dst);
        float_t density = pi.density;
        float_t shared_pressure = calculate_shared_pressure(density, particle.density);
        pressure_force += shared_pressure * dir * slope * MASS / density;
    }

    return pressure_force;
}

/// @brief Calculates the shared pressure between two densities;
/// @param density_a Density of first particle
/// @param density_b Density of second particle
/// @return Average between the two density values
float_t Simulation::calculate_shared_pressure(float_t density_a, float_t density_b) {
    float_t pressure_a = density_to_pressure(density_a);
    float_t pressure_b = density_to_pressure(density_b);

    return (pressure_a + pressure_b) / 2;
}

void Simulation::create_particles(uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float_t> dist(-0.5f, 0.5f);

    const float_t bound_size = DOMAIN_MAX - DOMAIN_MIN;

    particles_.clear();
    particles_.reserve(NUM_PARTICLES);

    for (uint32_t i = 0; i < NUM_PARTICLES; ++i) {
        Particle p{};

        p.position = glm::vec2(dist(rng) * bound_size, dist(rng) * bound_size);

        // Particle property corresponds to a value of this example function
        // Done for debugging purposes
        // p.property = std::cos(0.1f * p.position.y - 3.0f + std::sin(0.1f * p.position.x));
        particles_.push_back(p);
    }
}

float_t Simulation::calculate_property(glm::vec2 sample_particle) {
    float_t property = 0.0f;

    for (Particle& p : particles_) {
        glm::vec2 offset = p.position - sample_particle;

        float_t distance_sq = glm::dot(offset, offset);

        float_t influence = smoothing_kernel(KERNEL_RADIUS_SQ, distance_sq);
        float_t density = calculate_density_at(p.position);

        property += p.property * influence * MASS / density;
    }

    return property;
}

glm::vec2 Simulation::calculate_property_gradient(glm::vec2 sample_point) {
    glm::vec2 property_gradient(0, 0);

    for (Particle& p : particles_) {
        float_t distance = glm::distance(p.position, sample_point);
        glm::vec2 direction = (p.position - sample_point) / distance;
        float_t slope = spiky_kernel_derivative(KERNEL_RADIUS, distance);

        property_gradient += p.property * direction * slope * MASS / p.density;
    }

    return property_gradient;
}
