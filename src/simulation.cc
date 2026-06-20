#include "simulation.hpp"

#include <glm/geometric.hpp>
#include <iostream>
#include <random>
#include <execution>

Simulation::Simulation(std::vector<Particle>& particles) : particles_(particles) {}

void Simulation::simulation_step() {
    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        p.density = calculate_density(p.position);
        p.velocity += glm::vec2(0.0f, GRAVITY) * DT;
    });

    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        glm::vec2 pressure_force = calculate_pressure_force(p);
        // a = F/m, so divide by density to get acceleration
        glm::vec2 pressure_acceleration = pressure_force / p.density;
        p.velocity += pressure_acceleration * DT;
    });

    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        // Forward Euler integration
        p.velocity += DT * p.force / p.density;
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

    static bool printed = true;
    if (printed) {
        std::cout << particles_[0].density << "\n";
        printed = false;
    }
}

glm::vec2 Simulation::get_random_dir() {
    // thread_local: each TBB worker gets its own RNG so parallel calls don't race
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angle(0.0f, 2.0f * static_cast<float>(M_PI));

    // random direction angle
    float_t phi = angle(rng);
    return glm::vec2(std::cos(phi), std::sin(phi));
}

float_t Simulation::density_to_pressure(float_t density) {
    // how far off target (signed)
    float_t density_error = density - TARGET_DENSITY;
    float_t pressure = density_error * PRESSURE_MULTIPLIER;

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

glm::vec2 Simulation::calculate_pressure_force(Particle& particle) {
    glm::vec2 pressure_force = glm::vec2(0, 0);

    for (Particle& pi : particles_) {
        if (particle.position == pi.position) {
            continue;
        }

        glm::vec2 offset = pi.position - particle.position;
        float_t dst = glm::length(offset);
        glm::vec2 dir = dst < 1e-6f ? get_random_dir() : (offset) / dst;

        float_t slope = spiky_kernel_derivative(KERNEL_RADIUS, dst);
        pressure_force += density_to_pressure(pi.density) * dir * slope * MASS / pi.density;
    }

    return pressure_force;
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
        p.property = std::cos(p.position.y - 3.0f + std::sin(p.position.x));

        particles_.push_back(p);
    }
}
