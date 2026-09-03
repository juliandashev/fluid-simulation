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
    Wing,
};

struct ExperimentSpec {
    const char* name;
    Experiment id;
    bool starts_paused;  // a benchmark names its log at spawn, so it waits for R
    bool logs_front;     // off where an obstacle makes the surge front meaningless
    bool logs_profile;   // x-binned speed/pressure, for the duct scenes
    int32_t particles_across;  // 0 = the default resolution
    float_t column_width;      // 0 = the default; sets how much area the fluid fills
    const char* summary;
};

inline constexpr ExperimentSpec EXPERIMENTS[] = {
    {"sandbox", Experiment::Sandbox, false, false, false, 0, 0.0f,
     "centered block, interactive (default)"},
    {"dam-break", Experiment::DamBreak, true, true, false, 0, 0.0f,
     "Martin & Moyce column; logs the surge front"},
    {"dam-break-obstacle", Experiment::DamBreakObstacle, true, false, false, 0, 0.0f,
     "the same column against a block on the bed (Koshizuka & Oka)"},
    {"pipe", Experiment::Pipe, false, false, true, 0, 0.0f,
     "periodic pressurised pipe with a throat; steady-flow Bernoulli"},
    // 102, not 105: N*dx^2 has to match the domain area the gas actually gets,
    // or fill_region tightens the lattice to fit and spawns it over-dense. At
    // 105 the bigger chord pushed that to 13%, and the startup transient drove
    // particles through the wing's surface. See README.
    {"wing", Experiment::Wing, false, false, false, 0, 102.0f,
     "aerofoil in a screen-filling gas; S/P switch the colour field"},
};

// Ok: run. Exit: --help was served. Error: usage already reported.
enum class ArgsResult : int32_t { Ok, Exit, Error };

const ExperimentSpec& spec_of(Experiment which);

// Per-scene overrides of the panel defaults, applied once at startup.
void configure(Experiment which, SimParams& params);

// The scene's own resolution, with an optional --particles override.
Resolution resolution_of(const ExperimentSpec& scene, int32_t override_n);

// False on an unknown name; leaves out untouched.
bool lookup_experiment(const char* name, Experiment& out);

void print_usage(const char* argv0);

// particles_out is left alone unless --particles is given.
ArgsResult parse_args(int32_t argc, char** argv, Experiment& out, int32_t& particles_out);

}  // namespace fluid
