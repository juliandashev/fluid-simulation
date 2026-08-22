#!/usr/bin/env python3
"""Reduce a dam break front trace to the numbers a sweep is compared on.

The cohesion sweep's conclusions were read off the figure by eye. That works
once, but a second sweep has to be compared against the first, and "the plateau
looks about twice as long" does not survive contact with a third. This computes
the same quantities from the CSV so a claim in the README can be re-derived
rather than remembered.

    python3 front_metrics.py                     # every dam_break*.csv here
    python3 front_metrics.py dam_break_coh0.csv  # just these
    python3 front_metrics.py --slopes            # add the local slope profile
    python3 front_metrics.py --fit-band=0.05:1.0 # widen the early-rise fit band

Definitions, and why each one is what it is:

  onset T   The first T where Z exceeds its t=0 value by ONSET_DELTA (0.05
            column widths ~ 2.4 world units ~ 2.3 particle spacings). The front
            has to clear the lattice discretisation before "it moved" means
            anything, and one spacing is the smallest displacement the
            measurement can resolve at all. Ritter's front leaves at finite
            speed, so this threshold costs a real curve ONSET_DELTA/sqrt(2n) =
            0.023 of apparent delay - subtract that before calling a delay
            physical.

  T at Z    Linearly interpolated crossing of Z = 1.5, 2, 3. Interpolated, not
            nearest-sample: the log is written every 4th step and dt is
            adaptive, so sample spacing in T varies by an order of magnitude
            within a run and nearest-sample crossings inherit that jitter.

  slope     dZ/dT from least squares over a centred window, reported as a
            percentage of the Ritter slope sqrt(2n). Reported as a profile
            rather than a single number on purpose: see below.

  exponent  Least-squares fit of Z - Z0 = C*T^p over a band in *displacement*,
            not in time: the band runs from ONSET_DELTA (where onset is already
            declared) to FIT_BAND_HI column widths. Banding on displacement is
            what makes the number comparable across a sweep - a fixed T window
            catches cohesion 0 mid-collapse and cohesion 50 still standing
            still, and then reports the difference as an exponent. p ~ 1.5 is a
            front accelerating from rest; p much above that is a column that
            held and then let go. R^2 is printed with it because that second
            case is not a power law at all, and a lone exponent hides it.

  Re, Bo    Reynolds and Bond numbers built from the logged force parameters,
            each also given as a multiple of the real experiment's. This is the
            only line that says whether a run is anywhere near the fluid it is
            meant to represent; the sim-unit values cannot.

  wall      Z saturates when the front reaches the far wall. Everything logged
            after that is the pile-up, not the collapse, and must be excluded
            from any fit. The saturation value is detected from the data (a run
            of identical front_x at the end), not assumed from the domain size.

Both the 4-column runs (sim_time,front_x,Z,T) and the 6-column ones that also
carry the swept parameters are read; the extra columns are reported when there.
"""

from __future__ import annotations

import csv
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

# Realised column aspect: create_column clamps rows to fit the domain, so the
# 2.5 requested in defs.hpp spawns as 46 x 111 -> n = 117.0/48.49. Using the
# requested aspect instead biases the Ritter slope by 4%. Keep in sync with the
# value the .plt scripts use.
ASPECT = 2.41
RITTER_SLOPE = math.sqrt(2.0 * ASPECT)

# Scene constants needed to nondimensionalise. Not logged per sample - they are
# compile-time in defs.hpp - so they live here and must be kept in step with it.
RHO_0 = 0.9  # TARGET_DENSITY
GRAVITY = 9.81
COLUMN_A = 48.49  # a, the realised column width (create_column clamps rows to fit)

# What the real planar dam break runs at: water, a = 2.25 in = 0.0572 m, so
# U = sqrt(g*a) = 0.749 m/s, nu = 1e-6 m^2/s, sigma = 0.0728 N/m. Quoted so a
# run here can be read against the experiment it is trying to reproduce rather
# than against nothing.
RE_EXPERIMENT = 4.3e4
BO_EXPERIMENT = 4.4e2

