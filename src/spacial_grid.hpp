#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <vector>

struct Entry {
    int32_t particle_index;
    uint32_t cell_key;
};

class SpacialGrid {
public:
    void update_spacial_lookup(const std::vector<glm::vec2>& points, float_t radius);

    /// @brief Invokes `callback` for every stored point within the search radius of `sample_point`.
    /// @note Walks the 3x3 block of cells around the sample and filters by squared distance,
    ///       so hash collisions and corner-cell points are excluded. Uses the radius set by
    ///       update_spacial_lookup -> call that first, every frame, before querying.
    /// @param sample_point World-space location to search around.
    /// @param callback Run once per in-range point, given that point's index. `return` skips to
    ///        the next point (acts as continue); there is no way to break out of the search early.
    template <typename Callback>
    void foreach_point_within_radius(glm::vec2 sample_point, Callback&& callback) const {
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
                glm::vec2 offset = points_[particle_index] - sample_point;
                float_t dst_sq = glm::dot(offset, offset);

                if (dst_sq <= r_sq) {  // filters hash collisions + corner cells
                    callback(particle_index, offset, dst_sq);
                }
            }
        }
    }

private:
    glm::ivec2 position_to_cell_coord(glm::vec2 point, float_t radius) const;
    uint32_t hash_cell(int32_t cell_x, int32_t cell_y) const;
    uint32_t get_key_from_hash(uint32_t hash) const;

    std::vector<Entry> spacial_lookup_;   // one per particle, sorted by cell_key
    std::vector<int32_t> start_indices_;  // start_indices_[key] = first lookup idx with that key
    std::vector<glm::vec2> points_;
    float_t radius_;

    static constexpr std::array<glm::ivec2, 9> CELL_OFFSETS = {
        {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
};
