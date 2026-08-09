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

`front_metrics.py` reduces any front trace to the numbers a sweep is judged on,
so a claim below can be re-derived instead of remembered:

```
python3 front_metrics.py                     # every dam_break*.csv here
python3 front_metrics.py dam_break_coh0.csv --slopes
```

It reports, per run: the usable sample count (see *Tank length* below), the
onset `T`, interpolated `T` at `Z = 1.5, 2, 3`, and the local `dZ/dT` profile as
a percentage of the Ritter slope. Definitions and the reasoning behind each
threshold are in the script's docstring. Two conventions differ slightly from
the numbers quoted in earlier revisions of this file: crossings are interpolated
between samples rather than taken at the nearest sample (worth 0.01-0.04 in `T`,
since adaptive `dt` makes sample spacing vary by an order of magnitude within a
run), and slopes are reported as a profile rather than a single figure.

## Dam break benchmark

Planar dam break: a column of width `a` and height `n·a` released against the
left wall, tracking the surge front along the floor. Results are reported
dimensionlessly (`Z = (x_front − x₀)/a`, `T = t·√(2g/a)`) so they do not depend
on the tank size. Column as spawned: 46 × 111 particles, `a = 48.49`, realised
aspect `n = 2.41` (the requested 2.5 is clamped to fit the domain height).

| File | Contents |
| --- | --- |
| `dam_break_coh50_run1.csv` | cohesion 50, first run, `k = 250` |
| `dam_break_coh50.csv` | cohesion 50, repeat run, `k = 250` |
| `dam_break_coh20.csv` | cohesion 20, `k = 250` |
| `dam_break_coh5.csv` | cohesion 5, `k = 250` |
| `dam_break_coh0.csv` | cohesion 0, `k = 250` |
| `dam_break_coh<V>_visc3.14_ten20.csv` | the same four values re-run at `k = 4000` |
| `dam_break_coh0_visc3.14_ten0_k<K>.csv` | the stiffness sweep, `K` = 250 … 16000 |
| `dam_break.plt` → `dam_break.png` | single run against the Ritter bound |
| `dam_break_sweep.plt` → `dam_break_sweep.png` | the four cohesion values at `k = 250` |
| `sweep.plt` → `dam_break_sweep_k4000.png` | the same four at `k = 4000` |
| `front_metrics.py` | onset, crossings, slope profile, usable range |

The two sweep figures share their axes and differ only in stiffness, so they
invite a comparison that is easy to get backwards: where `dam_break_sweep.png`
has cohesion 0, 5 and 20 lying on top of one another, `dam_break_sweep_k4000.png`
separates 20 out. That difference is the pressure multiplier, not cohesion.

### Running the next sweep

Set a value in the ImGui panel, press **R**, let it run past `T ≈ 3.7`, repeat.
Each combination writes its own file, named after all three force parameters:

```
dam_break_coh<cohesion>_visc<viscosity>_ten<tension>.csv
dam_break_coh0_visc3.14_ten20.csv        # cohesion 0 at the stock settings
```

The five archived runs above keep their original `dam_break_coh<V>.csv` names
and their four-column layout; new runs carry `cohesion,viscosity,tension`
alongside `sim_time,front_x,Z,T`. Both `front_metrics.py` and the plot scripts
read either. Naming only the swept parameter was enough while cohesion was the
only thing that moved, but a viscosity sweep at fixed cohesion would have
written the same name four times and landed in `_2`, `_3`, `_4`, where the
filename no longer says which run is which.

Then overlay them:

```
gnuplot -e "files='dam_break_coh0_visc0_ten20.csv dam_break_coh0_visc3.14_ten20.csv'; \
            labels='visc0 visc3.14'; name='Artificial viscosity'; \
            out='viscosity_sweep.png'" sweep.plt
```

`dam_break_sweep.plt` is left as it is: it reproduces the archived cohesion
figure from the archived files, and pinning a published figure to its exact
inputs is worth more than folding it into something general.

The dashed line in both figures is the Ritter (1892) shallow-water solution,
`Z = 1 + √(2n)·T`, the inviscid frictionless upper bound. A measured front must
sit below it; a curve above it means the front criterion is tracking spray
rather than fluid.