ONSET_DELTA = 0.05  # column widths the front must clear to count as "moved"
WALL_TOL = 0.01  # column widths (~half a particle spacing) of slack at the wall
CROSSINGS = (1.5, 2.0, 3.0)
SLOPE_HALF_WINDOW = 0.15  # in T, each side of the centre point
FIT_BAND_HI = 0.5  # column widths; upper edge of the early-rise fit band


def read_run(path: Path) -> dict:
    """Parse one front trace, trimming the tail where the front sits at the wall.

    The trailing wall pile-up is not a small correction to be fitted around: in
    the archived runs it is most of the file. Detected from the data rather than
    by comparing against DOMAIN_HALF_X, so this stays correct if the domain is
    ever resized.

    The front does not go *still* at the wall - it packs, rebounds and creeps
    back a fraction of a spacing - so "unchanged front_x from here to the end"
    finds the moment the jitter stops, tens of seconds too late. What actually
    marks the end of the collapse is the front first arriving: it advances
    monotonically until the wall stops it, and never exceeds that maximum
    afterwards. So the first sample within WALL_TOL of the run's maximum Z is
    the last sample that is still measuring a free surge.
    """
    with path.open(newline="") as fh:
        rows = list(csv.reader(fh))

    header, body = rows[0], [r for r in rows[1:] if r]
    t = [float(r[0]) for r in body]
    front_x = [float(r[1]) for r in body]
    z = [float(r[2]) for r in body]
    big_t = [float(r[3]) for r in body]

    # Swept parameters, present only in runs logged after the columns were added.
    params = {}
    for i, name in enumerate(header[4:], start=4):
        if all(len(r) > i for r in body):
            values = {float(r[i]) for r in body}
            params[name] = values.pop() if len(values) == 1 else "varied mid-run"

    z_max = max(z)
    arrived = next((i for i, v in enumerate(z) if v >= z_max - WALL_TOL), None)
    # A run cut short before the wall has its maximum at the last sample; that
    # is the front still moving, not a plateau, so keep the whole trace.
    at_wall = arrived if arrived is not None and arrived < len(z) - 1 else None

    return {
        "path": path,
        "t": t,
        "Z": z,
        "T": big_t,
        "params": params,
        "n_samples": len(body),
        # First index at the wall, or None if the run never got there.
        "wall_at": at_wall,
        "Z_wall": z_max,
    }


def usable(run: dict) -> tuple[list[float], list[float]]:
    """(T, Z) with the wall plateau removed - the part that is still physics."""
    end = run["wall_at"] if run["wall_at"] is not None else len(run["T"])
    return run["T"][:end], run["Z"][:end]


def onset(T: list[float], Z: list[float], delta: float = ONSET_DELTA) -> float | None:
    """Interpolated T at which the front has cleared `delta` column widths.

    Interpolated for the same reason the crossings are, and it matters more
    here: onset differences between neighbouring sweep points can be smaller
    than the gap between samples, and taking the first sample that exceeds the
    threshold quantises them onto that grid. Reading nearest-sample onsets off a
    stiffness-converged cohesion sweep invents a monotone trend across 0, 5 and
    20 that interpolation shows is one resolved step, not three.
    """
    return crossing(T, Z, Z[0] + delta)


def crossing(T: list[float], Z: list[float], level: float) -> float | None:
    """Interpolated T at which Z first reaches `level`."""
    for i in range(1, len(Z)):
        if Z[i] >= level > Z[i - 1]:
            span = Z[i] - Z[i - 1]
            frac = (level - Z[i - 1]) / span if span else 0.0
            return T[i - 1] + frac * (T[i] - T[i - 1])
    return None


def local_slope(T: list[float], Z: list[float], centre: float) -> float | None:
    """Least-squares dZ/dT over a window centred on `centre`."""
    pts = [(t, z) for t, z in zip(T, Z) if abs(t - centre) <= SLOPE_HALF_WINDOW]
    if len(pts) < 4:
        return None
    m = len(pts)
    st = sum(p[0] for p in pts)
    sz = sum(p[1] for p in pts)
    stt = sum(p[0] * p[0] for p in pts)
    stz = sum(p[0] * p[1] for p in pts)
    denom = m * stt - st * st
    return (m * stz - st * sz) / denom if denom else None


