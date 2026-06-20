#version 430 core

layout(location = 0) in vec2 position;  // fullscreen quad corners in NDC

out vec2 v_world;
uniform float u_domain_half;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    v_world = position * u_domain_half;
}
