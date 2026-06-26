#version 430 core

layout(location = 0) in vec2 position;

uniform float u_domain_half;
uniform float u_aspect;   // window width / height; corrects x for non-square windows

void main() {
    gl_Position = vec4(position.x / (u_domain_half * u_aspect), position.y / u_domain_half, 0.0, 1.0);
}