def dimensionless(params: dict) -> dict | None:
    """Reynolds and Bond numbers for a run, from its logged force parameters.

    Both use the column width a as the length scale and U = sqrt(g*a) as the
    velocity scale - the free-fall speed over one column width, which is the
    scale the T nondimensionalisation is already built on. Any consistent choice
    works as long as the experiment is reduced the same way; this one is stated
    rather than assumed because Re moves with the square of a bad U.

    Re = U*a/nu with nu = mu/rho_0, and `viscosity` is Mueller dynamic viscosity
    (force.comp:182 and :262 apply it as mu, so no alpha conversion is needed).
    Bo = rho_0*g*a^2/sigma with sigma = `tension`, which the Mueller color-field
    term uses directly. Zero viscosity or tension gives an infinite ratio, which
    is the honest answer, not an error.
    """
    if "viscosity" not in params and "tension" not in params:
        return None

    u = math.sqrt(GRAVITY * COLUMN_A)
    out = {}

    if "viscosity" in params:
        mu = params["viscosity"]
        nu = mu / RHO_0
        out["Re"] = math.inf if nu <= 0.0 else u * COLUMN_A / nu

    if "tension" in params:
        sigma = params["tension"]
        out["Bo"] = math.inf if sigma <= 0.0 else RHO_0 * GRAVITY * COLUMN_A**2 / sigma

    return out or None


def power_fit(T: list[float], Z: list[float],
              lo: float = ONSET_DELTA, hi: float = FIT_BAND_HI) -> dict | None:
    """Fit Z - Z0 = C*T^p over the displacement band [lo, hi], by log-log least squares.

    Returns the exponent, its R^2, and the T range the band actually spanned,
    so a reader can see which part of the run produced the number. The band is
    in column widths cleared, so every run is fitted over the same stage of its
    own collapse rather than the same clock window.
    """
    z0 = Z[0]
    pts = [(math.log(t), math.log(z - z0))
           for t, z in zip(T, Z) if t > 0.0 and lo <= z - z0 <= hi]
    if len(pts) < 4:
        return None

    m = len(pts)
    sx = sum(x for x, _ in pts)
    sy = sum(y for _, y in pts)
    mx, my = sx / m, sy / m
    sxy = sum((x - mx) * (y - my) for x, y in pts)
    sxx = sum((x - mx) ** 2 for x, _ in pts)
    if not sxx:
        return None
    slope = sxy / sxx

    syy = sum((y - my) ** 2 for _, y in pts)
    r2 = (sxy * sxy) / (sxx * syy) if syy else 1.0

    return {"p": slope, "r2": r2, "n": m,
            "t_lo": math.exp(pts[0][0]), "t_hi": math.exp(pts[-1][0])}


def fmt(value: float | None, spec: str = "6.3f") -> str:
    return format(value, spec) if value is not None else "     -"


