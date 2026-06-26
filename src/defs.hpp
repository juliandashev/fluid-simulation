#pragma once

#include <cstdint>
#include <cmath>

// Scene
constexpr uint32_t PARTICLE_GRID = 20;  // 1 = single test particle; raise to 20 for the fluid
constexpr float_t PARTICLE_SPACING = 8.0f;
constexpr uint32_t NUM_PARTICLES = 2000;
constexpr uint32_t MAX_HISTORY = 300;  // ~21 MB ceiling at 72 KB/snapshot

// Rendering
constexpr float_t POINT_SIZE = 10.0f;
constexpr float_t SPEED_COLOR_MAX = 1.5f;       // speed that maps to full red
constexpr float_t DENSITY_COLOR_SCALE = 20.0f;  // maps (density - target) onto color intensity
constexpr float_t ARROW_THICKNESS = 3.0f;       // gradient arrow line width (pixels)
constexpr float_t INTERACTION_RADIUS = 10.0f;
constexpr float_t INTERACTION_STRENGTH = 2.0f;    // tune live; left = pull, right = push
constexpr uint32_t MAX_RENDERED_PARTICLES = 512;  // MUST match MAX_PARTICLES in density.frag

// SPH physics
constexpr float_t KERNEL_RADIUS = 4.0f;  // h
constexpr float_t KERNEL_RADIUS_SQ = KERNEL_RADIUS * KERNEL_RADIUS;
constexpr float_t MASS = 1.0f;
constexpr float_t TARGET_DENSITY = 0.12f;      // rho
constexpr float_t PRESSURE_MULTIPLIER = 4.0f;  // k
constexpr float_t VISCOSITY = 6.0f;
constexpr float_t GRAVITY = 0.0f;
constexpr float_t DT = 0.02f;  // integration time step
constexpr float_t EPS = 4.0f;  // boundary epsilon

// Domain bounds
constexpr float_t DOMAIN_MIN = -40.0f;
constexpr float_t DOMAIN_MAX = 40.0f;
constexpr float_t BOUNCE_DAMPING = -0.5f;
constexpr float_t WINDOW_ASPECT = 1280.0f / 720.0f;
constexpr float_t DOMAIN_HALF_X = DOMAIN_MAX * WINDOW_ASPECT;  // ≈ 71, matches the window

// Spatial constats
constexpr float_t CELL_SIZE = KERNEL_RADIUS;  // h - guarantees 3×3 covers all neighbors
constexpr int32_t GRID_WIDTH = static_cast<int32_t>((DOMAIN_MAX - DOMAIN_MIN) / CELL_SIZE) + 1;
constexpr int32_t FIELD_RES = 128;  // density-field texture resolution
