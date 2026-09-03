# Fluid Simulation

A real-time 2D fluid simulation written from scratch in C++17 and OpenGL 4.3,
based on **Smoothed Particle Hydrodynamics (SPH)**. The solver started as a
CPU implementation following Müller et al. [1] and has since moved entirely
onto the GPU: every stage of the physics - neighbour search, density,
forces, and integration - runs as compute shaders, with particle state living
in GPU buffers that are rendered directly, zero-copy.

<p align="center">
  <img src="media/demo.gif" alt="Fluid simulation demo" width="640">
</p>

## Status

Running and interactive on integrated graphics.

Particles spawn on a lattice at rest-density spacing `√(m/ρ₀)`, so the fluid
starts in mechanical equilibrium instead of venting an initial pressure
transient; a startup diagnostic reads back the density field and reports its
distribution (interior sits at `ρ₀` to within 0.02%).

**GPU pipeline.** Each physics step dispatches a chain of compute shaders:
a **counting-sort spatial grid** rebuild (count → scan → scatter) gives O(N)
neighbour queries, then fused density and force passes walk the 3×3 cell
neighbourhood once per particle. Positions and speeds are rendered straight
from the storage buffers - particle data never returns to the CPU.

**Physics.** Pressure follows the **Tait equation of state** (the form of
Becker & Teschner [3], with exponent 4 rather than their 7) clamped to
non-negative, so compression is punished
super-linearly while free surfaces feel no spurious attraction. An
Akinci-style **pairwise cohesion** force [4] holds detached blobs together
and pinches separating fluid into droplets. Müller color-field surface
tension [1] rounds free surfaces and holds grabbed fluid in a blob. Gravity
is on - the fluid falls, pools, splashes, and settles.

**Boundaries.** A wall truncates the kernel support: a particle within `h` of
one sums a half-empty disc, reads about `ρ₀/2`, and the clamped EOS gives it
zero pressure, so the wall acts as a low-pressure attractor that pulls fluid
into it. Both the density and force passes repair this with **mirror images** -
each neighbour reflected across any wall plane within `h`, corners taking a
third diagonal image. The image's tangential velocity is what sets the slip
condition, and it is set per face: the floor is no-slip and supplies the bulk
dissipation, the side walls are free-slip so fluid pushed against them still
falls freely. `reflect_face` in `step.comp` survives only as a penetration
catcher, not as the boundary condition.

**Obstacles.** Mirroring reflects across an infinite plane, so it cannot express
a shape. Solid geometry inside the domain is instead built from **boundary
particles**: frozen particles on the fluid lattice that contribute mass to the
density sum and carry the pressure of whichever fluid particle is looking at
them, so non-penetration falls out of the same pressure force as everything
else. They live in their own buffers with their own grid, and because they never
move that grid is built once at spawn instead of once per step. The two schemes
are additive - the domain box keeps its mirrors, so scenes without obstacles are
unaffected.

**Integration and stability.** A **midpoint (RK2)** integrator evaluates the
full pipeline twice per step. The timestep is **adaptive**: a GPU reduction
finds the maximum particle speed and acceleration each frame, and two CFL
conditions [3, 6] shrink `dt` when the flow gets violent - stiff moments run in
brief slow-motion instead of exploding. A third, **acoustic** condition
`dt ≤ λ_c·h/c` bounds it by the equation of state's own wave speed
`c = √(4k/ρ₀)`; unlike the other two it does not depend on the flow, so it
applies from the first step. At the stock pressure multiplier it is slack
(0.048 against the 0.015 ceiling) and changes nothing, which is precisely why it
had to be written down before `k` is raised. A hard velocity cap in the
integration shader acts as a final firewall against NaN blow-ups, and
explicit NaN/inf guards on velocity and position rescue any particle that
slips through - a bad step costs a particle its momentum, never the
simulation its stability.

**Measurement.** Each run writes `run.csv` (step, sim time, dt, max
speed/acceleration, sampled every 4th step), which shows *which* CFL condition
is limiting `dt` at each moment rather than just the result. Over a 477 s run the
velocity condition never once bound `dt` while the acceleration condition bound
it in 5.5% of frames.

