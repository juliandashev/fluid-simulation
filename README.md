# Fluid Simulation

A real-time 2D fluid simulation written from scratch in C++17 and OpenGL, based on
**Smoothed Particle Hydrodynamics (SPH)** following Müller et al. 2003,
*"Particle-Based Fluid Simulation for Interactive Applications."*

This is a learning project: particles are simulated on the CPU and rendered as
point sprites, with the physics built up incrementally — kernels, density and
pressure, pressure/viscosity forces, integration, and boundary handling.

## Status

Work in progress. The rendering pipeline and SPH core are in place; physics
constants are being tuned for stability. A position-based collision pass provides
a stable fallback while the full SPH pressure solve is dialed in.

## Tech stack

- **C++17**
- **OpenGL 4.3** (core profile)
- **GLFW 3.4** — windowing and input (fetched automatically)
- **GLM 1.0.1** — vector math (fetched automatically)
- **GLAD** — OpenGL loader (pre-generated, bundled in `external/glad/`)
- **CMake** ≥ 3.20 with `FetchContent`

## Requirements

- A C++17 compiler (GCC or Clang)
- CMake ≥ 3.20
- OpenGL development libraries
- **X11** — GLFW is configured for X11 (Wayland is disabled in `CMakeLists.txt`)

GLFW and GLM are downloaded and built automatically by CMake on first configure,
so no manual dependency installation is needed beyond OpenGL/X11 system headers.

## Building

The `run.sh` script provides build helpers. **Source it** to load the functions,
then call a build target:

```bash
source run.sh    # loads: debug, release, incremental-build (prints usage)

debug            # configure + build in Debug mode (defines DEBUG_MODE)
release          # configure + build in Release mode
incremental-build   # rebuild using the last configured mode
```

Each of `debug` / `release` removes any existing `build/` directory, recreates it,
configures with CMake, and builds in parallel.

### Manual build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Running

The compiled binary is placed in `bin/`:

```bash
./bin/fluid_simulation
```

Press **Esc** to quit.

## Project structure

```
src/
  main.cc          — window/context setup, particle spawn, render loop
  defs.hpp         — all tunable constants (physics, scene, rendering)
  particle.hpp     — Particle struct (position, velocity, force, density, pressure)
  simulation.hpp   — Simulation class + inline SPH kernels (poly6, spiky grad, viscosity)
  simulation.cc    — density/pressure, forces, integration, collision/boundary passes
  renderer.hpp/.cc — owns VAO/VBO/shader, uploads positions and draws each frame
  shader.hpp/.cc   — RAII OpenGL shader-program wrapper
shaders/
  particle.vert    — point-sprite vertex shader
  particle.frag    — circular point fragment shader
external/glad/     — bundled OpenGL loader
CMakeLists.txt     — top-level build (fetches GLFW/GLM, builds GLAD)
run.sh             — build helper functions
```

## Physics overview

Each simulation step runs three passes over the particles:

1. **Density & pressure** — for every particle, sum kernel-weighted contributions
   from neighbours (poly6 kernel) to estimate local density, then derive pressure
   from an equation of state `p = k(ρ − ρ₀)`.
2. **Forces** — accumulate pressure (spiky-gradient kernel), viscosity
   (viscosity-Laplacian kernel), and gravity for each particle.
3. **Integration** — semi-implicit (symplectic) Euler advances velocity then
   position, with reflective box boundaries.

Tunable parameters (kernel radius, mass, rest density, stiffness, viscosity,
gravity, timestep, domain bounds) all live in `src/defs.hpp`.

## License

MIT — see [LICENSE](LICENSE).
