#pragma once

#include <cmath>
#include <cstdint>

namespace fluid {

// Resolution. Every length and mass below derives from this, so refining the
// whole simulation is one integer; n_x = 44 is the historical setting.
constexpr int32_t PARTICLES_ACROSS = 44;  // n_x, particles across the column
constexpr float_t COLUMN_WIDTH = 46.38f;  // a, held fixed under refinement
constexpr float_t DAM_ASPECT = 2.5f;      // n = height/width of the initial column

constexpr float_t TARGET_DENSITY = 0.9f;                                        // rho_0
constexpr float_t PARTICLE_SPACING = COLUMN_WIDTH / PARTICLES_ACROSS;           // dx
constexpr float_t MASS = TARGET_DENSITY * PARTICLE_SPACING * PARTICLE_SPACING;  // m = rho_0*dx^2

// h/dx, held fixed under refinement so the neighbour count stays constant.
constexpr float_t KERNEL_RATIO = 3.79f;                             // ~45 neighbours in 2D
constexpr float_t KERNEL_RADIUS = KERNEL_RATIO * PARTICLE_SPACING;  // h
constexpr float_t KERNEL_RADIUS_SQ = KERNEL_RADIUS * KERNEL_RADIUS;

// Scene
constexpr int32_t COLUMN_ROWS = static_cast<int32_t>(PARTICLES_ACROSS * DAM_ASPECT);
constexpr uint32_t NUM_PARTICLES = static_cast<uint32_t>(PARTICLES_ACROSS * COLUMN_ROWS);
constexpr uint32_t MAX_HISTORY = 300;  // ~21 MB ceiling at 72 KB/snapshot

// Rendering
constexpr float_t PARTICLE_DRAW_SIZE = 1.0f;
constexpr float_t SPEED_COLOR_MAX = 15.0f;        // speed that maps to full red
constexpr float_t PRESSURE_COLOR_MAX = 1'000.0f;  // pressure that maps to full red
constexpr float_t INTERACTION_STRENGTH = 100.0f;  // mouse force
constexpr float_t INTERACTION_RADIUS = 30.0f;

// SPH physics
constexpr float_t PRESSURE_MULTIPLIER = 1'800.0f;  // k (gas constant)
constexpr float_t VISCOSITY = 3.14f;
constexpr float_t GRAVITY = -9.81f;
constexpr float_t SURFACE_TENSION = 0.0f;            // color-field detection threshold
constexpr float_t SURFACE_TENSION_STRENGTH = 20.0f;  // 0 = off
constexpr float_t COHESION_STRENGTH = 6.0f;          // Akinci pairwise cohesion

// Mirror plane offset outside each face.
constexpr float_t WALL_OFFSET = 0.25f * PARTICLE_SPACING;

// Tangential velocity an image keeps: 1 = free-slip, 0 = no-slip.
constexpr float_t WALL_SLIP_FLOOR = 0.0f;  // sliding along the floor/ceiling
constexpr float_t WALL_SLIP_SIDE = 1.0f;   // falling along a side wall
constexpr float_t SOLID_SLIP = 0.0f;       // same, for boundary particles

// Time stepping
constexpr float_t DT = 0.015f;               // adaptive ceiling
constexpr float_t DT_MIN = 0.001f;           // adaptive floor; NaN spikes land here
constexpr float_t CFL_LAMBDA = 0.4f;         // max kernel-radius fraction crossed per step
constexpr float_t CFL_LAMBDA_FORCE = 0.25f;  // safety factor of the acceleration condition
constexpr float_t CFL_LAMBDA_SOUND = 0.25f;  // same, for the pressure wave
constexpr float_t MAX_SPEED = 200.0f;        // hard velocity cap; firewall against NaN
constexpr float_t TIME_SCALE = 2.0f;
constexpr int32_t MAX_SUBSTEPS = 10;  // physics steps per rendered frame

// Domain bounds
constexpr float_t DOMAIN_MAX = 60.0f;
constexpr float_t DOMAIN_MIN = -DOMAIN_MAX;
constexpr float_t BOUNCE_DAMPING = -0.1f;
constexpr float_t WINDOW_ASPECT = 1280.0f / 720.0f;
constexpr float_t DOMAIN_HALF_X = DOMAIN_MAX * WINDOW_ASPECT;  // ~107, matches the window
constexpr float_t EPS = 1.0f;                                  // boundary epsilon

// Dam break benchmark (Martin & Moyce 1952, planar; Koshizuka & Oka 1996): a
// column of width a and height n*a released against the left wall at t=0.
//   ./fluid_simulation --experiment dam-break

// Spatial grid. CELL_SIZE = h is what guarantees a 3x3 walk covers every neighbour.
constexpr float_t CELL_SIZE = KERNEL_RADIUS;
constexpr int32_t GRID_W = static_cast<int32_t>(2.0f * DOMAIN_HALF_X / CELL_SIZE) + 1;       // ~54
constexpr int32_t GRID_H = static_cast<int32_t>((DOMAIN_MAX - DOMAIN_MIN) / CELL_SIZE) + 1;  // ~31
constexpr int32_t NUM_CELLS = GRID_W * GRID_H;  // ~1674

// Wrap width for periodic-x scenes. The grid must tile it exactly, or a
// neighbour carried across the seam lands at the wrong offset.
constexpr float_t PERIOD_X = GRID_W * CELL_SIZE;

// Runtime-tunable copy of the force parameters, defaulting to the values above.
struct SimParams {
    float_t gravity = GRAVITY;
    float_t pressure_multiplier = PRESSURE_MULTIPLIER;
    float_t target_density = TARGET_DENSITY;
    float_t viscosity = VISCOSITY;
    float_t dt = DT;
    float_t time_scale = TIME_SCALE;
    float_t tension_threshold = SURFACE_TENSION;
    float_t tension_strength = SURFACE_TENSION_STRENGTH;
    float_t cohesion_strength = COHESION_STRENGTH;
    float_t body_accel_x = 0.0f;  // drives the periodic channel; zero elsewhere
    float_t draw_scale = 1.0f;    // dot size as a fraction of the lattice pitch
};

}  // namespace fluid
