#pragma once
#include <glm/vec2.hpp>
#include <cmath>

struct Particle {
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec2 force;

    float_t density;
    float_t pressure;
    float_t property;
};
