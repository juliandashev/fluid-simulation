#pragma once

#include <cmath>
#include <glm/vec2.hpp>
#include <vector>

#include "defs.hpp"
#include "obstacle.hpp"

namespace fluid {
namespace wing {

constexpr float_t THICKNESS_RATIO = 0.18f;  // symmetric NACA 00xx

// A fixed fraction of the domain, not a multiple of h: the section keeps its
// physical size and simply gets better resolved as particles are added.
constexpr float_t CHORD_FRAC = 0.45f;

inline float_t chord() { return CHORD_FRAC * (DOMAIN_MAX - DOMAIN_MIN); }

// Must clear the kernel radius, or the gas sees straight through the section.
inline float_t thickness() { return THICKNESS_RATIO * chord(); }

inline obstacle::Poly section(float_t aoa_deg) {
    constexpr int32_t SAMPLES = 24;
    const float_t c = chord();
    const float_t t = THICKNESS_RATIO;
    const float_t a = aoa_deg * static_cast<float_t>(M_PI) / 180.0f;

    // NACA 00xx half-thickness, closed at the trailing edge.
    auto half = [&](float_t u) {
        return 5.0f * t * c *
               (0.2969f * std::sqrt(u) - 0.1260f * u - 0.3516f * u * u +
                0.2843f * u * u * u - 0.1036f * u * u * u * u);
    };

    // Cosine spacing puts more points where the leading edge curves hardest.
    std::vector<glm::vec2> upper, lower;
    for (int32_t i = 0; i <= SAMPLES; ++i) {
        const float_t beta = static_cast<float_t>(M_PI) * i / SAMPLES;
        const float_t u = 0.5f * (1.0f - std::cos(beta));
        upper.push_back(glm::vec2(u * c, half(u)));
        lower.push_back(glm::vec2(u * c, -half(u)));
    }

    obstacle::Poly out;
    for (const glm::vec2& q : upper) {
        out.p.push_back(q);
    }
    for (std::size_t i = lower.size() - 1; i > 0; --i) {
        out.p.push_back(lower[i - 1]);
    }

    // Rotate about the quarter chord, which leaves it centred in the domain.
    const glm::vec2 pivot(0.25f * c, 0.0f);
    for (glm::vec2& q : out.p) {
        const glm::vec2 d = q - pivot;
        q = glm::vec2(d.x * std::cos(a) + d.y * std::sin(a),
                      -d.x * std::sin(a) + d.y * std::cos(a));
    }

    return out;
}

}  // namespace wing
}  // namespace fluid
