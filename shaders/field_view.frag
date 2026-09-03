#version 430 core

in vec2 v_uv;

out vec4 frag_color;

uniform sampler2D u_field;
uniform int   u_color_field;   // 1 = speed, 2 = pressure
uniform float u_max_speed;
uniform float u_min_pressure;
uniform float u_max_pressure;
uniform float u_coverage_min;  // below this the kernel support is too empty to trust

#include "ramp.glsl"

void main() {
    vec4 sampled = texture(u_field, v_uv);

    // Nothing there: solid geometry, or outside the fluid. Leave the background.
    if (sampled.z < u_coverage_min) {
        discard;
    }

    float t;

    if (u_color_field == 2) {
        float span = max(u_max_pressure - u_min_pressure, 1e-6);
        t = sqrt(clamp((sampled.y - u_min_pressure) / span, 0.0, 1.0));
    } else {
        t = pow(clamp(sampled.x / u_max_speed, 0.0, 1.0), 1.5);
    }

    frag_color = vec4(ramp_color(t), 1.0);
}
