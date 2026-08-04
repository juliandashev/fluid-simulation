#pragma once

#include <cstdint>
#include <fstream>
#include <string>

class Logger {
    std::ofstream out_;

public:
    Logger(const std::string& path) : out_(path) {
        out_ << "step,sim_time,dt,max_speed,max_accel\n";
    }

    void log(uint64_t step, double t, float dt, float v, float a) {
        out_ << step << ',' << t << ',' << dt << ',' << v << ',' << a << '\n';
    }
};
