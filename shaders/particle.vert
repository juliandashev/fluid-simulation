#version 430 core

layout(location = 0) in vec2 position;

uniform float u_point_size;
uniform float u_domain_half;

void main() {
    gl_Position = vec4(position / u_domain_half, 0.0, 1.0);
    gl_PointSize = u_point_size;
}
