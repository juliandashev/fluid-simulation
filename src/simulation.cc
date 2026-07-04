#include "simulation.hpp"

#include <cmath>
#include <execution>
#include <glm/geometric.hpp>
#include <iostream>
#include <random>

Simulation::Simulation(std::vector<Particle>& particles) : particles_(particles) {}

void Simulation::simulation_step(float_t delta_time) {
    // Snapshot the current state into the flat arrays the integrator works on.
    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        size_t i = &p - particles_.data();
        cur_pos_[i] = p.position;
        cur_vel_[i] = p.velocity;
    });

    accelerations(cur_pos_, cur_vel_, accel1_);

    std::for_each(std::execution::par, particles_.begin(), particles_.end(),
                  [this, delta_time](Particle& p) {
                      size_t i = &p - particles_.data();
                      mid_pos_[i] = cur_pos_[i] + 0.5f * delta_time * cur_vel_[i];
                      mid_vel_[i] = cur_vel_[i] + 0.5f * delta_time * accel1_[i];
                  });

    accelerations(mid_pos_, mid_vel_, accel2_);

    std::for_each(std::execution::par, particles_.begin(), particles_.end(),
                  [this, delta_time](Particle& p) {
                      size_t i = &p - particles_.data();
                      cur_pos_[i] = cur_pos_[i] + delta_time * mid_vel_[i];
                      cur_vel_[i] = cur_vel_[i] + delta_time * accel2_[i];
                  });

    // Write the integrated state back onto the particles and resolve boundaries.
    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        size_t i = &p - particles_.data();
        p.position = cur_pos_[i];
        p.velocity = cur_vel_[i];

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

