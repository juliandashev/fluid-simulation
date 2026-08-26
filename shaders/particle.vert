#version 430 core

layout(location = 0) in vec2 position;
layout(location = 1) in float a_field;  // speed, or density when u_color_field = 1

uniform float u_point_size;
uniform float u_domain_half;
uniform float u_max_speed;
uniform float u_aspect;   // window width / height; corrects x for non-square windows
uniform int   u_color_field;  // 0 = speed, 1 = pressure
uniform float u_target_density;
uniform float u_pressure_multiplier;
uniform float u_max_pressure;

out float v_t;  // position in the colour ramp, 0 = blue, 1 = red

float speed_t(float speed) {
    float t = clamp(speed / u_max_speed, 0.0, 1.0);
    return pow(t, 1.5);   // exponent > 1 pushes mid-speeds toward blue
}

// Same Tait EOS as force.comp; EOS_EXPONENT is injected by shader.cc.
float pressure_of(float density) {
    float ratio = density / u_target_density;
    return max(0.0, u_pressure_multiplier * (pow(ratio, EOS_EXPONENT) - 1.0));
}

float pressure_t(float density) {
    float t = clamp(pressure_of(density) / u_max_pressure, 0.0, 1.0);
    return sqrt(t);   // exponent < 1 opens up the low end, where a Bernoulli drop lives
}

void main() {
    gl_Position = vec4(position.x / (u_domain_half * u_aspect), position.y / u_domain_half, 0.0, 1.0);
    gl_PointSize = u_point_size;
    v_t = u_color_field == 0 ? speed_t(a_field) : pressure_t(a_field);
}
