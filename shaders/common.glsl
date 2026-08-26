// Spliced into density.comp and force.comp by shader.cc. Holds what the two
// stages must agree on exactly; force-only declarations stay in force.comp.

const float PI = 3.14159265358979;

// ---------------------------------------------------------------- storage ---

layout(std430, binding = 0) buffer Positions     { vec2 positions[]; };
layout(std430, binding = 2) buffer Densities     { vec2 densities[]; };  // x = density, y unused
layout(std430, binding = 4) buffer CellCounts    { uint cell_counts[]; };
layout(std430, binding = 5) buffer CellStarts    { uint cell_starts[]; };
layout(std430, binding = 7) buffer SortedIndices { uint sorted_indices[]; };

// Static boundary particles, filling the kernel support a solid would truncate.
// Own grid, built once at spawn; mirror images cannot express a shape.
layout(std430, binding = 11) buffer SolidPositions     { vec2 solid_positions[]; };
layout(std430, binding = 12) buffer SolidCellCounts    { uint solid_cell_counts[]; };
layout(std430, binding = 13) buffer SolidCellStarts    { uint solid_cell_starts[]; };
layout(std430, binding = 14) buffer SolidSortedIndices { uint solid_sorted_indices[]; };

// --------------------------------------------------------------- uniforms ---

uniform uint u_num_solids;

uniform uint  u_num_particles;
uniform float u_kernel_radius;     // h
uniform float u_kernel_radius_sq;  // h^2
uniform float u_mass;
uniform vec2  u_grid_origin;
uniform float u_cell_size;
uniform int   u_grid_w;
uniform int   u_grid_h;

uniform float u_domain_half_x;
uniform float u_domain_min;
uniform float u_domain_max;

// Mirror plane offset outside each face; keeps a resting particle's self-image
// off the r = 0 dead zone of dW/dr. See README.
uniform float u_wall_offset;

// > 0 means x wraps with this period: no side walls, and the cell walk carries
// neighbours across the seam. The grid tiles it exactly - see PERIOD_X.
uniform float u_period_x;

// ------------------------------------------------------------- the kernel ---

// Wendland C2, 2D: W(s) = (7/(pi R^2))(1-s)^4(4s+1), s = r/R.
float smoothing_kernel(float radius_sq, float distance_sq) {
    if (distance_sq >= radius_sq) {
        return 0.0;
    }
    float radius = sqrt(radius_sq);
    float s = sqrt(distance_sq) / radius;
    float t = 1.0 - s;
    float t2 = t * t;
    return (7.0 / (PI * radius_sq)) * t2 * t2 * (4.0 * s + 1.0);
}

// dW/dr = -(140/(pi R^3)) s (1-s)^3, the actual derivative of the kernel above.
float smoothing_kernel_derivative(float radius, float distance) {
    if (distance >= radius) {
        return 0.0f;
    }

    float s = distance / radius;
    float t = 1.0 - s;
    float scale = PI * radius * radius * radius;

    return -140.0f * s * t * t * t / scale;
}

// ------------------------------------------------------------ wall images ---

// Mirror images restoring the kernel support truncated by a wall. Affine map
// p*scale + offset; scale alone maps a velocity. Corners get a third image.
struct WallImages {
    int count;
    vec2 scale[3];
    vec2 offset[3];
};

// Resolves one cell of the 3x3 walk. Returns false when the cell is outside the
// grid; otherwise `shift` is what moves a wrapped neighbour next to the querier.
bool resolve_cell(inout ivec2 cell, out float shift) {
    shift = 0.0;

    if (cell.y < 0 || cell.y >= u_grid_h) {
        return false;
    }

    if (u_period_x > 0.0) {
        if (cell.x < 0) {
            cell.x += u_grid_w;
            shift = -u_period_x;
        } else if (cell.x >= u_grid_w) {
            cell.x -= u_grid_w;
            shift = u_period_x;
        }
        return true;
    }

    return cell.x >= 0 && cell.x < u_grid_w;
}

WallImages wall_images(vec2 p) {
    WallImages im;
    im.count = 0;

    bool have_x = false;
    float bx = 0.0;
    if (u_period_x > 0.0) {
        have_x = false;  // wrapped, so there is no side wall to mirror against
    } else if (p.x + u_domain_half_x < u_kernel_radius) {
        have_x = true; bx = -u_domain_half_x - u_wall_offset;
    } else if (u_domain_half_x - p.x < u_kernel_radius) {
        have_x = true; bx = u_domain_half_x + u_wall_offset;
    }

    bool have_y = false;
    float by = 0.0;
    if (p.y - u_domain_min < u_kernel_radius) {
        have_y = true; by = u_domain_min - u_wall_offset;
    } else if (u_domain_max - p.y < u_kernel_radius) {
        have_y = true; by = u_domain_max + u_wall_offset;
    }

    if (have_x) {
        im.scale[im.count] = vec2(-1.0, 1.0);
        im.offset[im.count] = vec2(2.0 * bx, 0.0);
        im.count++;
    }
    if (have_y) {
        im.scale[im.count] = vec2(1.0, -1.0);
        im.offset[im.count] = vec2(0.0, 2.0 * by);
        im.count++;
    }
    if (have_x && have_y) {
        im.scale[im.count] = vec2(-1.0, -1.0);
        im.offset[im.count] = vec2(2.0 * bx, 2.0 * by);
        im.count++;
    }

    return im;
}
