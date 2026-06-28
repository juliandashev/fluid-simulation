#pragma once

#include <glm/vec2.hpp>
#include <vector>

#include "particle.hpp"

class History {
public:
    History(size_t max_snapshots, size_t particle_count)
        : ring_(max_snapshots), max_(max_snapshots) {
        for (Snapshot& s : ring_) {
            s.positions.resize(particle_count);
            s.velocities.resize(particle_count);
        }
    }

    void save(const std::vector<Particle>& particles) {
        Snapshot& s = ring_[head_];
        for (size_t i = 0; i < particles.size(); ++i) {
            s.positions[i] = particles[i].position;
            s.velocities[i] = particles[i].velocity;
        }
        head_ = (head_ + 1) % max_;
        if (count_ < max_) count_++;
    }

    void restore(std::vector<Particle>& particles) {
        if (count_ == 0) return;
        head_ = (head_ + max_ - 1) % max_;
        count_--;
        const Snapshot& s = ring_[head_];
        for (size_t i = 0; i < particles.size(); ++i) {
            particles[i].position = s.positions[i];
            particles[i].velocity = s.velocities[i];
        }
    }

    void clear() {
        head_ = 0;
        count_ = 0;
    }

private:
    struct Snapshot {
        std::vector<glm::vec2> positions;
        std::vector<glm::vec2> velocities;
    };

    std::vector<Snapshot> ring_;
    size_t head_ = 0;
    size_t count_ = 0;
    size_t max_;
};