**Reproducibility.** The two cohesion-50 runs agree to three significant figures
(onset `T = 0.487`, `T` at `Z = 2` of 1.842 and 1.843) despite frame timing
giving them different `dt` sequences, as do the cohesion-0 and cohesion-5 runs at
0.215/1.514. The agreement is tighter early than late: both cohesion-50 runs sit
at the `dt` ceiling for the first second or so and step identically, and only
diverge once the adaptive timestep starts responding to the flow.

**Tank length.** The front runs out of tank at `Z = 4.3476`. The column spawns
against the left wall and the domain is only 3.4 column widths wider than it, so
the collapse is over by `T ≈ 3.3-3.7` and 87-91% of every archived CSV is the
front pinned against the far wall. The figures are unaffected - both cap the
x-axis below that - but any fit must be restricted to the usable range, which is
what `front_metrics.py` does before reporting anything.

**Result.** Cohesion 50 delays the collapse - the column holds itself up for
roughly twice as long before the front moves (onset `T = 0.487` against 0.215).
Below 20 the effect saturates and cohesion 0, 5 and 20 are almost
indistinguishable. Cohesion therefore acts as a static restraint on the onset.

> **Superseded in part.** The second sentence is an artifact of the stock
> stiffness and does not survive the re-run at `k = 4000`; see *Cohesion sweep,
> converged*. The first and third stand.

Whether it also acts as ongoing drag cannot be settled from these runs, because
the front never reaches a steady speed to compare: `dZ/dT` climbs monotonically
from 5-14% of the Ritter slope at onset to 69-72% at the wall, in every run,
with no plateau anywhere. A "sustained collapse rate" quoted from these traces
is a statement about which window was chosen. The whole measurement sits inside
the startup transient, and Ritter - whose front leaves at full speed at `t = 0`
- has no startup transient to compare it against.

That also bears on the residual delay at zero cohesion. `T = 0.229` is not
wholly physical: the onset threshold requires the front to clear 0.05 column
widths, which costs `0.05/√(2n) = 0.023` even for a front moving at the full
Ritter speed, leaving ~0.21 to explain. See the next two sections, which explain
it and in doing so put every number above inside a caveat.

## Surface-tension control

| File | Contents |
| --- | --- |
| `dam_break_coh0_visc3.14_ten0_k250.csv` | cohesion 0, tension 0, else stock |

That is the same file as the `k = 250` row of the stiffness sweep below, not a
copy of it: the control was run at the stock pressure multiplier, which made it
the natural baseline for the sweep that followed, and it was renamed to carry
`k` once there were three siblings to distinguish it from.

Cohesion and the Müller color-field tension are separate additive terms in
`force.comp` (`:231` and `:239`), and the cohesion sweep varied only the first:
`tension_strength` sat at 20 with its threshold at 0 in every run, so the term
was active essentially everywhere and "cohesion 0" was never "no surface
tension". This run turns it off.

**Result.** Onset moves from 0.229 to 0.239 - less than one sample interval
(`ΔT ≈ 0.038` near the start), so within the measurement's resolution, nothing.
Crossings shift ~4% later, in the direction of a slightly *slower* collapse. Two
conclusions: the residual delay is not surface tension, and the Müller term at
strength 20 is doing almost nothing in this scenario.

## The Mach number

Not a run - arithmetic on the committed constants, prompted by having eliminated
every force-model term:

```
numerical sound speed  c = √(4k/ρ₀), k = 250, ρ₀ = 0.9   =  33.3
measured peak front speed (a·(dZ/dT)·√(2g/a) at 72%)     =  48.5   -> Mach 1.45
Ritter front speed 2√(gH), H = 117.0                     =  67.8   -> Mach 2.03
```

`c` is the wave speed of the equation of state in `force.comp:72`,
`p = k((ρ/ρ₀)⁴ − 1)`, whose stiffness at rest density is `dp/dρ = 4k/ρ₀`. It is
not a physical sound speed - it is the free parameter that makes the fluid stiff
enough to stay near `ρ₀` - and weakly-compressible SPH assumes the flow stays
well below it, conventionally by a factor of ten. **This benchmark runs at Mach
1.5-2.**

