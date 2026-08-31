#pragma once

#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <vector>

#include "defs.hpp"
#include "obstacle.hpp"
#include "profile.hpp"

namespace fluid {
namespace pipe {

// Periodic pipe: full-height walls spanning the wrap, pinched to a throat. The
// channel is deliberately smaller than the fluid's rest volume, so it runs
// pressurised instead of settling out to zero pressure against the EOS clamp.
constexpr float_t HALF_HEIGHT = 13.0f;
constexpr float_t THROAT_HALF = 6.5f;  // half of the channel, a 2:1 ratio
constexpr float_t RAMP_X0 = -30.0f;
constexpr float_t THROAT_X0 = -10.0f;
constexpr float_t THROAT_X1 = 10.0f;
constexpr float_t RAMP_X1 = 30.0f;

// In kernel radii, not world units: a wall thinner than h lets fluid see
// through it, and h scales with the particle spacing.
constexpr float_t WALL_IN_H = 2.0f;

inline std::vector<obstacle::Quad> geometry(const Resolution& r) {
    const float_t h = HALF_HEIGHT;
    const float_t t = THROAT_HALF;
    const float_t x = 0.5f * r.period_x;
    const float_t w = WALL_IN_H * r.kernel_radius;

    return {
        obstacle::box(glm::vec2(-x, -h - w), glm::vec2(x, -h)),
        obstacle::box(glm::vec2(-x, h), glm::vec2(x, h + w)),
        {{glm::vec2(RAMP_X0, -h), glm::vec2(RAMP_X1, -h), glm::vec2(THROAT_X1, -t),
          glm::vec2(THROAT_X0, -t)}},
        {{glm::vec2(RAMP_X0, h), glm::vec2(THROAT_X0, t), glm::vec2(THROAT_X1, t),
          glm::vec2(RAMP_X1, h)}},
    };
}

inline float_t half_gap(float_t x) {
    if (x <= RAMP_X0 || x >= RAMP_X1) {
        return HALF_HEIGHT;
    }
    if (x >= THROAT_X0 && x <= THROAT_X1) {
        return THROAT_HALF;
    }

    const bool narrowing = x < THROAT_X0;
    const float_t x0 = narrowing ? RAMP_X0 : RAMP_X1;
    const float_t x1 = narrowing ? THROAT_X0 : THROAT_X1;

    return glm::mix(HALF_HEIGHT, THROAT_HALF, (x - x0) / (x1 - x0));
}

inline float_t floor_at(float_t x) { return -half_gap(x); }
inline float_t ceiling_at(float_t x) { return half_gap(x); }

inline profile::Duct duct(const Resolution& r) {
    return {floor_at, ceiling_at, -0.5f * r.period_x, 0.5f * r.period_x};
}

}  // namespace pipe
}  // namespace fluid
