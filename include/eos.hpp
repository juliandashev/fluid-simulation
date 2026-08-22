#pragma once

#include <cmath>
#include <cstdint>

#include "defs.hpp"

// Tait EOS exponent, p = k((rho/rho_0)^EOS_EXPONENT - 1). shader.cc injects it
// into the shaders as a #define so the two cannot drift apart.
constexpr int32_t EOS_EXPONENT = 4;

// c(rho) = c_0*(rho/rho_0)^1.5. Numerical, not physical.
inline float_t sound_speed_at(const SimParams& params, float_t density) {
    if (density <= 0.0f) {
        return 0.0f;
    }
    const float_t rho_0 = params.target_density;
    const float_t ratio = density / rho_0;
    return std::sqrt(EOS_EXPONENT * params.pressure_multiplier / rho_0) * ratio * std::sqrt(ratio);
}

inline float_t sound_speed(const SimParams& params) {
    return sound_speed_at(params, params.target_density);
}

// Flow speed as a fraction of c; ten to one is the usual rule.
inline float_t mach_number(float_t flow_speed, const SimParams& params) {
    const float_t c = sound_speed(params);
    return c > 0.0f ? flow_speed / c : 0.0f;
}
