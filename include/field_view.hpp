#pragma once

#include <glad/gl.h>

#include "color_field.hpp"
#include "defs.hpp"
#include "shader.hpp"

namespace fluid {

class Simulation;

namespace gl {

// An Eulerian read-out of the Lagrangian state: the SPH interpolant evaluated on
// a fixed grid rather than at the particles. Purely a view - nothing here feeds
// back into the solver. See README.
class FieldView {
public:
    FieldView(int32_t width, int32_t height);
    ~FieldView();

    FieldView(const FieldView&) = delete;
    FieldView& operator=(const FieldView&) = delete;

    // Resamples from the current particle state. Call after step(), before draw().
    void update(const Simulation& sim, const SimParams& params);
    void draw(ColorField field, const SimParams& params) const;

private:
    int32_t width_;
    int32_t height_;
    GLuint texture_ = 0;
    GLuint vao_ = 0;
    Shader sample_;
    Shader present_;
};

}  // namespace gl
}  // namespace fluid