// Fills accel_out[i] with the acceleration of particle i evaluated at the given
// positions/velocities: gravity + (pressure + viscosity + interaction) / density.
// Rebuilds the neighbor grid and density field on the passed-in state, so it can
// be evaluated at any RK stage (start, midpoint, ...).
void Simulation::accelerations(const std::vector<glm::vec2>& pos, const std::vector<glm::vec2>& vel,
                               std::vector<glm::vec2>& accel_out) {
    predicted_positions_ = pos;  // grid + density + pressure-force sample these
    sample_velocities_ = vel;    // the viscosity term samples these

    grid_.update_spacial_lookup(predicted_positions_, KERNEL_RADIUS);

    std::for_each(std::execution::par, particles_.begin(), particles_.end(), [this](Particle& p) {
        size_t i = &p - particles_.data();
        auto [density, near_density] = calculate_density_at(predicted_positions_[i]);
        auto [pressure, near_pressure] = density_to_pressure(density, near_density);
        fields_[i] = {density, near_density, pressure, near_pressure};
    });

    std::for_each(std::execution::par, particles_.begin(), particles_.end(),
                  [this, &accel_out](Particle& p) {
                      size_t i = &p - particles_.data();
                      glm::vec2 force = calculate_forces(i);
                      if (interaction_strength_ != 0.0f) {
                          force += interaction_force(predicted_positions_[i], sample_velocities_[i],
                                                     interaction_point_, INTERACTION_RADIUS,
                                                     interaction_strength_);
                      }
                      accel_out[i] = glm::vec2(0.0f, params_.gravity) + force / fields_[i].density;
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

glm::vec2 Simulation::get_random_dir() const {
    // thread_local: each TBB worker gets its own RNG so parallel calls don't race
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float_t> angle(0.0f, 2.0f * static_cast<float_t>(M_PI));

    // random direction angle
    float_t phi = angle(rng);
    return glm::vec2(std::cos(phi), std::sin(phi));
}

PressurePair Simulation::density_to_pressure(const float_t density,
                                             const float_t near_density) const {
    // how far off target (signed)
    float_t pressure = (density - params_.target_density) * params_.pressure_multiplier;
    float_t near_pressure = near_density * params_.near_pressure_multiplier;

    return {pressure, near_pressure};
}

DensityPair Simulation::calculate_density_at(const glm::vec2& location) const {
    float_t density = 0.0f;
    float_t near_density = 0.0f;

    grid_.foreach_point_within_radius(location, [&](int32_t, glm::vec2, float_t dst_sq) {
        density += MASS * smoothing_kernel(KERNEL_RADIUS_SQ, dst_sq);
        near_density += MASS * near_smoothing_kernel(KERNEL_RADIUS, std::sqrt(dst_sq));
    });

    return {density, near_density};
}

glm::vec2 Simulation::calculate_forces(size_t particle_index) const {
    glm::vec2 pressure_force(0, 0);
    glm::vec2 viscosity_force(0, 0);

    grid_.foreach_point_within_radius(
        predicted_positions_[particle_index], [&](int32_t j, glm::vec2 offset, float_t dst_sq) {
            if (particle_index == static_cast<size_t>(j)) {
                return;
            }

            // predicted geometry
            float_t distance = std::sqrt(dst_sq);
            glm::vec2 norm_dir = distance < 1e-6f ? get_random_dir() : offset / distance;

            // pressure (regular and near)
            float_t slope = spiky_kernel_derivative(KERNEL_RADIUS, distance);
            float_t near_slope = near_spiky_kernel_derivative(KERNEL_RADIUS, distance);

            // scalars stay real
            float_t shared_pressure =
                calculate_shared_pressure(fields_[j].pressure, fields_[particle_index].pressure);
            float_t near_shared_pressure = calculate_shared_pressure(
                fields_[j].near_pressure, fields_[particle_index].near_pressure);

            float_t inv_density_j = 1.0f / fields_[j].density;

            pressure_force +=
                norm_dir * MASS * (shared_pressure * slope + near_shared_pressure * near_slope);

            float_t influence = viscosity_kernel_laplacian(KERNEL_RADIUS, distance);
            viscosity_force += (sample_velocities_[j] - sample_velocities_[particle_index]) *
                               influence * MASS * inv_density_j;
        });

    return pressure_force + (viscosity_force * params_.viscosity);
}

/// @brief Calculates the shared pressure between two densities;
/// @param pressure_a Pressure of first particle
/// @param pressure_b Pressure of second particle
/// @return Average between the two pressure values
float_t Simulation::calculate_shared_pressure(const float_t pressure_a,
                                              const float_t pressure_b) const {
    return (pressure_a + pressure_b) * 0.5f;
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
    fields_.resize(particles_.size());
    sample_velocities_.resize(particles_.size());
    cur_pos_.resize(particles_.size());
    cur_vel_.resize(particles_.size());
    mid_pos_.resize(particles_.size());
    mid_vel_.resize(particles_.size());
    accel1_.resize(particles_.size());
    accel2_.resize(particles_.size());
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
    fields_.resize(particles_.size());
    sample_velocities_.resize(particles_.size());
    cur_pos_.resize(particles_.size());
    cur_vel_.resize(particles_.size());
    mid_pos_.resize(particles_.size());
    mid_vel_.resize(particles_.size());
    accel1_.resize(particles_.size());
    accel2_.resize(particles_.size());
}

glm::vec2 Simulation::interaction_force(glm::vec2 position, glm::vec2 velocity,
                                        glm::vec2 input_position, float_t radius,
                                        float_t strength) const {
    glm::vec2 force(0.0f, 0.0f);
    glm::vec2 offset = input_position - position;
    float_t dst_sq = glm::dot(offset, offset);

    const float_t r_sq = radius * radius;

    if (dst_sq < r_sq) {
        float_t dst = std::sqrt(dst_sq);
        glm::vec2 dir = dst <= 1e-6f ? glm::vec2(0.0f) : offset / dst;
        float_t falloff = 1.0f - (dst / radius);  // 1 at center; 0 at edge

        force = (dir * strength - velocity) * falloff;
    }
    return force;
}