def report(run: dict, show_slopes: bool,
           band: tuple[float, float] = (ONSET_DELTA, FIT_BAND_HI)) -> None:
    T, Z = usable(run)
    name = run["path"].name

    print(f"\n=== {name}")
    if run["params"]:
        joined = ", ".join(f"{k}={v}" for k, v in run["params"].items())
        print(f"    logged parameters: {joined}")

        # Printed next to the raw values because the raw values are in sim
        # units and mean nothing on their own: viscosity 3.14 is only large or
        # small relative to h, rho_0 and the flow speed.
        nd = dimensionless(run["params"])
        if nd:
            bits = []
            if "Re" in nd:
                bits.append(f"Re = {nd['Re']:.3g} ({nd['Re'] / RE_EXPERIMENT:.2g}x experiment)"
                            if math.isfinite(nd["Re"]) else "Re = inf (inviscid)")
            if "Bo" in nd:
                bits.append(f"Bo = {nd['Bo']:.3g} ({nd['Bo'] / BO_EXPERIMENT:.2g}x experiment)"
                            if math.isfinite(nd["Bo"]) else "Bo = inf (no tension)")
            print("    dimensionless:     " + ",  ".join(bits))
    else:
        print("    logged parameters: none (pre-dates the parameter columns)")

    kept, total = len(T), run["n_samples"]
    if run["wall_at"] is not None:
        print(f"    front reaches the far wall at T = {run['T'][run['wall_at']]:.2f}, "
              f"Z = {run['Z_wall']:.4f}")
        print(f"    usable samples: {kept}/{total} "
              f"({100.0 * (total - kept) / total:.0f}% of the file is wall pile-up)")
    else:
        print(f"    usable samples: {kept}/{total} (front never reached the wall)")

    t_on = onset(T, Z)
    floor = ONSET_DELTA / RITTER_SLOPE
    print(f"    Z0 = {Z[0]:.4f}   onset T = {fmt(t_on)}  "
          f"(threshold alone costs {floor:.3f}, so physical delay ~ "
          f"{fmt(t_on - floor if t_on else None, '.3f')})")

    parts = [f"Z={lvl}: T={fmt(crossing(T, Z, lvl), '.3f')}" for lvl in CROSSINGS]
    print("    crossings   " + "   ".join(parts))

    fit = power_fit(T, Z, *band)
    if fit is None:
        print(f"    exponent    - (fewer than 4 samples in Z-Z0 = "
              f"[{band[0]}, {band[1]}])")
    else:
        flag = "" if fit["r2"] >= 0.99 else "   <- poor fit, not a power law"
        print(f"    exponent    Z-Z0 ~ T^{fit['p']:.2f}   R^2 = {fit['r2']:.4f}"
              f"   ({fit['n']} pts, T = {fit['t_lo']:.2f}-{fit['t_hi']:.2f},"
              f" band {band[0]}-{band[1]}){flag}")

    # The front's speed is quoted against Ritter's, which is constant. Whether
    # that comparison means anything depends on the measured slope settling
    # down, so report the profile and let the reader see whether it does.
    slopes = []
    step = 0.25
    centre = T[0] + SLOPE_HALF_WINDOW
    while centre <= T[-1] - SLOPE_HALF_WINDOW:
        s = local_slope(T, Z, centre)
        if s is not None:
            slopes.append((centre, s))
        centre += step

    if slopes:
        first_t, first_s = slopes[0]
        peak_t, peak_s = max(slopes, key=lambda p: p[1])
        print(f"    dZ/dT       {100 * first_s / RITTER_SLOPE:.0f}% of Ritter at T={first_t:.2f}"
              f"  ->  peak {100 * peak_s / RITTER_SLOPE:.0f}% at T={peak_t:.2f}"
              f"   (Ritter slope = {RITTER_SLOPE:.3f})")
        if show_slopes:
            for t, s in slopes:
                bar = "#" * int(round(40 * s / RITTER_SLOPE))
                print(f"      T={t:5.2f}  {s:6.3f}  {100 * s / RITTER_SLOPE:3.0f}% |{bar}")


def main(argv: list[str]) -> int:
    show_slopes = "--slopes" in argv

    # The exponent is the one number here that moves with its window, which is
    # exactly why it is worth being able to move it: a claim quoted from this
    # script should name the band it was fitted over.
    band = (ONSET_DELTA, FIT_BAND_HI)
    for arg in argv:
        if arg.startswith("--fit-band="):
            try:
                lo, hi = (float(v) for v in arg.split("=", 1)[1].split(":"))
            except ValueError:
                sys.exit("--fit-band wants LO:HI in column widths, e.g. --fit-band=0.05:0.5")
            if not 0.0 < lo < hi:
                sys.exit("--fit-band wants 0 < LO < HI")
            band = (lo, hi)

    names = [a for a in argv if not a.startswith("--")]
    paths = [Path(n) for n in names] if names else sorted(HERE.glob("dam_break*.csv"))

    missing = [p for p in paths if not p.exists()]
    if missing:
        sys.exit("no such file: " + ", ".join(str(p) for p in missing))
    if not paths:
        sys.exit(f"no dam_break*.csv found in {HERE}")

    print(f"Realised aspect n = {ASPECT}, Ritter slope sqrt(2n) = {RITTER_SLOPE:.4f}")
    for path in paths:
        report(read_run(path), show_slopes, band)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
