#pragma once

#include <glm/vec2.hpp>
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <functional>

struct Entry {
    int32_t particle_index;
    uint32_t cell_key;
};

class SpacialGrid {
public:
    void update_spacial_lookup(std::vector<glm::vec2> points, float_t radius);

    void foreach_point_within_radius(glm::vec2 sample_point,
                                     const std::function<void(int32_t)>& callback);

private:
    glm::ivec2 position_to_cell_coord(glm::vec2 point, float_t radius);
    uint32_t hash_cell(int32_t cell_x, int32_t cell_y);
    uint32_t get_key_from_hash(uint32_t hash);

    std::vector<Entry> spacial_lookup_;   // one per particle, sorted by cell_key
    std::vector<int32_t> start_indices_;  // start_indices_[key] = first lookup idx with that key
    std::vector<glm::vec2> points_;
    float_t radius_;

    static constexpr std::array<glm::ivec2, 9> CELL_OFFSETS = {
        {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
};
