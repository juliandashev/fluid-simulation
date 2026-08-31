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