The predicted consequences were two, and the `k` sweep below tested both. One
held, one did not:

- **The onset delay is numerical.** Hydrostatic pressure at the column base is
  `ρ₀gH = 1033`, and carrying that through the Tait EOS at `k = 250` needs
  `ρ/ρ₀ = 1.505` - **51% compression**. The column spawns at uniform rest
  density, hence `p ≈ 0` everywhere, so the argument ran that it must squash
  itself before it can spread. **Wrong** - see below.
- **The front cannot reach the Ritter slope**, that slope being Mach 2 in this
  fluid. Partly right, and worth 6.5% - see below.

Everything measured above is therefore provisional: the sweeps are real
measurements, of a solver operating outside its assumptions. Reaching the
conventional Mach 0.1 needs `k ≈ 1.0×10⁵` - 413× the current value - and with it
an acoustic timestep of 0.0024 against the present 0.015 ceiling, so roughly six
times the wall-clock cost per unit of sim time.

**Now enforced.** `main.cc` gained a third CFL condition, `dt ≤ λ_c·h/c`, so the
timestep tracks the stiffness automatically. It is deliberately slack at stock
settings - 0.048 against the 0.015 ceiling - so every run archived here is
unaffected and remains comparable. It starts binding at `k ≈ 4000`, and warns
once if it ever asks for less than `DT_MIN`.

## Stiffness sweep

Cohesion 0, tension 0, everything else stock, varying `pressure`.

| File | `k` | `c` | Mach vs Ritter | `dt` used |
| --- | --- | --- | --- | --- |
| `dam_break_coh0_visc3.14_ten0_k250.csv` | 250 | 33 | 2.03 | 0.015 |
| `dam_break_coh0_visc3.14_ten0_k1000.csv` | 1000 | 67 | 1.02 | 0.015 |
| `dam_break_coh0_visc3.14_ten0_k4000.csv` | 4000 | 133 | 0.51 | 0.012 |
| `dam_break_coh0_visc3.14_ten0_k16000.csv` | 16000 | 267 | 0.25 | 0.0060 |

The prediction was that onset shrinks as `1/√k`. It does not.

| `k` | onset `T` | `T` at `Z=2` | `T` at wall | peak `dZ/dT` |
| --- | --- | --- | --- | --- |
| 250 | 0.239 | 1.582 | 3.38 | 71% |
| 1000 | 0.248 | 1.533 | 3.23 | 69% |
| 4000 | 0.229 | 1.492 | 3.14 | 72% |
| 16000 | 0.218 | 1.479 | 3.09 | 74% |

**Onset is invariant.** 64× in stiffness, Mach 2.03 down to 0.25, and onset
moves 0.239 → 0.218 - a 12% scatter with no trend worth the name. The
compression argument is dead: whatever sets the onset, it is not the column
having to squash itself first.

**The collapse converges.** `T` at `Z = 2` falls 1.582 → 1.533 → 1.492 → 1.479,
successive changes of -3.1%, -2.7%, -0.9%. So the stock setting was costing
about 6.5% on the collapse rate, and the answer is **stiffness-converged by
`k ≈ 4000`** - the last quadrupling buys under 1% for 2× the wall-clock. That is
the useful form of the Mach finding: not "outside its assumptions" but a
quantified 6.5%, with a converged value to quote instead.

## There was never a residual delay

The thing three studies were chasing turns out to be an artifact of how onset
was defined. Two tests, both on data already in hand:

**Onset scales with the threshold.** `T_onset` measured at
`δ = 0.0125, 0.025, 0.05, 0.1, 0.2` gives `T_onset/√δ` constant to within ~30%
for every cohesion-0 run, at every `k`. A front accelerating from rest crosses
*any* threshold after a finite time; calling that time a delay reifies the
threshold. There is nothing left over to explain.

**The early rise is a power law, invariant to `k`.** Fitting
`Z − Z₀ = A·T^p` over `0.01 < Z − Z₀ < 0.3`:

| run | `p` |
| --- | --- |
| k=250 | 1.46 |
| k=1000 | 1.47 |
| k=4000 | 1.45 |
| k=16000 | 1.45 |
| cohesion 0, tension 20 | 1.51 |
| cohesion 20 | 1.48 |
| **cohesion 50** | **1.66** |

