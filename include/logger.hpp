#pragma once

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "stats.hpp"

namespace fluid {
namespace log {

class Logger {
    std::ofstream out_;

public:
    // New columns append rather than insert: media/plots/*.plt address columns
    // by index.
    Logger(const std::string& path) : out_(path) {
        if (!out_.is_open()) {
            std::cerr << "Cannot open log file: " << path << "\n";
            std::exit(EXIT_FAILURE);
        }
        out_ << "step,sim_time,dt,max_speed,max_accel,"
                "rho_min,rho_med,rho_p90,rho_max,rho_sigma,"
                "a_min,a_med,a_p90,a_max\n";
    }

    void log(uint64_t step, double t, float dt, float v, float a, const DensityStats& rho,
             const AccelStats& acc) {
        out_ << step << ',' << t << ',' << dt << ',' << v << ',' << a << ','
             << rho.min << ',' << rho.median << ',' << rho.p90 << ',' << rho.max << ','
             << rho.sigma << ','
             << acc.min << ',' << acc.median << ',' << acc.p90 << ',' << acc.max << '\n';
    }
};

}  // namespace log
}  // namespace fluid
