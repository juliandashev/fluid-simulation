#pragma once

#include <cstdint>
#include <cmath>

// Scene
constexpr uint32_t PARTICLE_GRID = 20;  // 1 = single test particle; raise to 20 for the fluid
constexpr float_t PARTICLE_SPACING = 0.05f;
constexpr uint32_t NUM_PARTICLES = 100;

// Rendering
constexpr float_t POINT_SIZE = 16.0f;
constexpr float_t DENSITY_COLOR_SCALE = 0.025f;   // maps (density - target) onto color intensity
constexpr float_t ARROW_THICKNESS = 3.0f;         // gradient arrow line width (pixels)
constexpr uint32_t MAX_RENDERED_PARTICLES = 512;  // MUST match MAX_PARTICLES in density.frag

// SPH physics
constexpr float_t KERNEL_RADIUS = 0.2f;  // h
constexpr float_t MASS = 1.0f;
constexpr float_t TARGET_DENSITY = 73.59f;     // rho
constexpr float_t PRESSURE_MULTIPLIER = 0.5f;  // k
constexpr float_t VISCOSITY = 6.0f;
constexpr float_t GRAVITY = -9.81f;
constexpr float_t TIME_STEP = 0.02f;

// Domain bounds (matches NDC screen space)
constexpr float_t DOMAIN_MIN = -1.0f;
constexpr float_t DOMAIN_MAX = 1.0f;
constexpr float_t BOUNCE_DAMPING = 0.5f;
