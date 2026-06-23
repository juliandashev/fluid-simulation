#version 430 core

in vec2 v_world;

out vec4 frag_color;

const int MAX_PARTICLES = 512;

uniform vec2 u_positions[MAX_PARTICLES];
uniform float u_properties[MAX_PARTICLES];
uniform float u_densities[MAX_PARTICLES];
uniform int u_count;
uniform float u_radius;
uniform float u_mass;
uniform float u_target;
uniform float u_scale;

// Same poly6-style kernel as the CPU side (volume = PI * radius^8 / 4).
float smoothing_kernel(float radius, float dist) {
    if (dist >= radius)
        return 0.0;
    float volume = 3.14159265 * pow(radius, 8.0) / 4.0;
    float v = radius * radius - dist * dist;
    return (v * v * v) / volume;
}

void main() {
    float density = 0.0;
    for (int i = 0; i < u_count; i++) {
        density += u_mass * smoothing_kernel(u_radius, distance(u_positions[i], v_world));
    }

    // density < target -> light blue, = target -> white, > target -> light red
    float t = clamp((density - u_target) * u_scale, -1.0, 1.0);

    vec3 white = vec3(1.0);
    vec3 blue = vec3(0.5, 0.7, 1.0);
    vec3 red = vec3(1.0, 0.5, 0.5);

    vec3 color = (t < 0.0) ? mix(white, blue, -t) : mix(white, red, t);
    frag_color = vec4(color, 1.0);
}