**Validation.** The solver is being checked against the planar **dam break**
benchmark [8, 9]: a column of width `a` and height `n·a` is released against the
left wall (`--experiment dam-break`; `--help` lists the scenes) and the surge
front along the floor is logged in dimensionless form, `Z = (x_front − x₀)/a`
against `T = t·√(2g/a)`, to a file named after the force parameters it was
run at
(`dam_break_coh<c>_visc<v>_ten<t>.csv`, with the stiffness in a column) so a
sweep is a single session and no two settings can land in the same file. The
front is defined as a high percentile of `x` among
particles within one kernel radius of the floor - a height gate rejects airborne
spray, the percentile rejects lone stragglers, and a genuine surge passes both.
Runs reproduce to three significant figures despite frame timing varying the
`dt` sequence between them. Raw traces are not committed; `experiments/`
keeps the reduction script and the write-up of what the sweeps found.

**Applications.** Beyond the benchmark, the solver is used to show that
textbook fluid behaviour falls out of the discretisation rather than being put in
by hand. `--experiment pipe` runs a **periodic** channel, walls and throat built
from boundary particles, driven by a body force with gravity off. Fluid leaving
the right edge re-enters on the left, which keeps the particle count fixed - the
neighbour search wraps across the seam and shifts those neighbours by one period,
so the grid has to tile the wrap exactly (`PERIOD_X = GRID_W * CELL_SIZE`). The
channel is deliberately smaller than the fluid's rest volume so it runs
pressurised rather than settling against the EOS's zero clamp. Nothing in the
solver knows Bernoulli's equation; it integrates mass and momentum, and
`p + rho*v^2/2 + rho*g*y` is a first integral of that. It reaches a true steady
state, and the two logged bands answer different questions - continuity is a
statement about a cross-section, Bernoulli about a streamline.

| | measured | expected |
|---|---|---|
| speed ratio, full section | 2.04 (sd 0.042) | 2.00, the area ratio |
| pressure at the throat | -8% | drop |
| `p + rho*v^2/2` deviation | 3.1% median | 0 |

Measured over 196 profiles at Mach 0.093. The same run at Mach 0.24 gives a much
larger pressure drop (29%) but twice the Bernoulli error (6.6%), which is the
compressibility limit showing up where the theory says it should. Velocity
recovers fully downstream of the throat while pressure does not - that asymmetry
is the viscous loss, and in ideal flow both would be symmetric.

`--experiment wing` is the same machinery without a duct: a NACA 0018 section in
a gas filling the whole domain, periodic in x. Angle of attack and chord are
panel fields - moving either rebuilds the section, its boundary particles and
their grid in place, without disturbing the gas.

Circulation is out of reach: at accessible resolutions the smoothing length is
comparable to the section thickness, so there is no trailing edge sharp enough to
set the Kutta condition, and without circulation there is no lift. A sweep at
n_x = 50 put the mean pressure above and below the section within 0.3% of each
other. Growing the chord is the cheap half of that ratio - it costs no timestep
and no fluid particles, where refining `n_x` costs `n_x³` - and takes the section
from 1.07 h thick to 1.84 h, but ~10 h is what a resolvable trailing edge wants.

So the scene starts still, and the lift asymmetry is **imposed rather than
grown**. `B` turns the mouse drag into a directional jet: the same
velocity-targeting interaction the pull already used, aimed along a fixed
direction instead of at the cursor. Held over the upper surface it forces the
flow asymmetry that circulation would otherwise have to build, and the pressure
field responds - measured over a 60 s run, mean pressure above the section runs
137 to 481 below the gas underneath it, at every sample and never with the wrong
sign. That is the pressure difference lift is made of, obtained without the
resolution a Kutta condition needs, and it is a demonstration rather than a
measurement: nothing here integrates a force over the surface.

The free-stream version is still there (`body force x` and `target speed` in the
panel), and shows bluff-body behaviour once the angle is large - a stagnation
region on the windward face and a separated low-pressure wake - which needs no
circulation either.

