#include "simulation.hpp"

#include <glm/geometric.hpp>
#include <iostream>
#include <random>
#include <execution>
#include <cmath>

Simulation::Simulation(std::vector<Particle>& particles) : particles_(particles) {}

void Simulation::simulation_step(float_t delta_time) {
    std::for_each(std::execution::par, particles_.begin(), particles_.end(),
                  [this, delta_time](Particle& p) {
                      size_t i = &p - particles_.data();
                      p.velocity += glm::vec2(0.0f, GRAVITY) * delta_time;
                      predicted_positions_[i] = p.position + p.velocity * delta_time;
                  });

    // Update spacial lookup with predicted positions
    grid_.update_spacial_lookup(predicted_positions_, KERNEL_RADIUS);

    // Calculate densities and pressures
    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        size_t i = &p - particles_.data();
        densities_[i] = calculate_density_at(predicted_positions_[i]);
        pressures_[i] = density_to_pressure(densities_[i]);
    });

    // Apply densities and pressure forces
    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        p.force = calculate_pressure_force(p);

        if (interaction_strength_ != 0.0f) {
            p.force +=
                interaction_force(p, interaction_point_, INTERACTION_RADIUS, interaction_strength_);
        }
    });

    // Update positions and resolve collisions
    std::for_each(std::execution::par, particles_.begin(), particles_.end(),
                  [this, delta_time](Particle& p) {
                      size_t i = &p - particles_.data();
                      // Forward Euler integration
                      p.velocity += delta_time * p.force / densities_[i];
                      p.position += delta_time * p.velocity;

                      // Enforce boundary conditions
                      if (p.position.x < -DOMAIN_HALF_X + EPS) {
                          p.position.x = -DOMAIN_HALF_X + EPS;
                          p.velocity.x *= BOUNCE_DAMPING;
                      } else if (p.position.x > DOMAIN_HALF_X - EPS) {
                          p.position.x = DOMAIN_HALF_X - EPS;
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

void Simulation::spawn_particles(bool random) {
    if (random) {
        std::random_device rd;
        uint32_t seed = rd();
        std::cout << "Seed: " << seed << "\n";
        create_particles(seed);
    } else {
        create_particles();
    }
}

glm::vec2 Simulation::get_random_dir() {
    // thread_local: each TBB worker gets its own RNG so parallel calls don't race
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float_t> angle(0.0f, 2.0f * static_cast<float_t>(M_PI));

    // random direction angle
    float_t phi = angle(rng);
    return glm::vec2(std::cos(phi), std::sin(phi));
}

float_t Simulation::density_to_pressure(const float_t density) {
    // how far off target (signed)
    float_t density_error = density - TARGET_DENSITY;
    return density_error * PRESSURE_MULTIPLIER;
}

float_t Simulation::calculate_density_at(const glm::vec2& location) {
    float_t density = 0.0f;

    grid_.foreach_point_within_radius(location, [&](int32_t j) {
        glm::vec2 offset = predicted_positions_[j] - location;
        float_t dst_sq = glm::dot(offset, offset);
        density += MASS * smoothing_kernel(KERNEL_RADIUS_SQ, dst_sq);
    });

    return density;
}

glm::vec2 Simulation::calculate_pressure_force(const Particle& particle) {
    size_t i = &particle - particles_.data();
    glm::vec2 pressure_force(0, 0);
    grid_.foreach_point_within_radius(predicted_positions_[i], [&](int32_t j) {
        if (i == static_cast<size_t>(j)) {
            return;
        }

        // predicted geometry
        glm::vec2 offset = predicted_positions_[j] - predicted_positions_[i];
        float_t distance = std::sqrt(glm::dot(offset, offset));
        glm::vec2 norm_dir = distance < 1e-6f ? get_random_dir() : offset / distance;

        float_t slope = spiky_kernel_derivative(KERNEL_RADIUS, distance);

        // scalars stay real
        float_t shared_pressure = calculate_shared_pressure(pressures_[j], pressures_[i]);
        pressure_force += norm_dir * shared_pressure * slope * MASS / densities_[j];
    });

    return pressure_force;
}

/// @brief Calculates the shared pressure between two densities;
/// @param pressure_a Density of first particle
/// @param pressure_b Density of second particle
/// @return Average between the two density values
float_t Simulation::calculate_shared_pressure(const float_t pressure_a, const float_t pressure_b) {
    return (pressure_a + pressure_b) / 2.0f;
}

void Simulation::create_particles() {
    particles_.clear();
    particles_.reserve(NUM_PARTICLES);

    // Lay the particles out as a centered square block. ceil(sqrt(N)) per side is
    // the squarest grid that holds N. Spacing is derived from the domain so the
    // block always fits and packs tighter as N grows, so no hand-tuning per count.
    const int32_t per_row =
        static_cast<int32_t>(std::ceil(std::sqrt(static_cast<float_t>(NUM_PARTICLES))));
    const float_t spacing = (DOMAIN_MAX - DOMAIN_MIN) * 0.6f / per_row;
    const float_t offset = (per_row - 1) * spacing * 0.5f;  // half block width -> centers on origin

    for (int32_t row = 0; row < per_row; ++row) {
        for (int32_t col = 0; col < per_row; ++col) {
            Particle p{};
            p.position = glm::vec2(col * spacing - offset, row * spacing - offset);
            particles_.push_back(p);
        }
    }

    predicted_positions_.resize(particles_.size());
    densities_.resize(particles_.size());
    pressures_.resize(particles_.size());
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
        particles_.push_back(p);
    }

    predicted_positions_.resize(particles_.size());
    densities_.resize(particles_.size());
    pressures_.resize(particles_.size());
}

glm::vec2 Simulation::interaction_force(const Particle& particle, glm::vec2 input_position,
                                        float_t radius, float_t strength) {
    glm::vec2 force(0.0f, 0.0f);
    glm::vec2 offset = input_position - particle.position;
    float_t dst_sq = glm::dot(offset, offset);

    const float_t r_sq = radius * radius;

    if (dst_sq < r_sq) {
        float_t dst = std::sqrt(dst_sq);
        glm::vec2 dir = dst <= 1e-6f ? glm::vec2(0.0f) : offset / dst;
        float_t falloff = 1.0f - dst / radius;  // 1 at center; 0 at edge
        force = (dir * strength - particle.velocity) * falloff;
    }
    return force;
}
