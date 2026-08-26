#pragma once

#include <cstdint>

#include "defs.hpp"

namespace fluid {

// Scene selected on the command line; Sandbox is what runs with no arguments.
enum class Experiment : int32_t {
    Sandbox,
    DamBreak,
    DamBreakObstacle,
    Pipe,
};

struct ExperimentSpec {
    const char* name;
    Experiment id;
    bool starts_paused;  // a benchmark names its log at spawn, so it waits for R
    bool logs_front;     // off where an obstacle makes the surge front meaningless
    bool logs_profile;   // x-binned speed/pressure, for the duct scenes
    const char* summary;
};

inline constexpr ExperimentSpec EXPERIMENTS[] = {
    {"sandbox", Experiment::Sandbox, false, false, false,
     "centered block, interactive (default)"},
    {"dam-break", Experiment::DamBreak, true, true, false,
     "Martin & Moyce column; logs the surge front"},
    {"dam-break-obstacle", Experiment::DamBreakObstacle, true, false, false,
     "the same column against a block on the bed (Koshizuka & Oka)"},
    {"pipe", Experiment::Pipe, false, false, true,
     "periodic pressurised pipe with a throat; steady-flow Bernoulli"},
};

// Ok: run. Exit: --help was served. Error: usage already reported.
enum class ArgsResult : int32_t { Ok, Exit, Error };

const ExperimentSpec& spec_of(Experiment which);

// Per-scene overrides of the panel defaults, applied once at startup.
void configure(Experiment which, SimParams& params);

// False on an unknown name; leaves out untouched.
bool lookup_experiment(const char* name, Experiment& out);

void print_usage(const char* argv0);

ArgsResult parse_args(int32_t argc, char** argv, Experiment& out);

}  // namespace fluid