`p ≈ 1.5` is what a front accelerating from rest and turning over toward
constant velocity gives when fitted across that transition - the `T²` and `T¹`
stages straddled. Flat to four significant figures across a 64× change in
stiffness.

**And the cohesion result survives.** Cohesion 50 is the one run that does not
fit the pattern: `p = 1.66`, and `T_onset/√δ` ranging 1.45-2.26 against
0.85-1.33 for everything else. That is the signature of an actual hold followed
by a release, which is what cohesion was claimed to do. So "cohesion restrains
the onset" stands; what does not stand is the idea that there was a mystery at
cohesion 0 needing an explanation.

## Cohesion sweep, converged

The sweep redone at `k = 4000`, everything else as the original - viscosity
3.14, tension 20 - so the only variable against the archived runs is stiffness.
Files are `dam_break_coh<V>_visc3.14_ten20.csv`.

| cohesion | onset `T` (k=250) | onset `T` (k=4000) | `T` at `Z=2`, Δ | early `p` |
| --- | --- | --- | --- | --- |
| 0 | 0.215 | 0.209 | −3.3% | 1.47 |
| 5 | 0.215 | 0.221 | −1.1% | 1.52 |
| 20 | 0.218 | 0.260 | −0.1% | 1.48 |
| 50 | 0.487 | 0.487 | −10.7% | 1.70 |

Reproduce the table and the figure with:

```
python3 front_metrics.py dam_break_coh{0,5,20,50}_visc3.14_ten20.csv
gnuplot -e "files='dam_break_coh0_visc3.14_ten20.csv dam_break_coh5_visc3.14_ten20.csv \
                   dam_break_coh20_visc3.14_ten20.csv dam_break_coh50_visc3.14_ten20.csv'; \
            labels='coh0 coh5 coh20 coh50'; name='cohesion (k = 4000)'; \
            out='dam_break_sweep_k4000.png'" sweep.plt
```

Onsets here are interpolated. They have to be: the sample interval is
`ΔT ≈ 0.031`, and neighbouring sweep points differ by less than that, so
nearest-sample onset quantises the whole comparison onto the logging grid and
manufactures a clean monotone trend out of three values.

**Cohesion 50 holds, as predicted.** `p = 1.70` against 1.47-1.52, onset 2.34×
cohesion 0. The headline result was not a Mach artifact.

The figure adds one thing the table cannot: past its knee the cohesion-50 curve
runs *parallel* to the others rather than staying proportionally behind them, so
cohesion buys a delay and then stops mattering. That is consistent with the
static-restraint reading, but it is an eyeball observation about curves that are
still accelerating everywhere, not a drag measurement - see *Tank length* for why
no drag measurement is available from these runs.

**The saturation claim was wrong, and the stock stiffness is why.** At `k = 250`
cohesion 0, 5 and 20 were indistinguishable (1.00×, 1.01×), which is where
"below 20 the effect saturates" came from. Converged, cohesion 20 separates out
at 1.25× - a shift of 0.051 in `T`, about 1.6 sample intervals, so resolved.
Cohesion 5 at 1.06× is still under half an interval and remains unresolved. The
saturation point is therefore somewhere between 5 and 20, not at 20; the
compressible fluid's own squashing had been swamping a real cohesive restraint.
Pinning it any tighter needs one more run at cohesion 10, which is the only gap
left in this sweep.

**The stiffness correction is not uniform**, which is the other reason the
original sweep could not have been fixed by scaling it. `T` at `Z = 2` moves
−3.2%, −1.1%, −0.1%, −10.7% across the four - stiffness interacts with cohesion
rather than offsetting everything equally.

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

Two, not three: the acoustic condition `dt ≤ λ_c·h/c` added since is a constant
for a given run, and `run.csv` logs step, time, `dt`, max speed and max
acceleration but not the pressure multiplier, so `c` cannot be reconstructed from
the file. It shows up in the trace only as a ceiling `dt` never exceeds.

The source CSV for this figure was overwritten by later runs and is not
recoverable. The live plotting script is `dt.plt` at the repository root, which
reads the current `run.csv`.
