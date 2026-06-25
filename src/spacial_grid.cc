#include "spacial_grid.hpp"

#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <execution>

void SpacialGrid::update_spacial_lookup(std::vector<glm::vec2> points, float_t radius) {
    points_ = points;
    radius_ = radius;

    const size_t n = points_.size();
    spacial_lookup_.resize(n);

    // INT32_MAX = "empty cell" sentinel. The query's (i < size) guard means a
    // start index of INT32_MAX simply never enters the inner loop.
    start_indices_.assign(n, INT32_MAX);

    // Stamp each particle with the key of the cell it lands in.
    std::for_each(std::execution::par, spacial_lookup_.begin(), spacial_lookup_.end(),
                  [&](Entry& e) {
                      size_t i = &e - spacial_lookup_.data();
                      glm::ivec2 c = position_to_cell_coord(points_[i], radius_);
                      uint32_t key = get_key_from_hash(hash_cell(c.x, c.y));
                      e = Entry{static_cast<int32_t>(i), key};
                  });

    // Sort so every particle sharing a key sits in one contiguous run.
    std::sort(std::execution::par, spacial_lookup_.begin(), spacial_lookup_.end(),
              [](const Entry& a, const Entry& b) { return a.cell_key < b.cell_key; });

    // Record where each key's run begins. We write start_indices_ only
    // when the key differs from the previous entry -> that's the run boundary.
    std::for_each(std::execution::par, spacial_lookup_.begin(), spacial_lookup_.end(),
                  [&](const Entry& e) {
                      size_t i = &e - spacial_lookup_.data();
                      uint32_t key = e.cell_key;
                      uint32_t key_prev = (i == 0) ? UINT32_MAX : spacial_lookup_[i - 1].cell_key;

                      if (key != key_prev) {
                          start_indices_[key] = static_cast<int32_t>(i);
                      }
                  });
}

glm::ivec2 SpacialGrid::position_to_cell_coord(glm::vec2 point, float_t radius) {
    // floor (not truncation) so negative coordinates bin correctly:
    // (int)(-0.1) == 0 would merge the -1 and 0 cells; floor keeps them apart.
    int32_t cell_x = static_cast<int32_t>(std::floor(point.x / radius));
    int32_t cell_y = static_cast<int32_t>(std::floor(point.y / radius));
    return glm::ivec2(cell_x, cell_y);
}

uint32_t SpacialGrid::hash_cell(int32_t cell_x, int32_t cell_y) {
    // Two large primes scatter neighboring cells far apart in hash space.
    uint32_t a = static_cast<uint32_t>(cell_x) * 15823u;
    uint32_t b = static_cast<uint32_t>(cell_y) * 9737333u;
    return a + b;
}

uint32_t SpacialGrid::get_key_from_hash(uint32_t hash) {
    // Fold the unbounded hash into a table slot. Collisions are possible here;
    // the dst_sq <= r_sq test in the query is what filters them out.
    return hash % static_cast<uint32_t>(spacial_lookup_.size());
}

/// @brief Invokes `callback` for every stored point within the search radius of `sample_point`.
/// @note Walks the 3x3 block of cells around the sample and filters by squared distance,
///       so hash collisions and corner-cell points are excluded. Uses the radius set by
///       update_spacial_lookup -> call that first, every frame, before querying.
/// @param sample_point World-space location to search around.
/// @param callback Run once per in-range point, given that point's index. `return` skips to
///        the next point (acts as continue); there is no way to break out of the search early.
void SpacialGrid::foreach_point_within_radius(glm::vec2 sample_point,
                                              const std::function<void(int32_t)>& callback) {
    glm::ivec2 centre = position_to_cell_coord(sample_point, radius_);
    float_t r_sq = radius_ * radius_;

    for (const glm::ivec2& off : CELL_OFFSETS) {
        uint32_t key = get_key_from_hash(hash_cell(centre.x + off.x, centre.y + off.y));
        int32_t cell_start = start_indices_[key];

        for (size_t i = cell_start; i < spacial_lookup_.size(); i++) {
            // Sorted runs: once the key changes, we've walked off this bucket.
            if (spacial_lookup_[i].cell_key != key) {
                break;
            }
            int32_t particle_index = spacial_lookup_[i].particle_index;
            glm::vec2 d = points_[particle_index] - sample_point;
            if (glm::dot(d, d) <= r_sq) {  // filters hash collisions + corner cells
                callback(particle_index);
            }
        }
    }
}