**Three defects this scene surfaced**, all of which looked like solver problems
and were not:

- **The governor watched the wrong number.** A body force in a periodic domain
  with free-slip walls has nothing to balance it, so the drive eases off as the
  flow approaches a target. Governing on the *maximum* speed meant the handful of
  particles accelerating around the section's shoulders - exactly the ones that
  are supposed to be fast - drove the throttle to 0.005 and cut the body force to
  0.030 of 6.0 for all 4840. The bulk then coasted while the wake lost momentum
  to the surface and wrapped around as its own inflow. Governing on the mean
  gives a real equilibrium: bulk at 19.6 of a 20 target, drive steady at 0.124.
- **The gas spawned 13% over-dense**, and the startup transient drove particles
  through the section's surface - twelve of them within 0.3 s, pushed 2.4 dx deep
  and then stuck, because deep inside the solids surround a particle symmetrically
  and the pressure *gradient* cancels. `N·dx²` has to match the area the gas
  actually gets; since `dx = W/n_x` and `N ∝ n_x²` that product depends only on
  `COLUMN_WIDTH`, so 105 → 102 fixes it at every resolution. Intrusion went from
  8-14 permanent to 0-1 transient at 0.17 dx.
- **The trailing edge had no boundary particles at all.** `to_particles` lays
  solids on the lattice at pitch `dx`; where the section thins below that,
  nothing lands inside it, and the fluid pours through a wall that is not there.
  It now also walks the polygon outline, keeping a sample only where nothing sits
  within `0.8 dx` already - so the thick part keeps its lattice packing exactly
  and only the starved edges gain particles.

**Resolution.** Every length and mass derives from one integer and one length,
built into a `Resolution` at startup rather than fixed at compile time, so a
scene can pick its own and `--particles N` overrides it. The dam-break scenes
keep the historical setting and reproduce bit-identically; the wing needs a
domain-filling gas, which is a different rest volume and therefore a different
`COLUMN_WIDTH`. Holding `h/dx` fixed keeps the neighbour count constant under
refinement, so `--particles 22/44/88` changes the discretisation without changing
the physical problem.

**Interaction & tooling.** Drag with the mouse to push or pull the fluid,
pause and step/rewind through a GPU-resident history buffer, and tune the
physics live - gravity, pressure, rest density, viscosity, tension,
cohesion, timestep, and time scale - through a Dear ImGui panel. `S` and `P`
select speed or pressure colouring; the pressure view reads the same Tait EOS
the solver runs on, so the field driving the motion is visible rather than only
its result. Its range is a pair of panel fields, because the useful band is
scene-specific: a wing perturbs the pressure by a few percent of ambient and the
pipe's throat drop spans 400-700, either of which a full-scale ramp flattens
into one hue. A colour-bar legend shows the active range, drawn with the same
`√t` warp the shader applies so the tick labels sit at the colours they name.

The band is worth more care than it looks: it decides whether a real effect is
visible at all, and it has to bracket the range the fluid occupies *in the state
being watched*, not at spawn. The wing alone has four such states - spawned,
settled still, under a free stream, and under the jet - whose pressures sit near
700, 700, 490 and 20-500. Every wrong choice reads as "the physics is not
happening": the pipe's 40% throat drop hid inside two adjacent hues at 0-1000,
and the jet's measured 137-481 asymmetry clamped entirely to blue under a band
centred on ambient.

**Reconstructed field.** `F` swaps the particle view for an Eulerian read-out:
the SPH interpolant `Σ (m_j/ρ_j) A_j W(r - r_j, h_vis)` evaluated on a fixed
grid rather than at the particles, rendered as a smooth colour map. Averaging
~45 neighbours per sample suppresses the per-particle pressure scatter that
weakly-compressible SPH inherently carries - `p ~ ρ⁴` turns sub-percent density
noise into several percent on pressure. The render smoothing length is a panel
field independent of the solver's `h`, and pressure is interpolated rather than
recomputed from an interpolated density, since the EOS is convex and the two
differ. The Shepard denominator doubles as a coverage test, so solid geometry
and the region outside the fluid blank themselves. Nothing here feeds back into
the solver: it is a view, and the measurements stay Lagrangian.

