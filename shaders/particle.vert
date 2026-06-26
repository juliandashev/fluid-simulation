#version 430 core

layout(location = 0) in vec2 position;
layout(location = 1) in float a_speed;

uniform float u_point_size;
uniform float u_domain_half;
uniform float u_max_speed;
uniform float u_aspect;   // window width / height; corrects x for non-square windows

out float v_t;  // speed / SPEED_COLOR_MAX, gamma-biased toward blue

void main() {
    gl_Position = vec4(position.x / (u_domain_half * u_aspect), position.y / u_domain_half, 0.0, 1.0);
    gl_PointSize = u_point_size;
    float t = clamp(a_speed / u_max_speed, 0.0, 1.0);
    v_t = pow(t, 1.5);   // exponent > 1 pushes mid-speeds toward blue
}
