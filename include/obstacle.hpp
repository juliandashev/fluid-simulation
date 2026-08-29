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

// Convex outline with any number of vertices, for shapes a quad cannot express.
// Filled as one region, so shared edges cannot produce duplicate particles the
// way a fan of quads would.
struct Poly {
    std::vector<glm::vec2> p;
};

inline Quad box(glm::vec2 lo, glm::vec2 hi) {
    return {{lo, glm::vec2(hi.x, lo.y), hi, glm::vec2(lo.x, hi.y)}};
}

// Winding-agnostic: inside means every edge sees the point on the same side.
// Compared as a perpendicular distance rather than a raw cross product, so the
// tolerance is in world units - a lattice point landing exactly on an edge comes
// out as +-1e-6 rather than 0, and a strict test would drop the whole edge row.
inline bool contains(const Quad& q, glm::vec2 point, const Resolution& r) {
    const float_t TOL = 1e-3f * r.spacing;
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

// Same side test as contains(Quad), over an arbitrary vertex count.
inline bool contains(const Poly& poly, glm::vec2 point, const Resolution& r) {
    const float_t TOL = 1e-3f * r.spacing;
    const std::size_t n = poly.p.size();
    float_t sign = 0.0f;

    for (std::size_t i = 0; i < n; ++i) {
        const glm::vec2 edge = poly.p[(i + 1) % n] - poly.p[i];
        const glm::vec2 rel = point - poly.p[i];
        const float_t len = std::sqrt(edge.x * edge.x + edge.y * edge.y);

        if (len <= 0.0f) {
            continue;
        }

        const float_t dist = (edge.x * rel.y - edge.y * rel.x) / len;

        if (std::abs(dist) <= TOL) {
            continue;
        }
        if (sign != 0.0f && (dist > 0.0f) != (sign > 0.0f)) {
            return false;
        }
        sign = dist;
    }

    return true;
}

inline std::vector<glm::vec2> to_particles(const Poly& poly, const Resolution& r) {
    std::vector<glm::vec2> out;
    if (poly.p.empty()) {
        return out;
    }

    glm::vec2 lo = poly.p[0];
    glm::vec2 hi = poly.p[0];
    for (const glm::vec2& c : poly.p) {
        lo = glm::min(lo, c);
        hi = glm::max(hi, c);
    }

    const glm::ivec2 n = glm::ivec2((hi - lo) / r.spacing + 1e-3f) + 1;

    for (int32_t j = 0; j < n.y; ++j) {
        for (int32_t i = 0; i < n.x; ++i) {
            const glm::vec2 p = lo + r.spacing * glm::vec2(i, j);
            if (contains(poly, p, r)) {
                out.push_back(p);
            }
        }
    }

    return out;
}

// A fan of degenerate quads, so the renderer can draw a Poly with no new path.
inline std::vector<Quad> to_quads(const Poly& poly) {
    std::vector<Quad> out;
    for (std::size_t i = 1; i + 1 < poly.p.size(); ++i) {
        out.push_back({{poly.p[0], poly.p[i], poly.p[i + 1], poly.p[i + 1]}});
    }
    return out;
}

// Solid particles sit on the fluid lattice, so a wall presents the same pitch
// the density summation is calibrated for.
inline std::vector<glm::vec2> to_particles(const std::vector<Quad>& quads,
                                          const Resolution& r) {
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
        const glm::ivec2 n = glm::ivec2((hi - lo) / r.spacing + 1e-3f) + 1;

        for (int32_t j = 0; j < n.y; ++j) {
            for (int32_t i = 0; i < n.x; ++i) {
                const glm::vec2 p = lo + r.spacing * glm::vec2(i, j);
                if (contains(q, p, r)) {
                    out.push_back(p);
                }
            }
        }
    }

    return out;
}

// Block on the bed downstream of the dam column (Koshizuka & Oka 1996).
inline std::vector<Quad> dam_break_block(const Resolution& r) {
    const float_t w = r.column_width / 4.0f;
    const float_t h = 0.75f * r.column_width;
    return {box(glm::vec2(-0.5f * w, DOMAIN_MIN), glm::vec2(0.5f * w, DOMAIN_MIN + h))};
}

// Periodic pipe: full-height walls spanning the wrap, pinched to a throat.
// The channel is deliberately smaller than the fluid's rest volume, so it runs
// pressurised instead of settling out to zero pressure against the EOS clamp.
constexpr float_t PIPE_HALF_HEIGHT = 13.0f;
// In kernel radii, not world units: a wall thinner than h lets fluid see
// through it, and h scales with the particle spacing.
// In kernel radii: a wall thinner than h lets fluid see through it.
constexpr float_t PIPE_WALL_IN_H = 2.0f;
constexpr float_t PIPE_RAMP_X0 = -30.0f;
constexpr float_t PIPE_THROAT_X0 = -10.0f;
constexpr float_t PIPE_THROAT_X1 = 10.0f;
constexpr float_t PIPE_RAMP_X1 = 30.0f;
constexpr float_t PIPE_THROAT_HALF = 6.5f;  // half of the channel, a 2:1 ratio

inline std::vector<Quad> pipe(const Resolution& r) {
    const float_t h = PIPE_HALF_HEIGHT;
    const float_t t = PIPE_THROAT_HALF;
    const float_t x = 0.5f * r.period_x;
    const float_t w = PIPE_WALL_IN_H * r.kernel_radius;

    return {
        box(glm::vec2(-x, -h - w), glm::vec2(x, -h)),
        box(glm::vec2(-x, h), glm::vec2(x, h + w)),
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

// Symmetric NACA 00xx section. The thickness is pinned to the kernel radius,
// not chosen freely: a wing thinner than h lets fluid see straight through it.
constexpr float_t WING_THICKNESS_RATIO = 0.18f;

// A fixed fraction of the domain, not a multiple of h: the wing keeps its
// physical size and simply gets better resolved as particles are added. Its
// thickness must still clear h, which wing() checks.
constexpr float_t WING_CHORD_FRAC = 0.45f;

inline Poly wing(float_t aoa_deg) {
    constexpr int32_t SAMPLES = 24;
    const float_t c = WING_CHORD_FRAC * (DOMAIN_MAX - DOMAIN_MIN);
    const float_t t = WING_THICKNESS_RATIO;
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

    Poly out;
    for (const glm::vec2& q : upper) {
        out.p.push_back(q);
    }
    for (std::size_t i = lower.size() - 1; i > 0; --i) {
        out.p.push_back(lower[i - 1]);
    }

    // Rotate about the quarter chord, then centre it in the channel.
    const glm::vec2 pivot(0.25f * c, 0.0f);
    for (glm::vec2& q : out.p) {
        const glm::vec2 r = q - pivot;
        q = glm::vec2(r.x * std::cos(a) + r.y * std::sin(a),
                      -r.x * std::sin(a) + r.y * std::cos(a));
    }

    return out;
}

inline float_t pipe_floor(float_t x) { return -pipe_half_gap(x); }
inline float_t pipe_ceiling(float_t x) { return pipe_half_gap(x); }

// Lays n particles on the finest lattice that clears the solids. Starts at the
// fluid pitch and tightens, because a pressurised channel packs tighter than dx.
inline std::vector<glm::vec2> fill_region(glm::vec2 lo, glm::vec2 hi,
                                          const std::vector<glm::vec2>& solids, uint32_t n,
                                          const Resolution& r) {
    const float_t margin_sq = 0.8f * r.spacing * 0.8f * r.spacing;

    for (float_t step = r.spacing; step > 0.5f * r.spacing; step *= 0.98f) {
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