### Working on now

Applications, on top of the validation. The solver now selects a scene from the
command line (`--experiment`, `--help` lists them), and solid geometry inside the
domain is built from boundary particles, so a scene is a set of quads plus a
table row, selectable at runtime from a menu that rebuilds the scene in place.
That gave the periodic pipe above, which is the first result that is not a
benchmark reproduction: a textbook law recovered from a solver that was never
told about it. The runtime particle count that was blocking a convergence study
is done, so that study is now a matter of running it. The wing shows the pressure
asymmetry now that the jet imposes it, but lift is still a picture rather than a
number: that needs a force accumulator over the boundary particles, which is the
next thing worth building there.

The particle layering the pressure view makes visible is a separate thread. The
field reconstruction hides it without addressing it. Of the fixes in the
literature, δ-SPH density diffusion [12] targets the acoustic noise and particle
shifting [13] targets the anisotropic packing; XSPH smooths velocity only, and
the DualSPHysics maintainers treat it as superseded by the other two.

Underneath that, quantitative validation rather than new physics. The dam break
gives a curve with a known shape to compare against, which turns "it looks like
water" into a measurable distance from published data. Four parameter studies
are done - cohesion, surface tension, stiffness, viscosity - and chasing a
residual through the first three turned it into two solver findings, one of
which invalidated a headline from the first. Every front number below was
measured with free-slip walls, before the floor became no-slip; the surge runs
along that floor, so the sweeps need re-running before they are quoted as
final:

- **Cohesion sweep**, run at the stock stiffness and again converged at
  `k = 4000`. Cohesion 50 delays the onset by 2.3×, in both, and is the only
  value that genuinely *holds* the column - its early rise goes as `T^1.70`
  against `T^1.47-1.52` for the rest, the difference between a release and a
  front simply accelerating from rest. Converged, cohesion 20 also separates out
  at 1.25×, where at stock stiffness it had been indistinguishable from zero: the
  compressible fluid's own squashing was masking it, and the saturation point is
  between 5 and 20 rather than at 20. Past its knee the cohesion-50 curve runs
  parallel to the others rather than staying proportionally behind, so what
  cohesion buys is a delay and not a sustained drag - though that reading is off
  the figure, since the front never reaches a steady speed to compare, climbing
  monotonically from 5-14% of the inviscid bound at onset to 69-74% when it runs
  out of tank, with no plateau in any run.
- **Surface-tension control.** Cohesion and the Müller color-field tension are
  separate additive terms in `force.comp`, and the sweep varied only the first -
  `tension_strength` sat at 20 with its threshold at 0 throughout, so "cohesion
  0" was never "no surface tension". Turning it fully off moves the onset from
  0.229 to 0.239, less than one sample interval: the term does essentially
  nothing here, and the residual delay is not surface tension.
- **Stiffness sweep.** At `k = 250` the equation of state's wave speed is
  `c = 33.3` while the front is measured at 48.5 and Ritter predicts 67.8 - the
  benchmark was running at **Mach 1.5-2**, where weakly-compressible SPH assumes
  0.1. Sweeping `k` over 64× to Mach 0.25 leaves the onset flat (0.239 → 0.218)
  but converges the collapse: `T` at `Z = 2` falls 1.582 → 1.479, the last
  quadrupling worth under 1%. So the stock stiffness costs ~6.5% on the collapse
  rate and the answer is converged by `k ≈ 4000`.
- **The residual delay was not real.** Invariant to cohesion below 20, to
  tension, and to a 64× change in stiffness, because it was never a delay:
  `T_onset` scales as `√δ` in the measurement threshold `δ`, and the early rise
  is `Z − Z₀ ∝ T^1.45` at every `k`. That is a front accelerating from rest,
  which crosses any threshold after a finite time. Cohesion 50 is the one run
  that genuinely holds (`p = 1.66`), so that result stands.
