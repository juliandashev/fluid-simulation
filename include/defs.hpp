#pragma once

#include <cmath>
#include <cstdint>

namespace fluid {

// Resolution defaults. n_x = 44 with a = 46.38 is the historical setting the
// dam-break numbers were measured at; a scene may pick its own - see Resolution.
constexpr int32_t PARTICLES_ACROSS = 44;  // n_x, particles across the column
constexpr float_t COLUMN_WIDTH = 46.38f;  // a, held fixed under refinement
constexpr float_t DAM_ASPECT = 2.5f;      // n = height/width of the initial column

constexpr float_t TARGET_DENSITY = 0.9f;  // rho_0

// h/dx, held fixed under refinement so the neighbour count stays constant.
constexpr float_t KERNEL_RATIO = 3.79f;  // ~45 neighbours in 2D

constexpr uint32_t MAX_HISTORY = 300;  // ~21 MB ceiling at 72 KB/snapshot

// Rendering
constexpr float_t PARTICLE_DRAW_SIZE = 1.0f;
constexpr float_t SPEED_COLOR_MAX = 15.0f;        // speed that maps to full red
constexpr float_t PRESSURE_COLOR_MIN = 0.0f;      // pressure that maps to full blue
constexpr float_t PRESSURE_COLOR_MAX = 1'000.0f;  // pressure that maps to full red
constexpr float_t INTERACTION_STRENGTH = 100.0f;  // mouse force
constexpr float_t INTERACTION_RADIUS = 30.0f;

// SPH physics
constexpr float_t PRESSURE_MULTIPLIER = 1'800.0f;  // k (gas constant)
constexpr float_t VISCOSITY = 3.14f;
constexpr float_t GRAVITY = -9.81f;
// Wing chord as a fraction of the domain height. Lives here rather than in
// wing.hpp so SimParams can default from it without inverting the include.
constexpr float_t WING_CHORD_FRAC = 0.75f;

constexpr float_t SURFACE_TENSION = 0.0f;            // color-field detection threshold
constexpr float_t SURFACE_TENSION_STRENGTH = 20.0f;  // 0 = off
constexpr float_t COHESION_STRENGTH = 6.0f;          // Akinci pairwise cohesion

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

// Everything that scales with the discretisation, derived from one integer and
// one length so the pieces cannot drift apart. A scene declares its own; the
// shaders already take all of these as uniforms.
struct Resolution {
    int32_t particles_across;
    float_t column_width;
    float_t aspect;

    uint32_t count;
    float_t spacing;           // dx
    float_t mass;              // m = rho_0 * dx^2
    float_t kernel_radius;     // h
    float_t kernel_radius_sq;
    float_t wall_offset;       // mirror plane offset outside each face
    float_t cell_size;         // = h, so a 3x3 walk covers every neighbour
    int32_t grid_w;
    int32_t grid_h;
    int32_t num_cells;
    float_t period_x;          // wrap width; the grid must tile it exactly
    float_t fluid_volume;      // count * dx^2, the area the fluid fills at rho_0
};

constexpr Resolution make_resolution(int32_t n_x, float_t column_width, float_t aspect) {
    const float_t dx = column_width / n_x;
    const float_t h = KERNEL_RATIO * dx;
    const int32_t rows = static_cast<int32_t>(n_x * aspect);
    const uint32_t count = static_cast<uint32_t>(n_x * rows);
    const int32_t gw = static_cast<int32_t>(2.0f * DOMAIN_HALF_X / h) + 1;
    const int32_t gh = static_cast<int32_t>((DOMAIN_MAX - DOMAIN_MIN) / h) + 1;

    return Resolution{n_x,
                      column_width,
                      aspect,
                      count,
                      dx,
                      TARGET_DENSITY * dx * dx,
                      h,
                      h * h,
                      0.25f * dx,
                      h,
                      gw,
                      gh,
                      gw * gh,
                      gw * h,
                      count * dx * dx};
}

constexpr Resolution DEFAULT_RESOLUTION =
    make_resolution(PARTICLES_ACROSS, COLUMN_WIDTH, DAM_ASPECT);

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
    float_t target_speed = 0.0f;  // 0 = constant drive; else the drive eases off here
    float_t wing_aoa_deg = 8.0f;  // rebuilt when changed; wing scene only
    float_t wing_chord_frac = WING_CHORD_FRAC;  // same; chord as a fraction of domain height
    float_t blow_speed = 40.0f;   // target speed inside the cursor in blow mode
    float_t render_smoothing = 1.6f;  // h_vis / h; a view knob, never seen by the solver
    float_t draw_scale = 1.0f;    // dot size as a fraction of the lattice pitch
    float_t pressure_min = PRESSURE_COLOR_MIN;  // narrow the band to see small
    float_t pressure_max = PRESSURE_COLOR_MAX;  // deviations around ambient
};

}  // namespace fluid
