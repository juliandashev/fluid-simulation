#include <glm/geometric.hpp>

#include "simulation.hpp"

Simulation::Simulation(std::vector<Particle>& particles) : particles_(particles) {}

void Simulation::step() {
    compute_density_pressure();
    compute_forces();
    integrate();
}

void Simulation::compute_density_pressure() {
    for (Particle& pi : particles_) {
        pi.density = 0.0f;

        for (Particle& pj : particles_) {
            glm::vec2 r = pj.position - pi.position;
            float r2 = glm::dot(r, r);
            pi.density += MASS * W_poly6(r2, KERNEL_RADIUS);
        }

        pi.pressure = K_STIFF * (pi.density - RHO0);
    }
}

void Simulation::compute_forces() {
    for (Particle& pi : particles_) {
        glm::vec2 f_pressure(0.0f);
        glm::vec2 f_viscosity(0.0f);

        for (Particle& pj : particles_) {
            if (&pi == &pj) {
                continue;
            }

            glm::vec2 r = pj.position - pi.position;
            float rlen = glm::length(r);

            // Pressure force is proportional to the pressure gradient, which is the sum of the
            // gradients of the spiky kernel
            f_pressure += -MASS * (pi.pressure + pj.pressure) / (2.0f * pj.density) *
                          grad_W_spiky(r, rlen, KERNEL_RADIUS);

            // Viscosity force is proportional to the velocity Laplacian, which is the sum of the
            // laplacians of the viscosity kernel
            f_viscosity += VISCOSITY * MASS * (pj.velocity - pi.velocity) / pj.density *
                           laplacian_W_viscosity(rlen, KERNEL_RADIUS);
        }

        glm::vec2 f_gravity = pi.density * glm::vec2(0.0f, GRAVITY);
        pi.force = f_pressure + f_viscosity + f_gravity;
    }
}

// Adds gravity and simple boundary collision response
void Simulation::integrate() {
    for (Particle& p : particles_) {
        p.velocity += TIME_STEP * p.force / p.density;
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
    }
}