- **Viscosity.** The Müller Laplacian term [1] - not Monaghan artificial
  viscosity, which this solver does not implement. It is the only term in the
  force model that dissipates energy. Removing it raises median peak
  acceleration 9.9×, holds
  `dt` below its ceiling in 93.5% of frames instead of 5%, and produces
  non-finite values the NaN firewall has to catch. The solver does not explode -
  it quietly runs at a third of the timestep and diverges.

### Later

- **Compare against experiment.** Martin & Moyce [8] and Koshizuka & Oka [9]
  both publish front-position data for the planar dam break; the plotting script
  is ready for a digitised dataset. Currently the only reference is the Ritter
  inviscid bound, which the measured front correctly stays below.
- **Cohesion 10 at `k = 4000`.** One run, and the only soft spot left in the
  converged sweep: cohesion 20 separates from zero by 1.6 sample intervals and
  cohesion 5 by less than half of one, so the saturation point is bracketed
  between them but not located.
- **A longer tank**, if the sustained collapse rate is ever to be measured. The
  domain is 3.4 column widths wider than the column, so the front hits the far
  wall at `Z = 4.35` while still accelerating. Onset questions are answerable as
  things stand; asymptotic-rate questions are not.
- **Viscosity sweep**, the standing hypothesis for the residual delay before the
  Mach number turned up. Worth much less now: viscous force is a velocity
  difference and so is identically zero while the column is at rest, which is
  exactly when the delay happens.
- **Move to 3D.** The neighbour count forces the kernel radius down from 4.0 to
  roughly 2.2, and cohesion scales as `h⁶`, so every tuned constant changes by
  more than an order of magnitude. Validation work ports unchanged; tuning does
  not - which is why the benchmarks come first.
- **Express parameters as dimensionless groups** (a Bond number for cohesion, a
  Reynolds-like number for viscosity) so they survive a resolution change
  instead of being silently tied to `h = 4.0`.
- **PCISPH** (predictive-corrective pressure solve [5]), trading the stiff
  equation of state for an iterative solver and larger timesteps.
- **Thermal transport.** Per-particle internal energy with SPH heat conduction
  [11] and buoyancy coupling, aiming at natural convection - a liquid application
  with an analytic onset to validate against.

## Performance

Measured on a 2013-era ultrabook - no discrete GPU:

| Component | Spec |
| --- | --- |
| CPU | Intel Core i7-4600U, 2 cores / 4 threads @ 2.1 GHz |
| GPU | Intel HD Graphics 4400 (Haswell GT2, integrated) |
| RAM | 8 GB |
| OS / driver | Fedora 44, Mesa 26.1.3 (OpenGL 4.6) |

| Particles | FPS |
| --- | --- |
| 5,000 | 60 (vsync-capped) |
| 10,000 | ~15 |

Each frame runs the full pipeline twice (RK2), so one frame = two grid
rebuilds, two density passes, and two force passes over every particle.

**What sets the frame rate.** A step of the pipe scene - 4,840 fluid plus 3,812
boundary particles - costs ~5 ms on this hardware. The frame loop drains a
real-time accumulator in `dt` increments, so it stays smooth only while

```
cost_per_step < dt / time_scale
```

`dt` is acoustic-CFL limited at `0.25 h / c ≈ 0.0094`, which puts the threshold
at 4.7 ms for `time_scale = 2` - just under the measured cost, so the pipe was
pinning `MAX_SUBSTEPS` and running at 18 fps while delivering 1.57 simulated
seconds per wall second. At `time_scale = 1.25` it holds 1.25 exactly at 47 fps.
The scene sets its own value for this reason; the wing, with 56 boundary
particles and a coarser grid, keeps 2.0 at 50 fps. Raising `time_scale` past the
threshold buys no simulated time at all - it only converts frame rate into
nothing.

Refinement costs more than it looks: `N ∝ n_x²` and `dt ∝ h ∝ 1/n_x`, so work
per simulated second goes as `n_x³`. Measured, `-n 88` runs at 0.31 simulated
seconds per wall second against 1.25 at `-n 44`.

