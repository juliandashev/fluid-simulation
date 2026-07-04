#pragma once

#include <cmath>
#include <cstdint>

// Scene
constexpr uint32_t NUM_PARTICLES = 3'000;
constexpr uint32_t MAX_HISTORY = 300;  // ~21 MB ceiling at 72 KB/snapshot

// Rendering
constexpr float_t POINT_SIZE = 8.0f;
constexpr float_t SPEED_COLOR_MAX = 15.0f;       // speed that maps to full red
constexpr float_t DENSITY_COLOR_SCALE = 20.0f;   // maps (density - target) onto color intensity
constexpr float_t ARROW_THICKNESS = 3.0f;        // gradient arrow line width (pixels)
constexpr float_t INTERACTION_STRENGTH = 45.0f;  // external force strength applied by mouse
constexpr float_t INTERACTION_RADIUS = 20.0f;

// SPH physics
constexpr float_t KERNEL_RADIUS = 4.0f;  // h
constexpr float_t KERNEL_RADIUS_SQ = KERNEL_RADIUS * KERNEL_RADIUS;
constexpr float_t MASS = 1.0f;
constexpr float_t TARGET_DENSITY = 1.1f;              // rho_0 (rest density)
constexpr float_t PRESSURE_MULTIPLIER = 750.0f;       // k (gas constant)
constexpr float_t NEAR_PRESSURE_MULTIPLIER = 300.0f;  // k_near
constexpr float_t VISCOSITY = 2.7f;
constexpr float_t GRAVITY = -9.81f;
constexpr float_t DT = 0.015f;  // integration time step
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
};

// Domain bounds
constexpr float_t DOMAIN_MIN = -40.0f;
constexpr float_t DOMAIN_MAX = 40.0f;
constexpr float_t BOUNCE_DAMPING = -0.2f;
constexpr float_t WINDOW_ASPECT = 1280.0f / 720.0f;
constexpr float_t DOMAIN_HALF_X = DOMAIN_MAX * WINDOW_ASPECT;  // ≈ 71, matches the window
constexpr float_t EPS = 1.0f;                                  // boundary epsilon

// Spatial constats
constexpr float_t CELL_SIZE = KERNEL_RADIUS;  // h - guarantees 3×3 covers all neighbors
constexpr int32_t GRID_WIDTH = static_cast<int32_t>((DOMAIN_MAX - DOMAIN_MIN) / CELL_SIZE) + 1;
constexpr int32_t FIELD_RES = 128;  // density-field texture resolution
