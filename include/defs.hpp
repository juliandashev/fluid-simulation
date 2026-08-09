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
constexpr float_t CFL_LAMBDA_SOUND = 0.4f;           // same, for the pressure wave
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

// Numerical speed of sound implied by the equation of state.
//
// force.comp prices pressure as p = k((rho/rho_0)^4 - 1), so the stiffness the
// solver actually feels is c^2 = dp/drho at rest density = 4k/rho_0. This is
// not a physical sound speed - it is a free parameter chosen to make the fluid
// stiff enough that density stays near rho_0 - and the weakly-compressible
// assumption is that the flow stays well below it. Ten to one is the usual
// rule; at Mach 1 the "incompressible" fluid is a gas.
//
// The exponent 4 is duplicated from force.comp:72. Changing it there without
// changing it here leaves the acoustic timestep condition silently wrong.
inline float_t sound_speed(const SimParams& params) {
    return std::sqrt(4.0f * params.pressure_multiplier / params.target_density);
}

// The flow speed this fluid can carry before it goes transonic and the pressure
// field stops being able to communicate upstream. Reported at startup so the
// scene is checked against the solver rather than assumed to fit it.
inline float_t mach_number(float_t flow_speed, const SimParams& params) {
    const float_t c = sound_speed(params);
    return c > 0.0f ? flow_speed / c : 0.0f;
}

// Domain bounds
constexpr float_t DOMAIN_MAX = 60.0f;
constexpr float_t DOMAIN_MIN = -DOMAIN_MAX;
constexpr float_t BOUNCE_DAMPING = -0.1f;
constexpr float_t WINDOW_ASPECT = 1280.0f / 720.0f;
constexpr float_t DOMAIN_HALF_X = DOMAIN_MAX * WINDOW_ASPECT;  // ≈ 107, matches the window
constexpr float_t EPS = 1.0f;                                  // boundary epsilon

// Dam break benchmark (Martin & Moyce 1952, planar case; Koshizuka & Oka 1996).
// A column of width a and height n*a is released against the left wall at t=0;
// the surge front position is logged and compared to the published curve.
constexpr bool DAM_BREAK_MODE = true;  // spawn the column instead of the centered block
constexpr float_t DAM_ASPECT = 2.5f;   // n = height/width of the initial column

// Spatial constats
constexpr float_t CELL_SIZE = KERNEL_RADIUS;  // h - guarantees 3×3 covers all neighbors

// spatial grid dimensions
constexpr int32_t GRID_W = static_cast<int32_t>(2.0f * DOMAIN_HALF_X / CELL_SIZE) + 1;       // ~54
constexpr int32_t GRID_H = static_cast<int32_t>((DOMAIN_MAX - DOMAIN_MIN) / CELL_SIZE) + 1;  // ~31
constexpr int32_t NUM_CELLS = GRID_W * GRID_H;                                               // ~1674
