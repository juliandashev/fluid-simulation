#pragma once

#include <cmath>

// Density field summary. Target: sigma/rho_0 ~ 1% (Monaghan 2005).
struct DensityStats {
    float_t min = 0.0f;
    float_t median = 0.0f;
    float_t p90 = 0.0f;
    float_t max = 0.0f;
    float_t sigma = 0.0f;
};

// Acceleration residual |a + g| summary. No sigma - the field is heavy-tailed.
struct AccelStats {
    float_t min = 0.0f;
    float_t median = 0.0f;
    float_t p90 = 0.0f;
    float_t max = 0.0f;
};
