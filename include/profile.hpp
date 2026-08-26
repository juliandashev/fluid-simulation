#pragma once

#include <glm/vec2.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "defs.hpp"
#include "eos.hpp"
#include "obstacle.hpp"

namespace fluid {
namespace profile {

// Duct profiling for the scenes that have one. Bernoulli says
// p + rho*v^2/2 + rho*g*y holds along a streamline, so a throat buys speed with
// pressure; binning by x is what turns that into a curve.
constexpr float_t BIN_WIDTH = 4.0f;
// The duct being profiled: its floor and ceiling at a given x. That fixes both
// the area ratio and the mid-height streamline to sample along.
struct Duct {
    float_t (*floor_at)(float_t);
    float_t (*ceiling_at)(float_t);
    float_t x0;
    float_t x1;
};

inline Duct pipe_duct() {
    return {obstacle::pipe_floor, obstacle::pipe_ceiling, -0.5f * PERIOD_X, 0.5f * PERIOD_X};
}

// Bernoulli is a statement about one streamline. A cross-section average mixes
// core flow with the free surface, so the core band is logged alongside it.
constexpr float_t CORE_HALF_BAND = 5.0f;

// Named after the scene, so two ducts cannot land in the same file.
inline std::string filename(const char* scene, const SimParams& params) {
    char base[128];
    std::snprintf(base, sizeof(base), "%s_visc%g_k%g", scene,
                  static_cast<double>(params.viscosity),
                  static_cast<double>(params.pressure_multiplier));

    std::string path = std::string(base) + ".csv";
    for (int32_t i = 2; std::filesystem::exists(path) && std::filesystem::file_size(path) > 0;
         ++i) {
        path = std::string(base) + "_" + std::to_string(i) + ".csv";
    }
    return path;
}

// One row per bin per sample, which is many times the volume of a scalar
// trace, so it rides a slower cadence than the other loggers.
class Logger {
    Duct duct_;
    int32_t bins_;
    std::ofstream out_;
    std::string pending_;
    uint32_t sample_ = 0;

    float_t bin_x(int32_t b) const { return duct_.x0 + (b + 0.5f) * BIN_WIDTH; }
    float_t gap_height(float_t x) const { return duct_.ceiling_at(x) - duct_.floor_at(x); }
    float_t duct_mid(float_t x) const {
        return 0.5f * (duct_.ceiling_at(x) + duct_.floor_at(x));
    }

    void ensure_open() {
        if (!out_.is_open()) {
            out_.open(pending_, std::ios::trunc);
            if (!out_.is_open()) {
                std::cerr << "Cannot open profile log: " << pending_ << "\n";
                std::exit(EXIT_FAILURE);
            }
            out_ << "sim_time,band,x,gap,count,speed,pressure,y,bernoulli\n";
        }
    }

public:
    static constexpr uint32_t EVERY = 8;  // samples between profiles

    Logger(std::string path, Duct duct)
        : duct_(duct),
          bins_(static_cast<int32_t>((duct.x1 - duct.x0) / BIN_WIDTH)),
          pending_(std::move(path)) {}

    void restart(std::string path) {
        out_.close();
        out_.clear();
        pending_ = std::move(path);
        sample_ = 0;
    }

    void log(float_t t, const std::vector<glm::vec2>& positions,
             const std::vector<float_t>& speeds, const std::vector<float_t>& densities,
             const SimParams& params) {
        if (sample_++ % EVERY != 0) {
            return;
        }

        ensure_open();

        const float_t g = std::abs(params.gravity);

        for (int32_t pass = 0; pass < 2; ++pass) {
            const bool core = pass == 1;

            std::vector<int32_t> count(bins_, 0);
            std::vector<double_t> sum_speed(bins_, 0.0);
            std::vector<double_t> sum_pressure(bins_, 0.0);
            std::vector<double_t> sum_y(bins_, 0.0);
            std::vector<double_t> sum_bernoulli(bins_, 0.0);

            for (std::size_t i = 0; i < positions.size(); ++i) {
                if (core && std::abs(positions[i].y - duct_mid(positions[i].x)) >
                                CORE_HALF_BAND) {
                    continue;
                }

                const int32_t b = static_cast<int32_t>((positions[i].x - duct_.x0) / BIN_WIDTH);
                if (b < 0 || b >= bins_) {
                    continue;
                }

                const float_t rho = densities[i];
                const float_t v = speeds[i];
                const float_t p = pressure_at(params, rho);

                ++count[b];
                sum_speed[b] += v;
                sum_pressure[b] += p;
                sum_y[b] += positions[i].y;
                sum_bernoulli[b] += p + 0.5f * rho * v * v + rho * g * positions[i].y;
            }

            for (int32_t b = 0; b < bins_; ++b) {
                if (count[b] == 0) {
                    continue;  // an empty bin is absence of fluid, not a zero reading
                }

                const float_t x = bin_x(b);
                const double_t n = count[b];

                out_ << t << ',' << (core ? "core" : "all") << ',' << x << ','
                     << gap_height(x) << ',' << count[b] << ',' << sum_speed[b] / n << ','
                     << sum_pressure[b] / n << ',' << sum_y[b] / n << ','
                     << sum_bernoulli[b] / n << '\n';
            }
        }

        out_.flush();
    }
};

}  // namespace profile
}  // namespace fluid
