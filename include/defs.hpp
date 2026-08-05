#pragma once

#include <cmath>
#include <cstdint>

// Scene
constexpr uint32_t NUM_PARTICLES = 5'000;
constexpr uint32_t MAX_HISTORY = 300;  // ~21 MB ceiling at 72 KB/snapshot

// Rendering
constexpr float_t PARTICLE_DRAW_SIZE = 1.0f;
constexpr float_t SPEED_COLOR_MAX = 15.0f;        // speed that maps to full red
constexpr float_t INTERACTION_STRENGTH = 100.0f;  // external force strength applied by mouse
constexpr float_t INTERACTION_RADIUS = 30.0f;

// SPH physics
constexpr float_t KERNEL_RADIUS = 4.0f;  // h
constexpr float_t KERNEL_RADIUS_SQ = KERNEL_RADIUS * KERNEL_RADIUS;
constexpr float_t MASS = 1.0f;
constexpr float_t TARGET_DENSITY = 0.9f;              // rho_0 (rest density)
constexpr float_t PRESSURE_MULTIPLIER = 250.0f;       // k (gas constant)
constexpr float_t NEAR_PRESSURE_MULTIPLIER = 150.0f;  // k_near
constexpr float_t VISCOSITY = 3.14f;
constexpr float_t GRAVITY = -9.81f;
constexpr float_t SURFACE_TENSION = 0.0f;            // detection threshold
constexpr float_t SURFACE_TENSION_STRENGTH = 20.0f;  // 0 = off
constexpr float_t COHESION_STRENGTH = 50.0f;         // Akinci pairwise cohesion
constexpr float_t DT = 0.015f;                       // integration time step (adaptive ceiling)
constexpr float_t DT_MIN = 0.001f;                   // adaptive floor; NaN spikes land here
constexpr float_t CFL_LAMBDA = 0.4f;                 // max kernel-radius fraction crossed per step
constexpr float_t CFL_LAMBDA_FORCE = 0.25f;          // safety factor of the acceleration condition
constexpr float_t MAX_SPEED = 200.0f;                // hard velocity cap; firewall against NaN
constexpr float_t TIME_SCALE = 2.0f;
constexpr int32_t MAX_SUBSTEPS = 5;  // cap of how many physics steps run per rendered frame

// Runtime-tunable copy of the force parameters, defaulting to the constexpr
// values above.
struct SimParams {
    float_t gravity = GRAVITY;
    float_t pressure_multiplier = PRESSURE_MULTIPLIER;
    float_t target_density = TARGET_DENSITY;
    float_t near_pressure_multiplier = NEAR_PRESSURE_MULTIPLIER;
    float_t viscosity = VISCOSITY;
    float_t dt = DT;
    float_t time_scale = TIME_SCALE;
    float_t tension_threshold = SURFACE_TENSION;
    float_t tension_strength = SURFACE_TENSION_STRENGTH;
    float_t cohesion_strength = COHESION_STRENGTH;
};

// Domain bounds
constexpr float_t DOMAIN_MAX = 60.0f;
constexpr float_t DOMAIN_MIN = -DOMAIN_MAX;
constexpr float_t BOUNCE_DAMPING = -0.9f;
constexpr float_t WINDOW_ASPECT = 1280.0f / 720.0f;
constexpr float_t DOMAIN_HALF_X = DOMAIN_MAX * WINDOW_ASPECT;  // ≈ 71, matches the window
constexpr float_t EPS = 1.0f;                                  // boundary epsilon

// Spatial constats
constexpr float_t CELL_SIZE = KERNEL_RADIUS;  // h - guarantees 3×3 covers all neighbors

// spatial grid dimensions
constexpr int32_t GRID_W = static_cast<int32_t>(2.0f * DOMAIN_HALF_X / CELL_SIZE) + 1;       // ~36
constexpr int32_t GRID_H = static_cast<int32_t>((DOMAIN_MAX - DOMAIN_MIN) / CELL_SIZE) + 1;  // ~26
constexpr int32_t NUM_CELLS = GRID_W * GRID_H;                                               // ~950
