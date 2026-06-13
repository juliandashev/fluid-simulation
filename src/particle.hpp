#pragma once
#include <glm/vec2.hpp>

struct Particle {
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec2 force;

    float density;
    float pressure;
};
