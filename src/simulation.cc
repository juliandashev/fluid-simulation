#include "simulation.hpp"

#include <glm/geometric.hpp>
#include <iostream>
#include <random>
#include <execution>

Simulation::Simulation(std::vector<Particle>& particles) : particles_(particles) {}

void Simulation::simulation_step() {
    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        p.density = calculate_density(p.position);
        p.velocity += glm::vec2(0.0f, GRAVITY) * TIME_STEP;
    });

    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        glm::vec2 pressure_force = calculate_pressure_force(p.position);
        // a = F/m, so divide by density to get acceleration
        glm::vec2 pressure_acceleration = pressure_force / p.density;
        p.velocity += pressure_acceleration * TIME_STEP;
    });

    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        p.position += p.velocity * TIME_STEP;

        if (p.position.x < DOMAIN_MIN) {
            p.position.x = DOMAIN_MIN;
            p.velocity.x *= -BOUNCE_DAMPING;
        } else if (p.position.x > DOMAIN_MAX) {
            p.position.x = DOMAIN_MAX;
            p.velocity.x *= -BOUNCE_DAMPING;
        }

        if (p.position.y < DOMAIN_MIN) {
            p.position.y = DOMAIN_MIN;
            p.velocity.y *= -BOUNCE_DAMPING;
        } else if (p.position.y > DOMAIN_MAX) {
            p.position.y = DOMAIN_MAX;
            p.velocity.y *= -BOUNCE_DAMPING;
        }
    });

    static bool printed = false;
    if (!printed) {
        printed = true;
        std::cout << "density at center: " << calculate_density(glm::vec2(0.0f, 0.0f)) << "\n";
        std::cout << "density far away:  " << calculate_density(glm::vec2(5.0f, 5.0f)) << "\n";
    }
}

glm::vec2 Simulation::calculate_pressure_force(glm::vec2 sample_point) {
    glm::vec2 pressure_force = glm::vec2(0, 0);

    for (Particle& pi : particles_) {
        float_t dst = glm::distance(pi.position, sample_point);
        if (dst < 1e-6f) {
            continue;  // sample point sits on this particle: direction is undefined
        }
        glm::vec2 dir = (pi.position - sample_point) / dst;
        float_t slope = smoothing_kernel_derivative(KERNEL_RADIUS, dst);
        float_t density = pi.density;
        pressure_force += -density_to_pressure(density) * dir * slope * MASS / density;
    }

    return pressure_force;
}

float_t Simulation::density_to_pressure(float_t density) {
    // how far off target (signed)
    float_t density_error = density - TARGET_DENSITY;
    float_t pressure = density_error * PRESSURE_MULTIPLIER;

    // k * (rho - rho_0) can make pressure negative when density is below target, which is
    // physically correct: it means the fluid is under tension and will pull together rather than
    // push apart. However, this can cause numerical instability, so we clamp it to zero.
    return pressure;
}

float_t Simulation::calculate_density(glm::vec2 sample_point) {
    float_t density = 0.0f;

    for (Particle& pi : particles_) {
        float_t dst = glm::distance(pi.position, sample_point);
        float_t influence = smoothing_kernel(KERNEL_RADIUS, dst);
        density += MASS * influence;
    }

    return density;
}

void Simulation::create_particles(uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

    const float bound_size = DOMAIN_MAX - DOMAIN_MIN;  // domain is 2 units wide

    particles_.clear();
    particles_.reserve(NUM_PARTICLES);

    for (uint32_t i = 0; i < NUM_PARTICLES; ++i) {
        Particle p{};

        p.position = glm::vec2(dist(rng) * bound_size, dist(rng) * bound_size);
        p.property = std::cos(p.position.y - 3.0f + std::sin(p.position.x));
        particles_.push_back(p);
    }
}

void Simulation::apply_pressure_force() {
    for (Particle& pi : particles_) {
        glm::vec2 pressure_force = calculate_pressure_force(pi.position);
        // a = F/m, so divide by density to get acceleration
        glm::vec2 pressure_acceleration = pressure_force / pi.density;
        pi.velocity += pressure_acceleration * TIME_STEP;
    }
}
