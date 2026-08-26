#pragma once

#include <glm/common.hpp>

#include <cmath>
#include <glm/vec2.hpp>
#include <vector>

#include "defs.hpp"

namespace fluid {
namespace obstacle {

// Solid geometry is defined as convex quads: the solver fills them with
// particles, the renderer draws them, and neither has to fake the other.
struct Quad {
    glm::vec2 p[4];
};

inline Quad box(glm::vec2 lo, glm::vec2 hi) {
    return {{lo, glm::vec2(hi.x, lo.y), hi, glm::vec2(lo.x, hi.y)}};
}

// Winding-agnostic: inside means every edge sees the point on the same side.
// Compared as a perpendicular distance rather than a raw cross product, so the
// tolerance is in world units - a lattice point landing exactly on an edge comes
// out as +-1e-6 rather than 0, and a strict test would drop the whole edge row.
inline bool contains(const Quad& q, glm::vec2 point) {
    constexpr float_t TOL = 1e-3f * PARTICLE_SPACING;
    float_t sign = 0.0f;

    for (int32_t i = 0; i < 4; ++i) {
        const glm::vec2 edge = q.p[(i + 1) % 4] - q.p[i];
        const glm::vec2 rel = point - q.p[i];
        const float_t len = std::sqrt(edge.x * edge.x + edge.y * edge.y);

        if (len <= 0.0f) {
            continue;
        }

        const float_t dist = (edge.x * rel.y - edge.y * rel.x) / len;

        if (std::abs(dist) <= TOL) {
            continue;  // on the edge; either side is acceptable
        }
        if (sign != 0.0f && (dist > 0.0f) != (sign > 0.0f)) {
            return false;
        }
        sign = dist;
    }

    return true;
}

// Solid particles sit on the fluid lattice, so a wall presents the same pitch
// the density summation is calibrated for.
inline std::vector<glm::vec2> to_particles(const std::vector<Quad>& quads) {
    std::vector<glm::vec2> out;

    for (const Quad& q : quads) {
        glm::vec2 lo = q.p[0];
        glm::vec2 hi = q.p[0];
        for (const glm::vec2& corner : q.p) {
            lo = glm::min(lo, corner);
            hi = glm::max(hi, corner);
        }

        // Integer counts, not an accumulating float: drift there loses a whole
        // column and leaves the body off-centre. The epsilon is for the case the
        // division lands on a whole number a bit under, which truncates down.
        const glm::ivec2 n = glm::ivec2((hi - lo) / PARTICLE_SPACING + 1e-3f) + 1;

        for (int32_t j = 0; j < n.y; ++j) {
            for (int32_t i = 0; i < n.x; ++i) {
                const glm::vec2 p = lo + PARTICLE_SPACING * glm::vec2(i, j);
                if (contains(q, p)) {
                    out.push_back(p);
                }
            }
        }
    }

    return out;
}

// Block on the bed downstream of the dam column (Koshizuka & Oka 1996).
constexpr float_t BLOCK_WIDTH = COLUMN_WIDTH / 4.0f;
constexpr float_t BLOCK_HEIGHT = 0.75f * COLUMN_WIDTH;

inline std::vector<Quad> dam_break_block() {
    return {box(glm::vec2(-0.5f * BLOCK_WIDTH, DOMAIN_MIN),
                glm::vec2(0.5f * BLOCK_WIDTH, DOMAIN_MIN + BLOCK_HEIGHT))};
}

// Periodic pipe: full-height walls spanning the wrap, pinched to a throat.
// The channel is deliberately smaller than the fluid's rest volume, so it runs
// pressurised instead of settling out to zero pressure against the EOS clamp.
constexpr float_t PIPE_HALF_HEIGHT = 13.0f;
// In kernel radii, not world units: a wall thinner than h lets fluid see
// through it, and h scales with the particle spacing.
constexpr float_t PIPE_WALL = 2.0f * KERNEL_RADIUS;
constexpr float_t PIPE_RAMP_X0 = -30.0f;
constexpr float_t PIPE_THROAT_X0 = -10.0f;
constexpr float_t PIPE_THROAT_X1 = 10.0f;
constexpr float_t PIPE_RAMP_X1 = 30.0f;
constexpr float_t PIPE_THROAT_HALF = 6.5f;  // half of the channel, a 2:1 ratio

inline std::vector<Quad> pipe() {
    const float_t h = PIPE_HALF_HEIGHT;
    const float_t t = PIPE_THROAT_HALF;
    const float_t x = 0.5f * PERIOD_X;

    return {
        box(glm::vec2(-x, -h - PIPE_WALL), glm::vec2(x, -h)),
        box(glm::vec2(-x, h), glm::vec2(x, h + PIPE_WALL)),
        {{glm::vec2(PIPE_RAMP_X0, -h), glm::vec2(PIPE_RAMP_X1, -h),
          glm::vec2(PIPE_THROAT_X1, -t), glm::vec2(PIPE_THROAT_X0, -t)}},
        {{glm::vec2(PIPE_RAMP_X0, h), glm::vec2(PIPE_THROAT_X0, t),
          glm::vec2(PIPE_THROAT_X1, t), glm::vec2(PIPE_RAMP_X1, h)}},
    };
}

inline float_t pipe_half_gap(float_t x) {
    if (x <= PIPE_RAMP_X0 || x >= PIPE_RAMP_X1) {
        return PIPE_HALF_HEIGHT;
    }
    if (x >= PIPE_THROAT_X0 && x <= PIPE_THROAT_X1) {
        return PIPE_THROAT_HALF;
    }

    const bool narrowing = x < PIPE_THROAT_X0;
    const float_t x0 = narrowing ? PIPE_RAMP_X0 : PIPE_RAMP_X1;
    const float_t x1 = narrowing ? PIPE_THROAT_X0 : PIPE_THROAT_X1;

    return glm::mix(PIPE_HALF_HEIGHT, PIPE_THROAT_HALF, (x - x0) / (x1 - x0));
}

inline float_t pipe_floor(float_t x) { return -pipe_half_gap(x); }
inline float_t pipe_ceiling(float_t x) { return pipe_half_gap(x); }

// Lays n particles on the finest lattice that clears the solids. Starts at the
// fluid pitch and tightens, because a pressurised channel packs tighter than dx.
inline std::vector<glm::vec2> fill_region(glm::vec2 lo, glm::vec2 hi,
                                          const std::vector<glm::vec2>& solids, uint32_t n) {
    const float_t margin_sq = 0.8f * PARTICLE_SPACING * 0.8f * PARTICLE_SPACING;

    for (float_t step = PARTICLE_SPACING; step > 0.5f * PARTICLE_SPACING; step *= 0.98f) {
        std::vector<glm::vec2> out;
        const glm::ivec2 c = glm::ivec2((hi - lo) / step) + 1;

        for (int32_t j = 0; j < c.y; ++j) {
            for (int32_t i = 0; i < c.x; ++i) {
                const glm::vec2 p = lo + step * glm::vec2(i, j);

                bool blocked = false;
                for (const glm::vec2& q : solids) {
                    const glm::vec2 d = q - p;
                    if (d.x * d.x + d.y * d.y < margin_sq) {
                        blocked = true;
                        break;
                    }
                }

                if (!blocked) {
                    out.push_back(p);
                }
            }
        }

        if (out.size() >= n) {
            out.resize(n);
            return out;
        }
    }

    return {};
}

}  // namespace obstacle
}  // namespace fluid