Two startup/step costs were removed along the way. `fill_region` tested every
candidate lattice point against every boundary particle, which for the pipe's
3,812 solids took 2.1 s at spawn; bucketing the solids into a grid sized to the
exclusion radius made it `O(lattice + solids)` and dropped every scene to 0.53 s,
spawning bit-identical particles. The counting sort's prefix scan ran on a single
GPU invocation; it is now a workgroup-wide chunk-scan, verified to produce
identical `cell_starts` across grids from 84 to 11,972 cells, and worth ~12% on
the larger grids where the serial chain actually bit.

## Tech stack

- **C++17**
- **OpenGL 4.3** (core profile, compute shaders)
- **GLFW 3.4** - windowing and input (fetched automatically)
- **GLM 1.0.1** - vector math (fetched automatically)
- **Dear ImGui 1.91** - live parameter panel (fetched automatically)
- **GLAD** - OpenGL loader (pre-generated, bundled in `external/glad/`)
- **CMake** ≥ 3.20 with `FetchContent`

## Requirements

- A C++17 compiler (GCC or Clang)
- CMake ≥ 3.20
- A GPU + driver supporting **OpenGL 4.3 compute shaders**
- **X11 and/or Wayland dev headers** - GLFW builds both backends and selects
  one at runtime (`CMakeLists.txt`)

On Fedora, install everything with:

```bash
sudo dnf install mesa-libGL-devel \
    wayland-devel libxkbcommon-devel wayland-protocols-devel \
    libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
```

GLFW, GLM and Dear ImGui are downloaded and built automatically by CMake on
first configure, so no manual dependency installation is needed beyond the
system headers above.

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

No `mkdir build` needed: `-S` points at the sources and `-B` names the build
directory, which CMake creates on its own. Both commands run from the
project root.

## Running

The compiled binary is placed in `bin/`:

```bash
./bin/fluid_simulation                       # sandbox, interactive
./bin/fluid_simulation --experiment pipe     # or -e; --help lists the scenes
./bin/fluid_simulation -e wing -n 66         # -n overrides the scene's resolution
```

Scenes are also switchable at runtime with `E`, which rebuilds the simulation in
place - each brings its own resolution and panel defaults, so nothing carries
over.

Both the startup banner and an on-screen strip list the keys, generated from
the same binding table that dispatches them, so neither can fall behind:

- **Left / right mouse** - pull/push the fluid to/from the center of the cursor within a given radius
- **Space** - pause and resume
- **Left / right arrow** - step back/forward while *paused*
- **R** - respawn the scene
- **H** - show/hide the parameter panel (hidden by default)
- **E** - show/hide the experiment menu; picking one rebuilds the scene in place
- **F** - switch between particles and the reconstructed field
- **B** - drag blows a jet along +x instead of pulling toward the cursor
- **S** / **P** - colour by speed / pressure; the same key again returns to plain fluid
- **Esc** - quit

## Physics overview

Each acceleration evaluation dispatches, in order:

1. **Grid rebuild** - counting sort over grid cells (histogram, prefix sum,
   scatter), giving each cell a contiguous slice of particle indices.
2. **Density** - Wendland C2 kernel gathered in one 3×3 cell walk, plus the
   mirror images of every neighbour when the particle is within `h` of a wall.
3. **Forces** - one fused neighbour loop accumulates pressure (the same
   Wendland C2 derivative, so the gradient belongs to the density it acts
   on), Müller Laplacian viscosity, pairwise cohesion, and the same
   wall images as the density pass. Pressure comes from the Tait equation of
   state `p = k((ρ/ρ₀)⁴ − 1)`, clamped to ≥ 0.

The RK2 integrator runs this pipeline twice per step - once at the current
state, once at the half-step - then advances position from the midpoint
velocity and velocity from the midpoint acceleration, with mirror-image wall
boundaries and a hard speed cap.

