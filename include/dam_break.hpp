#pragma once

#include <glm/vec2.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

#include "defs.hpp"

// Dam break benchmark instrumentation.
//
// Martin & Moyce (1952) released a column of liquid of width a and height n*a
// against a rigid wall and tracked the surge front along the floor. The result
// is reported dimensionlessly so it is independent of the tank size:
//
//     Z = (x_front - x_0) / a        how far the front has run, in column widths
//     T = t * sqrt(2 g / a)          time, scaled by the column's free-fall time
//
// Different papers use slightly different scalings for T (some drop the 2, some
// fold in the aspect ratio n), so match whichever dataset you plot against
// rather than assuming these agree.

// Reduce the particle cloud to a single surge front position (world x).
//
// Martin & Moyce tracked the leading edge *along the floor*, so two filters
// apply, each catching a kind of outlier the other misses:
//
//   1. Height gate - only particles within one kernel radius of the floor
//      count. A droplet flying at mid-height is not part of the bed flow by
//      the experiment's definition, and h is the distance over which particles
//      physically interact, so the gate is tied to the solver rather than to a
//      tuned number. This rejects spray by physics.
//   2. Quantile - of what survives the gate, discard the leading 1%, so a lone
//      particle skating ahead of the bulk cannot move the reported front
//      discontinuously. This rejects stragglers by statistics.
//
// A genuine surge is not rejected: 200 particles arriving together at the same
// x move the front, one particle there does not.
//
// Note the quantile is taken over the *gated* set, not all particles, so the
// filter grows more selective by itself as fluid accumulates on the floor.
//
// Reports particle-centre x. A t=0 column therefore gives Z = 0.978 rather
// than exactly 1.0: the outer face of the fluid sits half a lattice spacing
// beyond the last particle centre. Systematic, ~2%, and stated here rather
// than corrected so the measurement stays a raw observable.
//
// `positions` is every particle, unsorted, in world coordinates; the floor is
// at DOMAIN_MIN. `fallback` is returned when nothing is near the floor -
// unreachable for a column that spawns standing on it, but it keeps Z
// well-defined for any other initial condition. Pass the column origin so the
// degenerate case reads as Z = 0.
inline float_t surge_front(const std::vector<glm::vec2>& positions,
                           float_t fallback = -DOMAIN_HALF_X) {
    constexpr float_t FRONT_BAND = KERNEL_RADIUS;  // "along the bed" = within h of the floor
    constexpr float_t FRONT_QUANTILE = 0.01f;      // leading fraction discarded as spray

    std::vector<float_t> xs;
    xs.reserve(positions.size());

    const float_t bed = DOMAIN_MIN + FRONT_BAND;
    for (const glm::vec2& p : positions) {
        if (p.y <= bed) {
            xs.push_back(p.x);
        }
    }

    if (xs.empty()) {
        return fallback;  // nothing on the bed yet
    }

    const std::size_t k = static_cast<std::size_t>(FRONT_QUANTILE * xs.size());
    std::nth_element(xs.begin(), xs.begin() + k, xs.end(), std::greater<float_t>());
    return xs[k];
}

inline float_t dam_break_Z(float_t front_x, float_t origin_x, float_t a) {
    return a > 0.0f ? (front_x - origin_x) / a : 0.0f;
}

inline float_t dam_break_T(float_t t, float_t a, float_t gravity) {
    const float_t g = std::abs(gravity);
    return a > 0.0f ? t * std::sqrt(2.0f * g / a) : 0.0f;
}

// Output name for a parameter sweep: each combination gets its own file, e.g.
// dam_break_coh50_visc3.14_ten20.csv. If that name already holds a run, the
// next free suffix is used (..._2.csv) - measurement data is expensive to
// produce and must never be silently overwritten by a later run.
//
// All three force parameters appear, not just the one being swept. Naming only
// the swept parameter was enough while cohesion was the only thing that moved,
// but it makes every other sweep collide: a viscosity sweep at fixed cohesion
// writes the same name four times and lands in _2, _3, _4, where the filename
// no longer says which run is which. Carrying all three costs a longer name and
// buys a name that stays true whichever parameter the next study varies.
inline std::string dam_break_filename(const SimParams& params) {
    char base[128];
    std::snprintf(base, sizeof(base), "dam_break_coh%g_visc%g_ten%g",
                  static_cast<double>(params.cohesion_strength),
                  static_cast<double>(params.viscosity),
                  static_cast<double>(params.tension_strength));

    std::string path = std::string(base) + ".csv";
    for (int32_t i = 2; std::filesystem::exists(path) && std::filesystem::file_size(path) > 0;
         ++i) {
        path = std::string(base) + "_" + std::to_string(i) + ".csv";
    }
    return path;
}

// Writes the raw and dimensionless front trace so the comparison script can
// plot Z against T without knowing the tank geometry.
//
// The file is opened lazily, on the first sample rather than at construction.
// A run that logs nothing therefore leaves no file behind - which matters
// because the panel starts at the default cohesion, so eagerly opening would
// create (and clobber) that value's file on every single launch, before the
// user has chosen anything.
class DamBreakLogger {
    std::ofstream out_;
    std::string pending_;

    void ensure_open() {
        if (!out_.is_open()) {
            out_.open(pending_, std::ios::trunc);
            out_ << "sim_time,front_x,Z,T,cohesion,viscosity,tension,pressure\n";
        }
    }

public:
    explicit DamBreakLogger(std::string path) : pending_(std::move(path)) {}

    // Re-arm on a fresh spawn so one session can produce a whole sweep: set a
    // cohesion in the panel, press R, let it run, repeat.
    void restart(std::string path) {
        out_.close();
        out_.clear();
        pending_ = std::move(path);
    }

    // The swept parameters are logged per sample, not just encoded in the file
    // name. A filename records the value at spawn time, so changing a slider
    // without respawning silently mislabels the whole run; a column cannot lie,
    // and a mid-run change shows up as a step in the data instead of vanishing.
    //
    // Tension is here because the cohesion sweep needed it and did not have it.
    // Cohesion and the Mueller color-field tension are separate additive terms
    // in force.comp, so a run at cohesion 0 still has tension pulling at
    // strength 20 - which makes "cohesion 0" a misleading label for what was
    // really "cohesion off, the other surface-tension term still on". Logging
    // it means a future reader can tell the two apart without having to know
    // what the panel happened to be set to.
    //
    // Pressure is logged but deliberately kept out of the filename. Every knob
    // someone might sweep cannot go in the name without it growing without
    // bound, and it does not need to: the name only has to be unique, which the
    // _2/_3 suffix already guarantees, while the columns carry what the run
    // actually was. front_metrics.py reports whatever parameter columns it
    // finds, so a k sweep is readable even when three of its files differ only
    // by that suffix.
    void log(float_t t, float_t front_x, float_t origin_x, float_t a, const SimParams& params) {
        ensure_open();
        out_ << t << ',' << front_x << ',' << dam_break_Z(front_x, origin_x, a) << ','
             << dam_break_T(t, a, params.gravity) << ',' << params.cohesion_strength << ','
             << params.viscosity << ',' << params.tension_strength << ','
             << params.pressure_multiplier << '\n';
        // Flush every sample: ~30 lines/s is nothing, and it means killing the
        // window (or a crash mid-run) still leaves the samples collected so far
        // instead of losing whatever sat in the stream buffer.
        out_.flush();
    }
};
