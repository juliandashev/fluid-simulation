#pragma once

#include <algorithm>
#include <cmath>
#include <glm/vec2.hpp>
#include <vector>

#include "defs.hpp"
#include "obstacle.hpp"

namespace fluid {
namespace wing {

constexpr float_t THICKNESS_RATIO = 0.18f;  // symmetric NACA 00xx

// Trailing-edge half-thickness floor, in kernel radii; the section is cut where
// it thins past this. 0 keeps the true sharp tip, at the cost of a stretch with
// no boundary particles in it. 0.4 gives a 3 dx barrier and cuts 21% of chord;
// 0.15 is ~1 lattice row and cuts under 10%, which still reads as a sharp tip.
constexpr float_t TE_MIN_HALF = 0.0f;

// The chord is a fraction of the domain, not a multiple of h: the section keeps
// its physical size and simply gets better resolved as particles are added.
// Live-tunable, because thickness/h decides whether the trailing edge exists.
inline float_t chord(float_t frac) { return frac * (DOMAIN_MAX - DOMAIN_MIN); }

// Must clear the kernel radius, or the gas sees straight through the section.
inline float_t thickness(float_t frac) { return THICKNESS_RATIO * chord(frac); }

inline obstacle::Poly section(float_t aoa_deg, float_t frac, float_t min_half) {
    constexpr int32_t SAMPLES = 24;
    const float_t c = chord(frac);
    const float_t t = THICKNESS_RATIO;
    const float_t a = aoa_deg * static_cast<float_t>(M_PI) / 180.0f;

    // NACA 00xx half-thickness.
    auto half = [&](float_t u) {
        return 5.0f * t * c *
               (0.2969f * std::sqrt(u) - 0.1260f * u - 0.3516f * u * u +
                0.2843f * u * u * u - 0.1036f * u * u * u * u);
    };

    // The true profile closes to zero at the trailing edge, which leaves a
    // stretch too thin to hold even one lattice row - a hole the fluid pours
    // through. min_half cuts the section where it thins to that, trading the
    // sharp tip for a real barrier; 0 keeps the full profile and the leak.
    // (Truncating rather than flooring the thickness because a floor puts a
    // concave kink where it meets the curve, and contains() is a convex test.)
    float_t u_max = 1.0f;
    if (min_half > 0.0f) {
        float_t lo = 0.3f;  // past the peak, where the profile is decreasing
        float_t hi = 1.0f;

        for (int32_t i = 0; i < 40; ++i) {
            const float_t mid = 0.5f * (lo + hi);
            (half(mid) >= min_half ? lo : hi) = mid;
        }

        u_max = lo;
    }

    // Cosine spacing puts more points where the leading edge curves hardest.
    std::vector<glm::vec2> upper, lower;
    for (int32_t i = 0; i <= SAMPLES; ++i) {
        const float_t beta = static_cast<float_t>(M_PI) * i / SAMPLES;
        const float_t u = u_max * 0.5f * (1.0f - std::cos(beta));
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