Between frames, a GPU reduction reads back the maximum speed and acceleration
magnitude, and the CFL conditions `dt ≤ λ·h/v_max`, `dt ≤ λ_f·√(h/a_max)` and
`dt ≤ λ_c·h/c` pick the largest safe timestep, floored and ceilinged in
`defs.hpp`. The floor wins that clamp, so a stiff enough EOS is under-resolved
rather than slow - `main.cc` warns once when the acoustic condition asks for
less than `DT_MIN`, since nothing else in the run would say so. The
frame loop feeds physics steps from a fixed-timestep accumulator, so the
simulation stays deterministic through frame-rate spikes.

Walls are handled by mirroring rather than by spawned boundary particles [6].
Both fill the same hole - the kernel support a wall truncates - but a mirror
image is an affine transform of a neighbour already fetched into a register,
whereas a boundary particle is a separate memory read, and bandwidth is the
scarce resource here. Filling the truncated region properly needs a band of
thickness `h`, roughly four rows, which for this tank is ~2300 particles
against 4840 of fluid; they would also be sorted and walked
every frame across the whole perimeter, while mirrors cost nothing where no
fluid touches a wall. A reflected image additionally inherits the fluid's own
density and disorder, so counter-pressure rises automatically under compression
and no lattice structure is imprinted on the fluid. The trade is generality:
mirroring only expresses planar, axis-aligned walls. Anything curved, angled, or
moving requires boundary particles, and that is the point at which to switch -
so obstacles use them and the domain box does not. Measured on the dam break
with a block on the bed, the block excludes 92% of the particles that occupy its
volume in the same run without it (16 against 200); the residual is surface-layer
leakage at Mach 0.75, well past where the scheme is valid.

The image's tangential velocity is the slip condition, blended per face as
`mix(-v, v*scale, s)`. The normal component is identical at both endpoints, so
`s.x` reaches only floor and ceiling images and `s.y` only side walls, one
`vec2` uniform and no branching. Viscous drag runs as `2(1 − s)`: `s = 0.5`
is already a stationary wall, not half a wall, and useful slip lives close to
1. The floor runs no-slip because SPH's boundary layer is set by `h` rather
than by physical viscosity, so the drag band is ~4 particles thick either way;
the side walls run free-slip because partial values dragged fluid down them
visibly.

## References

