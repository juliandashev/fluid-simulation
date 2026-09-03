#version 430 core

in float v_t;  // normalized field value in [0, 1]

out vec4 frag_color;

#include "ramp.glsl"

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    if (dot(coord, coord) > 0.25)
        discard;

    frag_color = vec4(ramp_color(v_t), 1.0);
}
