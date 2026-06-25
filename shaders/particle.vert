#version 430 core

layout(location = 0) in vec2 position;
layout(location = 1) in float a_speed;

uniform float u_point_size;
uniform float u_domain_half;
uniform float u_max_speed;

out float v_t;  // normalized speed in [0, 1]: 0 = slowest, 1 = fastest this frame

void main() {
    gl_Position = vec4(position / u_domain_half, 0.0, 1.0);
    gl_PointSize = u_point_size;
    v_t = clamp(a_speed / u_max_speed, 0.0, 1.0);
}
