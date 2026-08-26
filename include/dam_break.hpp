#pragma once

#include <glm/vec2.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

#include "defs.hpp"

namespace fluid {
namespace dam_break {

// Dam break instrumentation (Martin & Moyce 1952). Z = (x_front - x_0)/a,
// T = t*sqrt(2g/a). T scalings differ between papers - see README.

// Surge front position (world x), gated to the bed and de-spiked by quantile.
// Reports particle-centre x, so a t=0 column reads Z = 0.978, not 1.0.
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

inline float_t Z(float_t front_x, float_t origin_x, float_t a) {
    return a > 0.0f ? (front_x - origin_x) / a : 0.0f;
}

inline float_t T(float_t t, float_t a, float_t gravity) {
    const float_t g = std::abs(gravity);
    return a > 0.0f ? t * std::sqrt(2.0f * g / a) : 0.0f;
}

// e.g. dam_break_coh50_visc3.14_ten20.csv; takes the next free _N suffix rather
// than overwriting an existing run.
inline std::string filename(const SimParams& params) {
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

// Front trace, raw and dimensionless. Opened lazily on the first sample, so a
// run that logs nothing leaves no file behind.
class Logger {
    std::ofstream out_;
    std::string pending_;

    void ensure_open() {
        if (!out_.is_open()) {
            out_.open(pending_, std::ios::trunc);
            if (!out_.is_open()) {
                std::cerr << "Cannot open dam-break log: " << pending_ << "\n";
                std::exit(EXIT_FAILURE);
            }
            out_ << "sim_time,front_x,Z,T,cohesion,viscosity,tension,pressure\n";
        }
    }

public:
    explicit Logger(std::string path) : pending_(std::move(path)) {}

    // Re-arm on a fresh spawn, so one session can produce a whole sweep.
    void restart(std::string path) {
        out_.close();
        out_.clear();
        pending_ = std::move(path);
    }

    // Parameters are logged per sample, not just encoded in the filename, so a
    // mid-run slider change shows up as a step in the data.
    void log(float_t t, float_t front_x, float_t origin_x, float_t a, const SimParams& params) {
        ensure_open();
        out_ << t << ',' << front_x << ',' << Z(front_x, origin_x, a) << ','
             << T(t, a, params.gravity) << ',' << params.cohesion_strength << ','
             << params.viscosity << ',' << params.tension_strength << ','
             << params.pressure_multiplier << '\n';
        out_.flush();  // ~30 lines/s; a kill mid-run keeps what was collected
    }
};

}  // namespace dam_break
}  // namespace fluid
