#version 430 core

out vec4 frag_color;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    if (dot(coord, coord) > 0.25)
        discard;

    frag_color = vec4(0.3, 0.7, 1.0, 1.0);
}
