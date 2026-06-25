#version 430 core

in float v_t;  // normalized speed in [0, 1]

out vec4 frag_color;

// Maps speed (0 = slow, 1 = fast) through a blue -> green -> yellow -> red ramp.
// Four stops, three equal segments; mix() lerps between the two stops of the
// segment t lands in, after remapping t to [0, 1] within that segment.
vec3 speed_color(float t) {
    vec3 blue   = vec3(0.3, 0.6, 1.0);
    vec3 green  = vec3(0.2, 0.9, 0.3);
    vec3 yellow = vec3(1.0, 0.9, 0.2);
    vec3 red    = vec3(1.0, 0.2, 0.15);

    if (t < 0.3333)
        return mix(blue, green, t / 0.3333);
    else if (t < 0.6666)
        return mix(green, yellow, (t - 0.3333) / 0.3333);
    return mix(yellow, red, (t - 0.6666) / 0.3334);
}

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    if (dot(coord, coord) > 0.25)
        discard;

    frag_color = vec4(speed_color(v_t), 1.0);
}