- [1] M. Müller, D. Charypar, M. Gross - *Particle-Based Fluid Simulation
  for Interactive Applications*, SCA 2003.
  [PDF](https://matthias-research.github.io/pages/publications/sca03.pdf) -
  core SPH formulation: kernels, EOS pressure, viscosity, color-field
  surface tension.
- [2] S. Clavet, P. Beaudoin, P. Poulin - *Particle-based Viscoelastic Fluid
  Simulation*, SCA 2005.
  [PDF](http://www.ligum.umontreal.ca/Clavet-2005-PVFS/pvfs.pdf) -
  the near-density / near-pressure double-density relaxation. Implemented
  earlier, removed once the Tait EOS and cohesion covered the same ground.
- [3] M. Becker, M. Teschner - *Weakly Compressible SPH for Free Surface
  Flows*, SCA 2007.
  [PDF](https://cg.informatik.uni-freiburg.de/publications/2007_SCA_SPH.pdf) -
  Tait equation of state and the CFL timestep conditions.
- [4] N. Akinci, G. Akinci, M. Teschner - *Versatile Surface Tension and
  Adhesion for SPH Fluids*, SIGGRAPH Asia 2013.
  [PDF](https://cg.informatik.uni-freiburg.de/publications/2013_SIGGRAPHASIA_surfaceTensionAdhesion.pdf) -
  the pairwise cohesion force used for droplet formation.
- [5] B. Solenthaler, R. Pajarola - *Predictive-Corrective Incompressible
  SPH*, SIGGRAPH 2009.
  [PDF](https://www.ifi.uzh.ch/dam/jcr:ffffffff-daa5-74d6-0000-00005a4f5c99/pcisph.pdf) -
  PCISPH, the planned next solver.
- [6] M. Ihmsen, N. Akinci, M. Gissler, M. Teschner - *Boundary Handling and
  Adaptive Time-stepping for PCISPH*, VRIPHYS 2010.
  [PDF](https://cg.informatik.uni-freiburg.de/publications/2010_VRIPHYS_boundaryHandling.pdf) -
  the velocity + acceleration adaptive time-stepping criteria.
- [7] D. Koschier, J. Bender, B. Solenthaler, M. Teschner - *Smoothed
  Particle Hydrodynamics Techniques for the Physics Based Simulation of
  Fluids and Solids*, Eurographics Tutorial 2019.
  [PDF](https://sph-tutorial.physics-simulation.org/pdf/SPH_Tutorial.pdf) -
  survey of modern SPH: kernels, WCSPH, PCISPH, and beyond.

- [8] J. C. Martin, W. J. Moyce - *An Experimental Study of the Collapse of
  Liquid Columns on a Rigid Horizontal Plane*, Philosophical Transactions of
  the Royal Society A 244, 1952 - the planar dam break front-position data the
  benchmark is measured against.
- [9] S. Koshizuka, Y. Oka - *Moving-Particle Semi-implicit Method for
  Fragmentation of Incompressible Fluid*, Nuclear Science and Engineering 123,
  1996 - the second standard dam break reference dataset.
- [10] A. Ritter - *Die Fortpflanzung der Wasserwellen*, Zeitschrift des
  Vereines Deutscher Ingenieure 36, 1892 - the shallow-water dam break
  solution used as the inviscid upper bound on the surge front.
- [11] P. W. Cleary, J. J. Monaghan - *Conduction Modelling Using Smoothed
  Particle Hydrodynamics*, Journal of Computational Physics 148, 1999 - the
  pairwise-antisymmetric heat conduction form planned for thermal transport.
- [12] D. Molteni, A. Colagrossi - *A Simple Procedure to Improve the Pressure
  Evaluation in Hydrodynamic Context Using the SPH*, Computer Physics
  Communications 180, 2009; refined by M. Antuono, A. Colagrossi, S. Marrone -
  *Numerical Diffusive Terms in Weakly-Compressible SPH Schemes*, Computer
  Physics Communications 183, 2012 - the density-diffusion term (δ ≈ 0.1) that
  damps the pressure noise the reconstructed field currently only hides.
- [13] S. J. Lind, R. Xu, P. K. Stansby, B. D. Rogers - *Incompressible Smoothed
  Particle Hydrodynamics for Free-Surface Flows*, Journal of Computational
  Physics 231, 2012; A. Skillen, S. Lind, P. K. Stansby, B. D. Rogers -
  *Incompressible SPH with Reduced Temporal Noise*, CMAME 265, 2013 - particle
  shifting, the remedy for anisotropic packing under shear.

**Further reading** (consulted, not directly used):

- R. Bridson, M. Müller-Fischer - *Fluid Simulation*, SIGGRAPH 2007 course
  notes ([PDF](https://www.cs.ubc.ca/~rbridson/fluidsimulation/fluids_notes.pdf)) -
  grid-based (Eulerian) fluid simulation.
- D. Price - *Smoothed Particle Hydrodynamics and Magnetohydrodynamics*
  ([arXiv](https://arxiv.org/pdf/1110.3711)) - SPH theory from the
  astrophysics side.
- R. Klessen - *SPH lecture, Cardiff 2002*
  ([PDF](https://www.ita.uni-heidelberg.de/research/klessen/people/klessen/publications/presentations/2002-04-26-SPH-lecture-cardiff.pdf)) -
  introductory SPH lecture slides.
- *Fluid Simulation Tutorial*
  ([web](https://unusualinsights.github.io/fluid_tutorial/)) - hands-on
  particle fluid walkthrough.
- *SPH numerical simulation of peak pressure in near-field underwater
  explosion* ([AIP Advances](https://pubs.aip.org/aip/adv/article/13/9/095027/2913440)) -
  SPH applied to shock/pressure peaks.

## License

MIT - see [LICENSE](LICENSE).
