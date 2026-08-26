#include "experiment.hpp"

#include <cstring>
#include <iostream>

namespace fluid {

namespace {

constexpr char INLINE_FLAG[] = "--experiment=";
constexpr std::size_t INLINE_FLAG_LEN = sizeof(INLINE_FLAG) - 1;

ArgsResult reject(const char* argv0, const char* what, const char* detail) {
    std::cerr << what << ": " << detail << "\n\n";
    print_usage(argv0);
    return ArgsResult::Error;
}

}  // namespace

const ExperimentSpec& spec_of(Experiment which) {
    for (const ExperimentSpec& spec : EXPERIMENTS) {
        if (spec.id == which) {
            return spec;
        }
    }
    return EXPERIMENTS[0];
}

bool lookup_experiment(const char* name, Experiment& out) {
    for (const ExperimentSpec& spec : EXPERIMENTS) {
        if (std::strcmp(spec.name, name) == 0) {
            out = spec.id;
            return true;
        }
    }
    return false;
}

// The pipe runs horizontally under a body force, so gravity would only make it
// pool against one wall.
void configure(Experiment which, SimParams& params) {
    if (which == Experiment::Pipe) {
        params.gravity = 0.0f;
        params.body_accel_x = 1.5f;  // 0.6 is the Mach-0.1 measurement setting; see README
        params.cohesion_strength = 0.0f;
        params.draw_scale = 0.55f;  // separated dots; the field reads as colour, not a smear
    }
}

void print_usage(const char* argv0) {
    std::cout << "usage: " << argv0 << " [--experiment <name>]\n\nexperiments:\n";
    for (const ExperimentSpec& spec : EXPERIMENTS) {
        std::cout << "  " << spec.name << "\n      " << spec.summary << "\n";
    }
}

// Unknown arguments are rejected rather than ignored: a typo'd flag that
// silently runs the default would only show up in the wrong log file.
ArgsResult parse_args(int32_t argc, char** argv, Experiment& out) {
    for (int32_t i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            return ArgsResult::Exit;
        }

        const char* name = nullptr;

        if (std::strncmp(arg, INLINE_FLAG, INLINE_FLAG_LEN) == 0) {
            name = arg + INLINE_FLAG_LEN;
        } else if (std::strcmp(arg, "--experiment") == 0 || std::strcmp(arg, "-e") == 0) {
            if (i + 1 >= argc) {
                return reject(argv[0], "missing experiment name after", arg);
            }
            name = argv[++i];
        } else if (arg[0] != '-') {
            name = arg;  // bare name, so `fluid_simulation dam-break` also works
        } else {
            return reject(argv[0], "unknown option", arg);
        }

        if (!lookup_experiment(name, out)) {
            return reject(argv[0], "unknown experiment", name);
        }
    }

    return ArgsResult::Ok;
}

}  // namespace fluid
