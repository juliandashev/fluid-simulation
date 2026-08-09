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

ONSET_DELTA = 0.05  # column widths the front must clear to count as "moved"
WALL_TOL = 0.01  # column widths (~half a particle spacing) of slack at the wall
CROSSINGS = (1.5, 2.0, 3.0)
SLOPE_HALF_WINDOW = 0.15  # in T, each side of the centre point


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


def fmt(value: float | None, spec: str = "6.3f") -> str:
    return format(value, spec) if value is not None else "     -"


def report(run: dict, show_slopes: bool) -> None:
    T, Z = usable(run)
    name = run["path"].name

    print(f"\n=== {name}")
    if run["params"]:
        joined = ", ".join(f"{k}={v}" for k, v in run["params"].items())
        print(f"    logged parameters: {joined}")
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
    names = [a for a in argv if not a.startswith("--")]
    paths = [Path(n) for n in names] if names else sorted(HERE.glob("dam_break*.csv"))

    missing = [p for p in paths if not p.exists()]
    if missing:
        sys.exit("no such file: " + ", ".join(str(p) for p in missing))
    if not paths:
        sys.exit(f"no dam_break*.csv found in {HERE}")

    print(f"Realised aspect n = {ASPECT}, Ritter slope sqrt(2n) = {RITTER_SLOPE:.4f}")
    for path in paths:
        report(read_run(path), show_slopes)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
