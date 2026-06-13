#pragma once

#include <cstdint>
#include <cmath>

// Scene
constexpr uint32_t PARTICLE_GRID = 20;  // 1 = single test particle; raise to 20 for the fluid
constexpr float_t PARTICLE_SPACING = 0.025f;

// Rendering
constexpr float_t POINT_SIZE = 16.0f;

// SPH physics
constexpr float_t KERNEL_RADIUS = 0.04f;  // h
constexpr float_t MASS = 0.02f;
constexpr float_t RHO0 = 30.0f;  // rest density
constexpr float_t K_STIFF = 8.0f;
constexpr float_t VISCOSITY = 6.0f;
constexpr float_t GRAVITY = -9.81f;
constexpr float_t TIME_STEP = 0.002f;

// Domain bounds (matches NDC screen space)
constexpr float_t DOMAIN_MIN = -1.0f;
constexpr float_t DOMAIN_MAX = 1.0f;
constexpr float_t BOUNCE_DAMPING = 0.5f;
