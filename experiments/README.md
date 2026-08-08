# Experiments

Measurement runs, the gnuplot scripts that plot them, and the resulting figures.
Everything here is raw output from the simulator plus the scripts needed to
reproduce the plots - no derived conclusions, so the numbers can be re-checked.

Each script writes a PNG when given an `out` variable, or opens an interactive
window without one:

```
gnuplot dam_break_sweep.plt                                   # interactive
gnuplot -e "out='dam_break_sweep.png'" dam_break_sweep.plt    # write the figure
```

Run them from inside this directory - the data paths are relative.

## Dam break benchmark

Planar dam break: a column of width `a` and height `n·a` released against the
left wall, tracking the surge front along the floor. Results are reported
dimensionlessly (`Z = (x_front − x₀)/a`, `T = t·√(2g/a)`) so they do not depend
on the tank size. Column as spawned: 46 × 111 particles, `a = 48.49`, realised
aspect `n = 2.41` (the requested 2.5 is clamped to fit the domain height).

| File | Contents |
| --- | --- |
| `dam_break_coh50_run1.csv` | cohesion 50, first run |
| `dam_break_coh50.csv` | cohesion 50, repeat run |
| `dam_break_coh20.csv` | cohesion 20 |
| `dam_break_coh5.csv` | cohesion 5 |
| `dam_break_coh0.csv` | cohesion 0 |
| `dam_break.plt` → `dam_break.png` | single run against the Ritter bound |
| `dam_break_sweep.plt` → `dam_break_sweep.png` | all four cohesion values overlaid |

The dashed line in both figures is the Ritter (1892) shallow-water solution,
`Z = 1 + √(2n)·T`, the inviscid frictionless upper bound. A measured front must
sit below it; a curve above it means the front criterion is tracking spray
rather than fluid.

**Reproducibility.** The two cohesion-50 runs agree to three significant figures
(plateau `T = 0.51`, `T` at `Z = 2` of 1.88) despite frame timing giving them
different `dt` sequences, as do the cohesion-0 and cohesion-5 runs at 0.23/1.53.

**Result.** Cohesion 50 delays the collapse - the column holds itself up for
roughly twice as long before the front moves. Below 20 the effect saturates and
cohesion 0, 5 and 20 are almost indistinguishable. The sustained collapse rate
is far less sensitive (61-65% of the Ritter slope across the whole sweep), so
cohesion acts as a static restraint on the onset rather than as ongoing drag.
A residual delay (`T ≈ 0.23`) survives at zero cohesion and is not yet
explained - viscosity is the leading candidate.

Not yet compared against experiment: Martin & Moyce (1952) tabulate front
position for the planar case and Koshizuka & Oka (1996) give a second dataset.
`dam_break.plt` has a commented plot line ready for a digitised `martin_moyce.csv`
(two columns, `T,Z`). Use the curve for a matching aspect ratio - `n` changes the
answer.

## Artificial viscosity

Matched pair, identical in every parameter except the viscosity coefficient.

| File | Contents |
| --- | --- |
| `run_visc3.14.csv` | viscosity 3.14 (default) |
| `run_visc0.csv` | viscosity 0 |
| `run_visc*_smoothed.csv` | rolling-median series used by the figure |
| `viscosity_compare.plt` → `viscosity_comparison.png` | three-panel comparison |

Columns are `step, sim_time, dt, max_speed, max_accel`. Note that
`decode_max_bits` reports `1e6` when the reduction sees a non-finite value, so
rows at exactly `1000000` are the NaN sentinel firing, not a real acceleration.

**Result.** Over the 32.5 s both runs cover, removing viscosity raises median
peak acceleration 9.9× and median peak speed 5.6×, pushes `dt` below its ceiling
in 93.5% of frames (versus 5.0%), and twice produces non-finite values that drive
`dt` to its floor. With viscosity the run settles - peak speed falls to a third
of its early value; without it, peak acceleration nearly triples across the run
and is still climbing. Artificial viscosity is the only term in the force model
that dissipates energy, and this is what its absence costs.

## dt trace

`dt.png` plots the adaptive timestep against the two CFL candidate timesteps,
showing which condition is actually limiting `dt` at each moment. Over that run
the velocity condition never bound `dt` while the acceleration condition bound it
in 5.5% of frames - which is why the trace plots the candidates rather than raw
`max_speed`.

The source CSV for this figure was overwritten by later runs and is not
recoverable. The live plotting script is `dt.plt` at the repository root, which
reads the current `run.csv`.
